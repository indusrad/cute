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

#include "prompt-action-group.h"
#include "prompt-application.h"
#include "prompt-container-provider.h"
#include "prompt-host-provider.h"
#include "prompt-podman-provider.h"
#include "prompt-preferences-window.h"
#include "prompt-profile-menu.h"
#include "prompt-settings.h"
#include "prompt-toolbox-container.h"
#include "prompt-window.h"

#define PORTAL_BUS_NAME "org.freedesktop.portal.Desktop"
#define PORTAL_OBJECT_PATH "/org/freedesktop/portal/desktop"
#define PORTAL_SETTINGS_INTERFACE "org.freedesktop.portal.Settings"

struct _PromptApplication
{
  AdwApplication       parent_instance;
  GListStore          *profiles;
  GtkFlattenListModel *containers;
  GListStore          *providers;
  PromptSettings      *settings;
  PromptShortcuts     *shortcuts;
  PromptProfileMenu   *profile_menu;
  char                *system_font_name;
  GDBusProxy          *portal;
};

static void prompt_application_about        (PromptApplication *self,
                                             GVariant          *param);
static void prompt_application_edit_profile (PromptApplication *self,
                                             GVariant          *param);
static void prompt_application_help_overlay (PromptApplication *self,
                                             GVariant          *param);
static void prompt_application_preferences  (PromptApplication *self,
                                             GVariant          *param);

PROMPT_DEFINE_ACTION_GROUP (PromptApplication, prompt_application, {
  { "about", prompt_application_about },
  { "edit-profile", prompt_application_edit_profile, "s" },
  { "help-overlay", prompt_application_help_overlay },
  { "preferences", prompt_application_preferences },
})

G_DEFINE_FINAL_TYPE_WITH_CODE (PromptApplication, prompt_application, ADW_TYPE_APPLICATION,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, prompt_application_init_action_group))

enum {
  PROP_0,
  PROP_DEFAULT_PROFILE,
  PROP_PROFILE_MENU,
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

static void
prompt_application_activate (GApplication *app)
{
  PromptWindow *window;

  g_assert (PROMPT_IS_APPLICATION (app));

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

  window = prompt_window_new ();

  gtk_window_present (GTK_WINDOW (window));
}

static int
prompt_application_command_line (GApplication            *app,
                                 GApplicationCommandLine *cmdline)
{
  PromptApplication *self = (PromptApplication *)app;
  GVariantDict *dict;

  g_assert (PROMPT_IS_APPLICATION (self));
  g_assert (G_IS_APPLICATION_COMMAND_LINE (cmdline));

  dict = g_application_command_line_get_options_dict (cmdline);

  if (g_variant_dict_contains (dict, "preferences"))
    g_action_group_activate_action (G_ACTION_GROUP (self), "preferences", NULL);
  else
    g_application_activate (G_APPLICATION (self));

  return G_APPLICATION_CLASS (prompt_application_parent_class)->command_line (app, cmdline);
}

static void
prompt_application_add_providers (PromptApplication *self)
{
  g_autoptr(PromptContainerProvider) host = NULL;
  g_autoptr(PromptContainerProvider) podman = NULL;

  g_assert (PROMPT_IS_APPLICATION (self));

  host = prompt_host_provider_new ();
  g_list_store_append (self->providers, host);

  podman = prompt_podman_provider_new ();
  prompt_podman_provider_set_type_for_label (PROMPT_PODMAN_PROVIDER (podman),
                                             "com.github.containers.toolbox",
                                             NULL,
                                             PROMPT_TYPE_TOOLBOX_CONTAINER);
  g_list_store_append (self->providers, podman);
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

static void
prompt_application_startup (GApplication *application)
{
  static const char *patterns[] = { "org.gnome.*", NULL };
  PromptApplication *self = (PromptApplication *)application;
  g_autoptr(PromptContainer) host = NULL;
  AdwStyleManager *style_manager;

  g_assert (PROMPT_IS_APPLICATION (self));

  g_application_set_default (application);
  g_application_set_resource_base_path (G_APPLICATION (self), "/org/gnome/Prompt");

  self->profiles = g_list_store_new (PROMPT_TYPE_PROFILE);
  self->settings = prompt_settings_new ();
  self->shortcuts = prompt_shortcuts_new (NULL);
  self->profile_menu = prompt_profile_menu_new (self->settings);
  self->providers = g_list_store_new (PROMPT_TYPE_CONTAINER_PROVIDER);
  self->containers = gtk_flatten_list_model_new (g_object_ref (G_LIST_MODEL (self->providers)));

  G_APPLICATION_CLASS (prompt_application_parent_class)->startup (application);

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

  prompt_application_add_providers (self);
}

static void
prompt_application_shutdown (GApplication *application)
{
  PromptApplication *self = (PromptApplication *)application;

  g_assert (PROMPT_IS_APPLICATION (self));

  G_APPLICATION_CLASS (prompt_application_parent_class)->shutdown (application);

  g_clear_object (&self->profile_menu);
  g_clear_object (&self->profiles);
  g_clear_object (&self->containers);
  g_clear_object (&self->providers);
  g_clear_object (&self->portal);
  g_clear_object (&self->shortcuts);
  g_clear_object (&self->settings);

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

    case PROP_PROFILE_MENU:
      g_value_set_object (value, self->profile_menu);
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

  properties[PROP_PROFILE_MENU] =
    g_param_spec_object ("profile-menu", NULL, NULL,
                         PROMPT_TYPE_PROFILE_MENU,
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
  static const GOptionEntry main_entries[] = {
    { "preferences", 0, 0, G_OPTION_ARG_NONE, NULL, N_("Show the application preferences") },
    { NULL }
  };

  g_application_add_main_option_entries (G_APPLICATION (self), main_entries);

  self->system_font_name = g_strdup ("Monospace 11");
}

static void
prompt_application_edit_profile (PromptApplication *self,
                                 GVariant          *param)
{
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

static void
prompt_application_about (PromptApplication *self,
                          GVariant          *param)
{
  static const char *developers[] = {"Christian Hergert", NULL};
  GtkWindow *window = NULL;

  g_assert (PROMPT_IS_APPLICATION (self));

  window = gtk_application_get_active_window (GTK_APPLICATION (self));

  adw_show_about_window (window,
                         "application-icon", PACKAGE_ICON_NAME,
                         "application-name", PACKAGE_NAME,
                         "copyright", "© 2023 Red Hat, Inc.",
                         "developer-name", "Christian Hergert",
                         "developers", developers,
                         "version", "0.1.0",
                         "license-type", GTK_LICENSE_GPL_3_0,
                         NULL);
}

static void
prompt_application_preferences (PromptApplication *self,
                                GVariant          *param)
{
  PromptPreferencesWindow *window;

  g_assert (PROMPT_IS_APPLICATION (self));

  window = prompt_preferences_window_get_default ();
  gtk_application_add_window (GTK_APPLICATION (self), GTK_WINDOW (window));
  gtk_window_present (GTK_WINDOW (window));
}

static void
prompt_application_help_overlay (PromptApplication *self,
                                 GVariant          *param)
{
  PromptPreferencesWindow *window;

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

  if (profile_uuid == 0 || profile_uuid[0] == 0)
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

/**
 * prompt_application_list_containers:
 * @self: a #PromptApplication
 *
 * Gets a #GListModel of #PromptContainer.
 *
 * Returns: (transfer full) (not nullable): a #GListModel
 */
GListModel *
prompt_application_list_containers (PromptApplication *self)
{
  g_return_val_if_fail (PROMPT_IS_APPLICATION (self), NULL);

  return g_object_ref (G_LIST_MODEL (self->containers));
}

PromptContainer *
prompt_application_lookup_container (PromptApplication *self,
                                     const char        *container_id)
{
  g_autoptr(GListModel) model = NULL;
  guint n_items;

  g_return_val_if_fail (PROMPT_IS_APPLICATION (self), NULL);

  if (container_id == NULL || container_id[0] == 0)
    return NULL;

  model = prompt_application_list_containers (self);
  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(PromptContainer) container = g_list_model_get_item (model, i);
      const char *id = prompt_container_get_id (container);

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
