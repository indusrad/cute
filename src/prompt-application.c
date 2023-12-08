/* prompt-application.c
 *
 * Copyright 2023 Christian Hergert
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include <glib/gi18n.h>

#include <sys/wait.h>

#include "prompt-action-group.h"
#include "prompt-application.h"
#include "prompt-client.h"
#include "prompt-container-menu.h"
#include "prompt-preferences-window.h"
#include "prompt-profile-menu.h"
#include "prompt-session.h"
#include "prompt-settings.h"
#include "prompt-util.h"
#include "prompt-window.h"

#define PORTAL_BUS_NAME "org.freedesktop.portal.Desktop"
#define PORTAL_OBJECT_PATH "/org/freedesktop/portal/desktop"
#define PORTAL_SETTINGS_INTERFACE "org.freedesktop.portal.Settings"

struct _PromptApplication
{
  AdwApplication       parent_instance;
  GListStore          *profiles;
  PromptSettings      *settings;
  PromptShortcuts     *shortcuts;
  PromptContainerMenu *container_menu;
  PromptProfileMenu   *profile_menu;
  char                *system_font_name;
  GDBusProxy          *portal;
  PromptClient        *client;
  GVariant            *session;
};

static void prompt_application_about             (GSimpleAction *action,
                                                  GVariant      *param,
                                                  gpointer       user_data);
static void prompt_application_edit_profile      (GSimpleAction *action,
                                                  GVariant      *param,
                                                  gpointer       user_data);
static void prompt_application_help_overlay      (GSimpleAction *action,
                                                  GVariant      *param,
                                                  gpointer       user_data);
static void prompt_application_new_window_action (GSimpleAction *action,
                                                  GVariant      *param,
                                                  gpointer       user_data);
static void prompt_application_preferences       (GSimpleAction *action,
                                                  GVariant      *param,
                                                  gpointer       user_data);
static void prompt_application_focus_tab_by_uuid (GSimpleAction *action,
                                                  GVariant      *param,
                                                  gpointer       user_data);

static GActionEntry action_entries[] = {
  { "about", prompt_application_about },
  { "edit-profile", prompt_application_edit_profile, "s" },
  { "help-overlay", prompt_application_help_overlay },
  { "preferences", prompt_application_preferences },
  { "focus-tab-by-uuid", prompt_application_focus_tab_by_uuid, "s" },
  { "new-window", prompt_application_new_window_action },
};

G_DEFINE_FINAL_TYPE (PromptApplication, prompt_application, ADW_TYPE_APPLICATION)

enum {
  PROP_0,
  PROP_DEFAULT_PROFILE,
  PROP_OS_NAME,
  PROP_SYSTEM_FONT_NAME,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

PromptApplication *
prompt_application_new (const char        *application_id,
                        GApplicationFlags  flags)
{
  g_return_val_if_fail (application_id != NULL, NULL);

  return g_object_new (PROMPT_TYPE_APPLICATION,
                       "application-id", application_id,
                       "flags", flags,
                       NULL);
}

static inline GFile *
get_session_file (void)
{
  return g_file_new_build_filename (g_get_user_config_dir (),
                                    APP_ID,
                                    "session.gvariant",
                                    NULL);
}

static void
prompt_application_activate (GApplication *app)
{
  PromptApplication *self = (PromptApplication *)app;

  g_assert (PROMPT_IS_APPLICATION (self));
  g_assert (PROMPT_IS_CLIENT (self->client));

  for (const GList *windows = gtk_application_get_windows (GTK_APPLICATION (app));
       windows != NULL;
       windows = windows->next)
    {
      if (PROMPT_IS_WINDOW (windows->data))
        {
          gtk_window_present (windows->data);
          return;
        }
    }

  if (self->session == NULL ||
      !prompt_session_restore (self, self->session))
    {
      PromptWindow *window = prompt_window_new ();
      gtk_window_present (GTK_WINDOW (window));
    }
}

static int
prompt_application_command_line (GApplication            *app,
                                 GApplicationCommandLine *cmdline)
{
  PromptApplication *self = (PromptApplication *)app;
  g_autofree char *command = NULL;
  g_auto(GStrv) argv = NULL;
  GVariantDict *dict;
  int argc;

  g_assert (PROMPT_IS_APPLICATION (self));
  g_assert (G_IS_APPLICATION_COMMAND_LINE (cmdline));

  dict = g_application_command_line_get_options_dict (cmdline);

  argv = g_application_command_line_get_arguments (cmdline, &argc);

  if (g_variant_dict_contains (dict, "preferences"))
    g_action_group_activate_action (G_ACTION_GROUP (self), "preferences", NULL);
  else if (g_variant_dict_contains (dict, "execute") &&
           g_variant_dict_lookup (dict, "execute", "s", &command))
    {
      const char *cwd = g_application_command_line_get_cwd (cmdline);
      g_autoptr(GError) error = NULL;
      g_autofree char *cwd_uri = NULL;
      PromptWindow *window;

      if (!g_shell_parse_argv (command, &argc, &argv, &error))
        {
          g_application_command_line_printerr (cmdline,
                                               _("Cannot parse command: %s"),
                                               error->message);
          return EXIT_FAILURE;
        }

      if (cwd != NULL && g_path_is_absolute (cwd))
        cwd_uri = g_strdup_printf ("file://%s", cwd);

      window = prompt_window_new_for_command ((const char * const *)argv, cwd_uri);
      gtk_application_add_window (GTK_APPLICATION (self), GTK_WINDOW (window));
      gtk_window_present (GTK_WINDOW (window));
    }
  else if (g_variant_dict_contains (dict, "new-window"))
    prompt_application_new_window_action (NULL, NULL, self);
  else
    g_application_activate (G_APPLICATION (self));

  return G_APPLICATION_CLASS (prompt_application_parent_class)->command_line (app, cmdline);
}

static void
on_portal_settings_changed_cb (PromptApplication *self,
                               const char        *sender_name,
                               const char        *signal_name,
                               GVariant          *parameters,
                               gpointer           user_data)
{
  g_autoptr(GVariant) value = NULL;
  const char *schema_id;
  const char *key;

  g_assert (PROMPT_IS_APPLICATION (self));
  g_assert (sender_name != NULL);
  g_assert (signal_name != NULL);

  if (g_strcmp0 (signal_name, "SettingChanged") != 0)
    return;

  g_variant_get (parameters, "(&s&sv)", &schema_id, &key, &value);

  if (g_strcmp0 (schema_id, "org.gnome.desktop.interface") == 0 &&
      g_strcmp0 (key, "monospace-font-name") == 0 &&
      g_strcmp0 (g_variant_get_string (value, NULL), "") != 0)
    {
      if (g_set_str (&self->system_font_name, g_variant_get_string (value, NULL)))
        g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SYSTEM_FONT_NAME]);
    }
}

static void
parse_portal_settings (PromptApplication *self,
                       GVariant          *parameters)
{
  GVariantIter *iter = NULL;
  const char *schema_str;
  GVariant *val;

  g_assert (PROMPT_IS_APPLICATION (self));

  if (parameters == NULL)
    return;

  g_variant_get (parameters, "(a{sa{sv}})", &iter);

  while (g_variant_iter_loop (iter, "{s@a{sv}}", &schema_str, &val))
    {
      GVariantIter *iter2 = g_variant_iter_new (val);
      const char *key;
      GVariant *v;

      while (g_variant_iter_loop (iter2, "{sv}", &key, &v))
        {
          if (g_strcmp0 (schema_str, "org.gnome.desktop.interface") == 0 &&
              g_strcmp0 (key, "monospace-font-name") == 0 &&
              g_strcmp0 (g_variant_get_string (v, NULL), "") != 0)
            g_set_str (&self->system_font_name, g_variant_get_string (v, NULL));
        }

      g_variant_iter_free (iter2);
    }

  g_variant_iter_free (iter);
}

static void
prompt_application_notify_default_profile_uuid_cb (PromptApplication *self,
                                                   GParamSpec        *pspec,
                                                   PromptSettings    *settings)
{
  g_assert (PROMPT_IS_APPLICATION (self));
  g_assert (PROMPT_IS_SETTINGS (settings));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DEFAULT_PROFILE]);
}

static void
prompt_application_notify_profile_uuids_cb (PromptApplication *self,
                                            GParamSpec        *pspec,
                                            PromptSettings    *settings)
{
  g_auto(GStrv) profile_uuids = NULL;
  g_autoptr(GPtrArray) array = NULL;
  guint n_items;

  g_assert (PROMPT_IS_APPLICATION (self));
  g_assert (PROMPT_IS_SETTINGS (settings));

  profile_uuids = prompt_settings_dup_profile_uuids (settings);
  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->profiles));
  array = g_ptr_array_new_with_free_func (g_object_unref);

  for (guint i = 0; profile_uuids[i]; i++)
    g_ptr_array_add (array, prompt_profile_new (profile_uuids[i]));

  g_list_store_splice (self->profiles, 0, n_items, array->pdata, array->len);
}

static gboolean
filter_session_container (gpointer item,
                          gpointer user_data)
{
  return g_strcmp0 ("session", prompt_ipc_container_get_id (item)) != 0;
}

static void
prompt_application_startup (GApplication *application)
{
  static const char *patterns[] = { "org.gnome.*", NULL };
  PromptApplication *self = (PromptApplication *)application;
  g_autoptr(PromptIpcContainer) host = NULL;
  g_autoptr(GtkFilterListModel) filter_model = NULL;
  g_autoptr(GtkCustomFilter) filter = NULL;
  g_autoptr(GFile) session_file = get_session_file ();
  g_autoptr(GBytes) session_bytes = NULL;
  g_autoptr(GError) error = NULL;
  AdwStyleManager *style_manager;

  g_assert (PROMPT_IS_APPLICATION (self));

  g_application_set_default (application);
  g_application_set_resource_base_path (G_APPLICATION (self), "/org/gnome/Prompt");

  self->profiles = g_list_store_new (PROMPT_TYPE_PROFILE);
  self->settings = prompt_settings_new ();
  self->shortcuts = prompt_shortcuts_new (NULL);

  /* Load the session state so it's available if we need it */
  if ((session_bytes = g_file_load_bytes (session_file, NULL, NULL, NULL)))
    {
      g_autoptr(GVariant) variant = g_variant_new_from_bytes (G_VARIANT_TYPE_VARDICT, session_bytes, FALSE);

      if (variant != NULL)
        self->session = g_variant_take_ref (g_steal_pointer (&variant));
    }

  G_APPLICATION_CLASS (prompt_application_parent_class)->startup (application);

  if (!(self->client = prompt_client_new (&error)))
    g_error ("Failed to launch prompt-agent: %s", error->message);

  self->profile_menu = prompt_profile_menu_new (self->settings);

  filter = gtk_custom_filter_new (filter_session_container, NULL, NULL);
  filter_model = gtk_filter_list_model_new (g_object_ref (G_LIST_MODEL (self->client)),
                                            g_object_ref (GTK_FILTER (filter)));
  self->container_menu = prompt_container_menu_new (G_LIST_MODEL (filter_model));

  g_action_map_add_action_entries (G_ACTION_MAP (self),
                                   action_entries,
                                   G_N_ELEMENTS (action_entries),
                                   self);

  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "app.help-overlay",
                                         (const char * const []) {"<ctrl>question", NULL});

  /* Setup portal to get settings */
  self->portal = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                G_DBUS_PROXY_FLAGS_NONE,
                                                NULL,
                                                PORTAL_BUS_NAME,
                                                PORTAL_OBJECT_PATH,
                                                PORTAL_SETTINGS_INTERFACE,
                                                NULL,
                                                NULL);

  if (self->portal != NULL)
    {
      g_autoptr(GVariant) all = NULL;

      g_signal_connect_object (self->portal,
                               "g-signal",
                               G_CALLBACK (on_portal_settings_changed_cb),
                               self,
                               G_CONNECT_SWAPPED);
      all = g_dbus_proxy_call_sync (self->portal,
                                    "ReadAll",
                                    g_variant_new ("(^as)", patterns),
                                    G_DBUS_CALL_FLAGS_NONE,
                                    G_MAXINT,
                                    NULL,
                                    NULL);
      parse_portal_settings (self, all);
    }

  g_signal_connect_object (self->settings,
                           "notify::profile-uuids",
                           G_CALLBACK (prompt_application_notify_profile_uuids_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->settings,
                           "notify::default-profile-uuid",
                           G_CALLBACK (prompt_application_notify_default_profile_uuid_cb),
                           self,
                           G_CONNECT_SWAPPED);

  prompt_application_notify_profile_uuids_cb (self, NULL, self->settings);

  style_manager = adw_style_manager_get_default ();

  g_object_bind_property (self->settings, "interface-style",
                          style_manager, "color-scheme",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
}

static void
prompt_application_shutdown (GApplication *application)
{
  PromptApplication *self = (PromptApplication *)application;

  g_assert (PROMPT_IS_APPLICATION (self));

  G_APPLICATION_CLASS (prompt_application_parent_class)->shutdown (application);

  g_clear_object (&self->profile_menu);
  g_clear_object (&self->profiles);
  g_clear_object (&self->portal);
  g_clear_object (&self->shortcuts);
  g_clear_object (&self->settings);
  g_clear_object (&self->client);

  g_clear_pointer (&self->system_font_name, g_free);
}

static void
prompt_application_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  PromptApplication *self = PROMPT_APPLICATION (object);

  switch (prop_id)
    {
    case PROP_DEFAULT_PROFILE:
      g_value_take_object (value, prompt_application_dup_default_profile (self));
      break;

    case PROP_OS_NAME:
      g_value_set_string (value, prompt_application_get_os_name (self));
      break;

    case PROP_SYSTEM_FONT_NAME:
      g_value_set_string (value, self->system_font_name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_application_class_init (PromptApplicationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

  object_class->get_property = prompt_application_get_property;

  app_class->activate = prompt_application_activate;
  app_class->startup = prompt_application_startup;
  app_class->shutdown = prompt_application_shutdown;
  app_class->command_line = prompt_application_command_line;

  properties[PROP_DEFAULT_PROFILE] =
    g_param_spec_object ("default-profile", NULL, NULL,
                         PROMPT_TYPE_PROFILE,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_OS_NAME] =
    g_param_spec_string ("os-name", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_SYSTEM_FONT_NAME] =
    g_param_spec_string ("system-font-name",
                         "System Font Name",
                         "System Font Name",
                         "Monospace 11",
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
prompt_application_init (PromptApplication *self)
{
  g_autoptr(GString) summary = g_string_new (_("Examples:"));
  static const GOptionEntry main_entries[] = {
    { "new-window", 'n', 0, G_OPTION_ARG_NONE, NULL, N_("New terminal window") },
    { "preferences", 0, 0, G_OPTION_ARG_NONE, NULL, N_("Show the application preferences") },
    { "execute", 'x', 0, G_OPTION_ARG_STRING, NULL, N_("Command to execute in new window") },
    { NULL }
  };

  g_string_append_c (summary, '\n');
  g_string_append_c (summary, '\n');
  g_string_append_printf (summary, "  %s\n", _("Run Separate Instance"));
  g_string_append (summary, "    prompt -s\n");

  g_string_append_c (summary, '\n');
  g_string_append_printf (summary, "  %s\n", _("Open Preferences"));
  g_string_append (summary, "    prompt --preferences\n");

  g_string_append_c (summary, '\n');
  g_string_append_printf (summary, "  %s\n", _("Run Custom Command in New Window"));
  g_string_append (summary, "    prompt -x \"bash -c 'sleep 3'\"\n");
  g_string_append (summary, "    prompt -- bash -c 'sleep 3'");

  g_application_set_option_context_parameter_string (G_APPLICATION (self), _("[-- COMMAND ARGUMENTS]"));
  g_application_add_main_option_entries (G_APPLICATION (self), main_entries);
  g_application_set_option_context_summary (G_APPLICATION (self), summary->str);

  self->system_font_name = g_strdup ("Monospace 11");
}

static void
prompt_application_edit_profile (GSimpleAction *action,
                                 GVariant      *param,
                                 gpointer       user_data)
{
  PromptApplication *self = user_data;
  g_autoptr(PromptProfile) profile = NULL;
  const char *profile_uuid;

  g_assert (PROMPT_IS_APPLICATION (self));
  g_assert (param != NULL);
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));

  profile_uuid = g_variant_get_string (param, NULL);

  if ((prompt_application_dup_profile (self, profile_uuid)))
    {
      PromptPreferencesWindow *window = prompt_preferences_window_get_default ();

      prompt_preferences_window_edit_profile (window, profile);
    }
}

static char *
generate_debug_info (PromptApplication *self)
{
  GString *str = g_string_new (NULL);
  g_autoptr(GListModel) containers = NULL;
  g_autofree char *flatpak_info = NULL;
  const char *os_name = prompt_application_get_os_name (self);
  guint n_items;

  g_string_append_printf (str, "Host: %s\n", os_name);
  g_string_append_c (str, '\n');

  g_string_append_printf (str,
                          "GLib: %d.%d.%d (compiled against %d.%d.%d)\n",
                          glib_major_version,
                          glib_minor_version,
                          glib_micro_version,
                          GLIB_MAJOR_VERSION,
                          GLIB_MINOR_VERSION,
                          GLIB_MICRO_VERSION);
  g_string_append_printf (str,
                          "GTK: %d.%d.%d (compiled against %d.%d.%d)\n",
                          gtk_get_major_version (),
                          gtk_get_minor_version (),
                          gtk_get_micro_version (),
                          GTK_MAJOR_VERSION,
                          GTK_MINOR_VERSION,
                          GTK_MICRO_VERSION);
  g_string_append_printf (str,
                          "VTE: %d.%d.%d (compiled against %d.%d.%d) %s\n",
                          vte_get_major_version (),
                          vte_get_minor_version (),
                          vte_get_micro_version (),
                          VTE_MAJOR_VERSION,
                          VTE_MINOR_VERSION,
                          VTE_MICRO_VERSION,
                          vte_get_features ());

#if DEVELOPMENT_BUILD
  g_string_append_c (str, '\n');
  g_string_append (str, "** DEVELOPMENT BUILD **\n");
#endif

  if (strstr (APP_ID, "Devel") != NULL)
    {
      g_string_append_c (str, '\n');
      g_string_append_printf (str, "App ID: %s\n", APP_ID);
    }


  g_string_append_c (str, '\n');
  g_string_append (str, "Containers:\n");

  containers = prompt_application_list_containers (self);
  n_items = g_list_model_get_n_items (containers);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(PromptIpcContainer) container = g_list_model_get_item (containers, i);

      if (g_strcmp0 ("session", prompt_ipc_container_get_id (container)) == 0)
        continue;

      g_string_append_printf (str,
                              "  • %s (%s)\n",
                              prompt_ipc_container_get_display_name (container),
                              prompt_ipc_container_get_provider (container));
    }

  if (g_file_get_contents ("/.flatpak-info", &flatpak_info, NULL, NULL))
    {
      g_string_append_c (str, '\n');
      g_string_append (str, flatpak_info);
    }

  return g_string_free (str, FALSE);
}

static void
prompt_application_about (GSimpleAction *action,
                          GVariant      *param,
                          gpointer       user_data)
{
  static const char *developers[] = {"Christian Hergert", NULL};
  static const char *artists[] = {"Jakub Steiner", NULL};
  PromptApplication *self = user_data;
  GtkWindow *window = NULL;
  g_autofree char *debug_info = NULL;

  g_assert (PROMPT_IS_APPLICATION (self));

  window = gtk_application_get_active_window (GTK_APPLICATION (self));
  debug_info = generate_debug_info (self);

  adw_show_about_window (window,
                         "application-icon", PACKAGE_ICON_NAME,
                         "application-name", PACKAGE_NAME,
                         "artists", artists,
                         "copyright", "© 2023 Red Hat, Inc.",
                         "debug-info", debug_info,
                         "developer-name", "Christian Hergert",
                         "developers", developers,
                         "issue-url", "https://gitlab.gnome.org/chergert/prompt/issues",
                         "license-type", GTK_LICENSE_GPL_3_0,
                         "translator-credits", _("translator-credits"),
                         "version", PACKAGE_VERSION,
                         "website", "https://gitlab.gnome.org/chergert/prompt",
                         NULL);
}

static void
prompt_application_preferences (GSimpleAction *action,
                                GVariant      *param,
                                gpointer       user_data)
{
  PromptPreferencesWindow *window;
  PromptApplication *self = user_data;

  g_assert (PROMPT_IS_APPLICATION (self));

  window = prompt_preferences_window_get_default ();
  gtk_application_add_window (GTK_APPLICATION (self), GTK_WINDOW (window));
  gtk_window_present (GTK_WINDOW (window));
}

static void
prompt_application_new_window_action (GSimpleAction *action,
                                      GVariant      *param,
                                      gpointer       user_data)
{
  PromptApplication *self = user_data;
  PromptWindow *window;

  g_assert (!action || G_IS_SIMPLE_ACTION (action));
  g_assert (PROMPT_IS_APPLICATION (self));

  window = prompt_window_new ();
  gtk_application_add_window (GTK_APPLICATION (self), GTK_WINDOW (window));
  gtk_window_present (GTK_WINDOW (window));
}

static void
prompt_application_help_overlay (GSimpleAction *action,
                                 GVariant      *param,
                                 gpointer       user_data)
{
  PromptPreferencesWindow *window;
  PromptApplication *self = user_data;

  g_assert (PROMPT_IS_APPLICATION (self));

  window = prompt_preferences_window_get_default ();
  prompt_preferences_window_edit_shortcuts (window);
  gtk_window_present (GTK_WINDOW (window));
}

/**
 * prompt_application_list_profiles:
 * @self: a #PromptApplication
 *
 * Gets a #GListModel or profiles that are available to the application.
 *
 * The resulting #GListModel will update as profiles are created or deleted.
 *
 * Returns: (transfer full) (not nullable): a #GListModel
 */
GListModel *
prompt_application_list_profiles (PromptApplication *self)
{
  g_return_val_if_fail (PROMPT_IS_APPLICATION (self), NULL);

  return g_object_ref (G_LIST_MODEL (self->profiles));
}

/**
 * prompt_application_dup_default_profile:
 * @self: a #PromptApplication
 *
 * Gets the default profile for the application.
 *
 * Returns: (transfer full): a #PromptProfile
 */
PromptProfile *
prompt_application_dup_default_profile (PromptApplication *self)
{
  g_autoptr(PromptProfile) new_profile = NULL;
  g_autoptr(GListModel) profiles = NULL;
  g_autofree char *default_profile_uuid = NULL;
  guint n_items;

  g_return_val_if_fail (PROMPT_IS_APPLICATION (self), NULL);

  default_profile_uuid = prompt_settings_dup_default_profile_uuid (self->settings);
  profiles = prompt_application_list_profiles (self);
  n_items = g_list_model_get_n_items (profiles);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(PromptProfile) profile = g_list_model_get_item (profiles, i);
      const char *profile_uuid = prompt_profile_get_uuid (profile);

      if (g_strcmp0 (profile_uuid, default_profile_uuid) == 0)
        return g_steal_pointer (&profile);
    }

  if (n_items > 0)
    return g_list_model_get_item (profiles, 0);

  new_profile = prompt_profile_new (NULL);

  g_assert (new_profile != NULL);
  g_assert (PROMPT_IS_PROFILE (new_profile));
  g_assert (prompt_profile_get_uuid (new_profile) != NULL);

  prompt_application_add_profile (self, new_profile);
  prompt_application_set_default_profile (self, new_profile);

  return g_steal_pointer (&new_profile);
}

void
prompt_application_set_default_profile (PromptApplication *self,
                                        PromptProfile     *profile)
{
  g_return_if_fail (PROMPT_IS_APPLICATION (self));
  g_return_if_fail (PROMPT_IS_PROFILE (profile));

  prompt_settings_set_default_profile_uuid (self->settings,
                                            prompt_profile_get_uuid (profile));
}

void
prompt_application_add_profile (PromptApplication *self,
                                PromptProfile     *profile)
{
  g_return_if_fail (PROMPT_IS_APPLICATION (self));

  prompt_settings_add_profile_uuid (self->settings,
                                    prompt_profile_get_uuid (profile));
}

void
prompt_application_remove_profile (PromptApplication *self,
                                   PromptProfile     *profile)
{
  g_return_if_fail (PROMPT_IS_APPLICATION (self));
  g_return_if_fail (PROMPT_IS_PROFILE (profile));

  prompt_settings_remove_profile_uuid (self->settings,
                                       prompt_profile_get_uuid (profile));
}

PromptProfile *
prompt_application_dup_profile (PromptApplication *self,
                                const char        *profile_uuid)
{
  g_autoptr(GListModel) model = NULL;
  guint n_items;

  g_return_val_if_fail (PROMPT_IS_APPLICATION (self), NULL);

  if (prompt_str_empty0 (profile_uuid))
    return prompt_application_dup_default_profile (self);

  model = prompt_application_list_profiles (self);
  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(PromptProfile) profile = g_list_model_get_item (model, i);

      if (g_strcmp0 (profile_uuid, prompt_profile_get_uuid (profile)) == 0)
        return g_steal_pointer (&profile);
    }

  return prompt_profile_new (profile_uuid);
}

gboolean
prompt_application_control_is_pressed (PromptApplication *self)
{
  GdkDisplay *display = gdk_display_get_default ();
  GdkSeat *seat = gdk_display_get_default_seat (display);
  GdkDevice *keyboard = gdk_seat_get_keyboard (seat);
  GdkModifierType modifiers = gdk_device_get_modifier_state (keyboard) & gtk_accelerator_get_default_mod_mask ();

  return !!(modifiers & GDK_CONTROL_MASK);
}

const char *
prompt_application_get_system_font_name (PromptApplication *self)
{
  g_return_val_if_fail (PROMPT_IS_APPLICATION (self), NULL);

  return self->system_font_name;
}

GMenuModel *
prompt_application_dup_profile_menu (PromptApplication *self)
{
  g_return_val_if_fail (PROMPT_IS_APPLICATION (self), NULL);

  return g_object_ref (G_MENU_MODEL (self->profile_menu));
}

GMenuModel *
prompt_application_dup_container_menu (PromptApplication *self)
{
  g_return_val_if_fail (PROMPT_IS_APPLICATION (self), NULL);

  return g_object_ref (G_MENU_MODEL (self->container_menu));
}

/**
 * prompt_application_list_containers:
 * @self: a #PromptApplication
 *
 * Gets a #GListModel of #PromptIpcContainer.
 *
 * Returns: (transfer full) (not nullable): a #GListModel
 */
GListModel *
prompt_application_list_containers (PromptApplication *self)
{
  g_return_val_if_fail (PROMPT_IS_APPLICATION (self), NULL);

  return g_object_ref (G_LIST_MODEL (self->client));
}

PromptIpcContainer *
prompt_application_lookup_container (PromptApplication *self,
                                     const char        *container_id)
{
  g_autoptr(GListModel) model = NULL;
  guint n_items;

  g_return_val_if_fail (PROMPT_IS_APPLICATION (self), NULL);

  if (prompt_str_empty0 (container_id))
    return NULL;

  model = prompt_application_list_containers (self);
  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(PromptIpcContainer) container = g_list_model_get_item (model, i);
      const char *id = prompt_ipc_container_get_id (container);

      if (g_strcmp0 (id, container_id) == 0)
        return g_steal_pointer (&container);
    }

  return NULL;
}

PromptSettings *
prompt_application_get_settings (PromptApplication *self)
{
  g_return_val_if_fail (PROMPT_IS_APPLICATION (self), NULL);

  return self->settings;
}

/**
 * prompt_application_get_shortcuts:
 * @self: a #PromptApplication
 *
 * Gets the shortcuts for the application.
 *
 * Returns: (transfer none) (not nullable): a #PromptShortcuts
 */
PromptShortcuts *
prompt_application_get_shortcuts (PromptApplication *self)
{
  g_return_val_if_fail (PROMPT_IS_APPLICATION (self), NULL);

  return self->shortcuts;
}

void
prompt_application_report_error (PromptApplication *self,
                                 GType              subsystem,
                                 const GError      *error)
{
  g_return_if_fail (PROMPT_IS_APPLICATION (self));

  /* TODO: We probably want to provide some feedback if we fail to
   *       run in a number of scenarios. And this will also let us
   *       do some deduplication of repeated error messages once
   *       we start showing errors to users.
   */

  g_debug ("%s: %s[%u]: %s",
           g_type_name (subsystem),
           g_quark_to_string (error->domain),
           error->code,
           error->message);
}

VtePty *
prompt_application_create_pty (PromptApplication  *self,
                               GError            **error)
{
  g_return_val_if_fail (PROMPT_IS_APPLICATION (self), NULL);

  return prompt_client_create_pty (self->client, error);
}

typedef struct _Spawn
{
  PromptIpcContainer *container;
  PromptProfile *profile;
  char *last_working_directory_uri;
  VtePty *pty;
  char **argv;
} Spawn;

static void
spawn_free (gpointer data)
{
  Spawn *spawn = data;

  g_clear_object (&spawn->container);
  g_clear_object (&spawn->profile);
  g_clear_object (&spawn->pty);
  g_clear_pointer (&spawn->last_working_directory_uri, g_free);
  g_clear_pointer (&spawn->argv, g_strfreev);
  g_free (spawn);
}

static void
prompt_application_spawn_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  PromptClient *client = (PromptClient *)object;
  g_autoptr(PromptIpcProcess) process = NULL;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (PROMPT_IS_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!(process = prompt_client_spawn_finish (client, result, &error)))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_pointer (task, g_steal_pointer (&process), g_object_unref);
}

static void
prompt_application_get_preferred_shell_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  PromptClient *client = (PromptClient *)object;
  g_autofree char *default_shell = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;
  PromptApplication *self;
  Spawn *spawn;

  g_assert (PROMPT_IS_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  spawn = g_task_get_task_data (task);

  g_assert (PROMPT_IS_APPLICATION (self));
  g_assert (spawn != NULL);
  g_assert (VTE_IS_PTY (spawn->pty));
  g_assert (PROMPT_IPC_IS_CONTAINER (spawn->container));
  g_assert (PROMPT_IS_PROFILE (spawn->profile));
  g_assert (PROMPT_IS_CLIENT (self->client));

  default_shell = prompt_client_discover_shell_finish (client, result, NULL);

  if (default_shell && !default_shell[0])
    g_clear_pointer (&default_shell, g_free);

  prompt_client_spawn_async (self->client,
                             spawn->container,
                             spawn->profile,
                             default_shell,
                             spawn->last_working_directory_uri,
                             spawn->pty,
                             (const char * const *)spawn->argv,
                             g_task_get_cancellable (task),
                             prompt_application_spawn_cb,
                             g_object_ref (task));
}

void
prompt_application_spawn_async (PromptApplication   *self,
                                PromptIpcContainer  *container,
                                PromptProfile       *profile,
                                const char          *last_working_directory_uri,
                                VtePty              *pty,
                                const char * const  *argv,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  Spawn *spawn;

  g_return_if_fail (PROMPT_IS_APPLICATION (self));
  g_return_if_fail (PROMPT_IPC_IS_CONTAINER (container));
  g_return_if_fail (PROMPT_IS_PROFILE (profile));
  g_return_if_fail (VTE_IS_PTY (pty));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (PROMPT_IS_CLIENT (self->client));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, prompt_application_spawn_async);

  spawn = g_new0 (Spawn, 1);
  g_set_object (&spawn->container, container);
  g_set_object (&spawn->profile, profile);
  g_set_object (&spawn->pty, pty);
  g_set_str (&spawn->last_working_directory_uri, last_working_directory_uri);
  spawn->argv = g_strdupv ((char **)argv);
  g_task_set_task_data (task, spawn, spawn_free);

  prompt_client_discover_shell_async (self->client,
                                      NULL,
                                      prompt_application_get_preferred_shell_cb,
                                      g_steal_pointer (&task));
}

PromptIpcProcess *
prompt_application_spawn_finish (PromptApplication  *self,
                                 GAsyncResult       *result,
                                 GError            **error)
{
  g_return_val_if_fail (PROMPT_IS_APPLICATION (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
wait_complete (GTask  *task,
                     int     exit_status,
                     int     term_sig,
                     GError *error)
{
  if (!g_object_get_data (G_OBJECT (task), "DID_COMPLETE"))
    {
      g_object_set_data (G_OBJECT (task), "DID_COMPLETE", GINT_TO_POINTER (1));

      if (error)
        g_task_return_error (task, error);
      else
        g_task_return_int (task, W_EXITCODE (exit_status, term_sig));

      g_object_unref (task);
    }
}

static void
prompt_application_process_exited_cb (PromptIpcProcess *process,
                                      int               exit_status,
                                      GTask            *task)
{
  g_assert (PROMPT_IPC_IS_PROCESS (process));
  g_assert (G_IS_TASK (task));

  wait_complete (task, exit_status, 0, NULL);
}

static void
prompt_application_process_signaled_cb (PromptIpcProcess *process,
                                        int               term_sig,
                                        GTask            *task)
{
  g_assert (PROMPT_IPC_IS_PROCESS (process));
  g_assert (G_IS_TASK (task));

  wait_complete (task, 0, term_sig, NULL);
}

static void
prompt_application_get_leader_kind_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  PromptIpcProcess *process = (PromptIpcProcess *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;
  g_autofree char *leader_kind = NULL;

  g_assert (PROMPT_IPC_IS_PROCESS (process));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!prompt_ipc_process_call_get_leader_kind_finish (process, &leader_kind, NULL, result, &error))
    wait_complete (task, 0, 0, g_steal_pointer (&error));
}

void
prompt_application_wait_async (PromptApplication   *self,
                               PromptIpcProcess    *process,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (PROMPT_IS_APPLICATION (self));
  g_return_if_fail (PROMPT_IPC_IS_PROCESS (process));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  /* Because we only get signals/exit-status via signals (to avoid
   * various race conditions in IPC), we use the RPC to get the leader
   * kind as a sort of ping to determine if the process is still alive
   * initially. It will be removed from the D-Bus connection once it
   * exits or signals.
   */

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, prompt_application_wait_async);

  /* Keep alive until signal is received */
  g_object_ref (task);
  g_signal_connect_object (process,
                           "exited",
                           G_CALLBACK (prompt_application_process_exited_cb),
                           task,
                           0);
  g_signal_connect_object (process,
                           "signaled",
                           G_CALLBACK (prompt_application_process_signaled_cb),
                           task,
                           0);

  /* Now query to ensure the process is still there */
  prompt_ipc_process_call_get_leader_kind (process,
                                           g_variant_new_handle (-1),
                                           NULL,
                                           cancellable,
                                           prompt_application_get_leader_kind_cb,
                                           g_steal_pointer (&task));
}

int
prompt_application_wait_finish (PromptApplication  *self,
                                GAsyncResult       *result,
                                GError            **error)
{
  g_return_val_if_fail (PROMPT_IS_APPLICATION (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_int (G_TASK (result), error);
}

PromptIpcContainer *
prompt_application_discover_current_container (PromptApplication *self,
                                               VtePty            *pty)
{
  g_return_val_if_fail (PROMPT_IS_APPLICATION (self), NULL);

  return prompt_client_discover_current_container (self->client, pty);
}

static void
prompt_application_focus_tab_by_uuid (GSimpleAction *action,
                                      GVariant      *param,
                                      gpointer       user_data)
{
  PromptApplication *self = user_data;
  const char *uuid;

  g_assert (PROMPT_IS_APPLICATION (self));
  g_assert (param != NULL);
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));

  uuid = g_variant_get_string (param, NULL);

  for (const GList *windows = gtk_application_get_windows (GTK_APPLICATION (self));
       windows != NULL;
       windows = windows->next)
    {
      if (PROMPT_IS_WINDOW (windows->data))
        {
          if (prompt_window_focus_tab_by_uuid (windows->data, uuid))
            break;
        }
    }
}

/**
 * prompt_application_find_container_by_name:
 * @self: a #PromptApplication
 *
 * Locates the container by runtime/name.
 *
 * Returns: (transfer full) (nullable): a container or %NULL
 */
PromptIpcContainer *
prompt_application_find_container_by_name (PromptApplication *self,
                                           const char        *runtime,
                                           const char        *name)
{
  g_autoptr(GListModel) model = NULL;
  guint n_items;

  g_return_val_if_fail (PROMPT_IS_APPLICATION (self), NULL);

  if (runtime == NULL || name == NULL)
    return NULL;

  model = prompt_application_list_containers (self);
  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(PromptIpcContainer) container = g_list_model_get_item (model, i);

      if (g_strcmp0 (runtime, prompt_ipc_container_get_provider (container)) == 0 &&
          g_strcmp0 (name, prompt_ipc_container_get_display_name (container)) == 0)
        return g_steal_pointer (&container);
    }

  return NULL;
}

const char *
prompt_application_get_os_name (PromptApplication *self)
{
  g_return_val_if_fail (PROMPT_IS_APPLICATION (self), NULL);

  return prompt_client_get_os_name (self->client);
}

static void
prompt_application_save_session_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(PromptApplication) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (PROMPT_IS_APPLICATION (self));

  g_application_release (G_APPLICATION (self));

  if (!g_file_replace_contents_finish (file, result, NULL, &error))
    g_warning ("Failed to save session state: %s", error->message);
}

void
prompt_application_save_session (PromptApplication *self)
{
  g_autoptr(GVariant) state = NULL;
  g_autoptr(GBytes) bytes = NULL;

  g_return_if_fail (PROMPT_IS_APPLICATION (self));

  if ((state = prompt_session_save (self)) &&
      (bytes = g_variant_get_data_as_bytes (state)))
    {
      g_autoptr(GFile) file = get_session_file ();
      g_autoptr(GFile) directory = g_file_get_parent (file);

      g_application_hold (G_APPLICATION (self));

      g_file_make_directory_with_parents (directory, NULL, NULL);
      g_file_replace_contents_bytes_async (file,
                                           bytes,
                                           NULL,
                                           FALSE,
                                           G_FILE_CREATE_REPLACE_DESTINATION,
                                           NULL,
                                           prompt_application_save_session_cb,
                                           g_object_ref (self));
    }
}
