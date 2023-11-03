/* capsule-application.c
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

#include "capsule-action-group.h"
#include "capsule-application.h"
#include "capsule-host-container.h"
#include "capsule-preferences-window.h"
#include "capsule-profile-menu.h"
#include "capsule-settings.h"
#include "capsule-window.h"

#define PORTAL_BUS_NAME "org.freedesktop.portal.Desktop"
#define PORTAL_OBJECT_PATH "/org/freedesktop/portal/desktop"
#define PORTAL_SETTINGS_INTERFACE "org.freedesktop.portal.Settings"

struct _CapsuleApplication
{
	AdwApplication      parent_instance;
  GListStore         *profiles;
  GListStore         *containers;
  CapsuleSettings    *settings;
  CapsuleProfileMenu *profile_menu;
  char               *system_font_name;
  GDBusProxy         *portal;
};

static void capsule_application_about        (CapsuleApplication *self,
                                              GVariant           *param);
static void capsule_application_edit_profile (CapsuleApplication *self,
                                              GVariant           *param);
static void capsule_application_preferences  (CapsuleApplication *self,
                                              GVariant           *param);

CAPSULE_DEFINE_ACTION_GROUP (CapsuleApplication, capsule_application, {
  { "about", capsule_application_about },
  { "edit-profile", capsule_application_edit_profile, "s" },
  { "preferences", capsule_application_preferences },
})

G_DEFINE_FINAL_TYPE_WITH_CODE (CapsuleApplication, capsule_application, ADW_TYPE_APPLICATION,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, capsule_application_init_action_group))

enum {
  PROP_0,
  PROP_PROFILE_MENU,
  PROP_SYSTEM_FONT_NAME,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

CapsuleApplication *
capsule_application_new (const char        *application_id,
                         GApplicationFlags  flags)
{
	g_return_val_if_fail (application_id != NULL, NULL);

	return g_object_new (CAPSULE_TYPE_APPLICATION,
	                     "application-id", application_id,
	                     "flags", flags,
	                     NULL);
}

static void
capsule_application_activate (GApplication *app)
{
  CapsuleWindow *window;

	g_assert (CAPSULE_IS_APPLICATION (app));

	for (const GList *windows = gtk_application_get_windows (GTK_APPLICATION (app));
       windows != NULL;
       windows = windows->next)
    {
      if (CAPSULE_IS_WINDOW (windows->data))
        {
          gtk_window_present (windows->data);
          return;
        }
    }

  window = capsule_window_new ();

	gtk_window_present (GTK_WINDOW (window));
}

static void
on_portal_settings_changed_cb (CapsuleApplication *self,
                               const char         *sender_name,
                               const char         *signal_name,
                               GVariant           *parameters,
                               gpointer            user_data)
{
  g_autoptr(GVariant) value = NULL;
  const char *schema_id;
  const char *key;

  g_assert (CAPSULE_IS_APPLICATION (self));
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
parse_portal_settings (CapsuleApplication *self,
                       GVariant           *parameters)
{
  GVariantIter *iter = NULL;
  const char *schema_str;
  GVariant *val;

  g_assert (CAPSULE_IS_APPLICATION (self));

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
capsule_application_notify_profile_uuids_cb (CapsuleApplication *self,
                                             GParamSpec         *pspec,
                                             CapsuleSettings    *settings)
{
  g_auto(GStrv) profile_uuids = NULL;
  g_autoptr(GPtrArray) array = NULL;
  guint n_items;

  g_assert (CAPSULE_IS_APPLICATION (self));
  g_assert (CAPSULE_IS_SETTINGS (settings));

  profile_uuids = capsule_settings_dup_profile_uuids (settings);
  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->profiles));
  array = g_ptr_array_new_with_free_func (g_object_unref);

  for (guint i = 0; profile_uuids[i]; i++)
    g_ptr_array_add (array, capsule_profile_new (profile_uuids[i]));

  g_list_store_splice (self->profiles, 0, n_items, array->pdata, array->len);
}

static void
capsule_application_startup (GApplication *application)
{
  static const char *patterns[] = { "org.gnome.*", NULL };
  CapsuleApplication *self = (CapsuleApplication *)application;
  g_autoptr(CapsuleContainer) host = NULL;

  g_assert (CAPSULE_IS_APPLICATION (self));

  g_application_set_default (application);
  g_application_set_resource_base_path (G_APPLICATION (self), "/org/gnome/Capsule");

  self->containers = g_list_store_new (CAPSULE_TYPE_CONTAINER);
  self->profiles = g_list_store_new (CAPSULE_TYPE_PROFILE);
  self->settings = capsule_settings_new ();
  self->profile_menu = capsule_profile_menu_new (self->settings);

  host = capsule_host_container_new ();
  g_list_store_append (self->containers, host);

  G_APPLICATION_CLASS (capsule_application_parent_class)->startup (application);

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
                           G_CALLBACK (capsule_application_notify_profile_uuids_cb),
                           self,
                           G_CONNECT_SWAPPED);

  capsule_application_notify_profile_uuids_cb (self, NULL, self->settings);
}

static void
capsule_application_shutdown (GApplication *application)
{
  CapsuleApplication *self = (CapsuleApplication *)application;

  g_assert (CAPSULE_IS_APPLICATION (self));

  G_APPLICATION_CLASS (capsule_application_parent_class)->shutdown (application);

  g_clear_object (&self->profile_menu);
  g_clear_object (&self->profiles);
  g_clear_object (&self->containers);
  g_clear_object (&self->portal);
  g_clear_object (&self->settings);

  g_clear_pointer (&self->system_font_name, g_free);
}

static void
capsule_application_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  CapsuleApplication *self = CAPSULE_APPLICATION (object);

  switch (prop_id)
    {
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
capsule_application_class_init (CapsuleApplicationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

  object_class->get_property = capsule_application_get_property;

	app_class->activate = capsule_application_activate;
	app_class->startup = capsule_application_startup;
	app_class->shutdown = capsule_application_shutdown;

  properties[PROP_PROFILE_MENU] =
    g_param_spec_object ("profile-menu", NULL, NULL,
                         CAPSULE_TYPE_PROFILE_MENU,
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
capsule_application_init (CapsuleApplication *self)
{
  self->system_font_name = g_strdup ("Monospace 11");
}

static void
capsule_application_edit_profile (CapsuleApplication *self,
                                  GVariant           *param)
{
  const char *profile_uuid;

	g_assert (CAPSULE_IS_APPLICATION (self));
	g_assert (param != NULL);
	g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));

  profile_uuid = g_variant_get_string (param, NULL);

  g_printerr ("TODO: show preferences for %s\n", profile_uuid);
}

static void
capsule_application_about (CapsuleApplication *self,
                           GVariant           *param)
{
	static const char *developers[] = {"Christian Hergert", NULL};
	GtkWindow *window = NULL;

	g_assert (CAPSULE_IS_APPLICATION (self));

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
capsule_application_preferences (CapsuleApplication *self,
                                 GVariant           *param)
{
  CapsulePreferencesWindow *window;

  g_assert (CAPSULE_IS_APPLICATION (self));

  window = capsule_preferences_window_get_default ();

  gtk_window_present (GTK_WINDOW (window));
}

/**
 * capsule_application_list_profiles:
 * @self: a #CapsuleApplication
 *
 * Gets a #GListModel or profiles that are available to the application.
 *
 * The resulting #GListModel will update as profiles are created or deleted.
 *
 * Returns: (transfer full) (not nullable): a #GListModel
 */
GListModel *
capsule_application_list_profiles (CapsuleApplication *self)
{
  g_return_val_if_fail (CAPSULE_IS_APPLICATION (self), NULL);

  return g_object_ref (G_LIST_MODEL (self->profiles));
}

/**
 * capsule_application_dup_default_profile:
 * @self: a #CapsuleApplication
 *
 * Gets the default profile for the application.
 *
 * Returns: (transfer full): a #CapsuleProfile
 */
CapsuleProfile *
capsule_application_dup_default_profile (CapsuleApplication *self)
{
  g_autoptr(CapsuleProfile) new_profile = NULL;
  g_autoptr(GListModel) profiles = NULL;
  g_autofree char *default_profile_uuid = NULL;
  guint n_items;

  g_return_val_if_fail (CAPSULE_IS_APPLICATION (self), NULL);

  default_profile_uuid = capsule_settings_dup_default_profile_uuid (self->settings);
  profiles = capsule_application_list_profiles (self);
  n_items = g_list_model_get_n_items (profiles);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(CapsuleProfile) profile = g_list_model_get_item (profiles, i);
      const char *profile_uuid = capsule_profile_get_uuid (profile);

      if (g_strcmp0 (profile_uuid, default_profile_uuid) == 0)
        return g_steal_pointer (&profile);
    }

  if (n_items > 0)
    return g_list_model_get_item (profiles, 0);

  new_profile = capsule_profile_new (NULL);

  g_assert (new_profile != NULL);
  g_assert (CAPSULE_IS_PROFILE (new_profile));
  g_assert (capsule_profile_get_uuid (new_profile) != NULL);

  capsule_application_add_profile (self, new_profile);
  capsule_application_set_default_profile (self, new_profile);

  return g_steal_pointer (&new_profile);
}

void
capsule_application_set_default_profile (CapsuleApplication *self,
                                         CapsuleProfile     *profile)
{
  g_return_if_fail (CAPSULE_IS_APPLICATION (self));
  g_return_if_fail (CAPSULE_IS_PROFILE (profile));

  capsule_settings_set_default_profile_uuid (self->settings,
                                             capsule_profile_get_uuid (profile));
}

void
capsule_application_add_profile (CapsuleApplication *self,
                                 CapsuleProfile     *profile)
{
  g_return_if_fail (CAPSULE_IS_APPLICATION (self));

  capsule_settings_add_profile_uuid (self->settings,
                                     capsule_profile_get_uuid (profile));
}

void
capsule_application_remove_profile (CapsuleApplication *self,
                                    CapsuleProfile     *profile)
{
  g_return_if_fail (CAPSULE_IS_APPLICATION (self));
  g_return_if_fail (CAPSULE_IS_PROFILE (profile));

  capsule_settings_remove_profile_uuid (self->settings,
                                        capsule_profile_get_uuid (profile));
}

CapsuleProfile *
capsule_application_dup_profile (CapsuleApplication *self,
                                 const char         *profile_uuid)
{
  g_autoptr(GListModel) model = NULL;
  guint n_items;

  g_return_val_if_fail (CAPSULE_IS_APPLICATION (self), NULL);

  if (profile_uuid == 0 || profile_uuid[0] == 0)
    return capsule_application_dup_default_profile (self);

  model = capsule_application_list_profiles (self);
  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(CapsuleProfile) profile = g_list_model_get_item (model, i);

      if (g_strcmp0 (profile_uuid, capsule_profile_get_uuid (profile)) == 0)
        return g_steal_pointer (&profile);
    }

  return capsule_profile_new (profile_uuid);
}

gboolean
capsule_application_control_is_pressed (CapsuleApplication *self)
{
  GdkDisplay *display = gdk_display_get_default ();
  GdkSeat *seat = gdk_display_get_default_seat (display);
  GdkDevice *keyboard = gdk_seat_get_keyboard (seat);
  GdkModifierType modifiers = gdk_device_get_modifier_state (keyboard) & gtk_accelerator_get_default_mod_mask ();

  return !!(modifiers & GDK_CONTROL_MASK);
}

const char *
capsule_application_get_system_font_name (CapsuleApplication *self)
{
  g_return_val_if_fail (CAPSULE_IS_APPLICATION (self), NULL);

  return self->system_font_name;
}

GMenuModel *
capsule_application_dup_profile_menu (CapsuleApplication *self)
{
  g_return_val_if_fail (CAPSULE_IS_APPLICATION (self), NULL);

  return g_object_ref (G_MENU_MODEL (self->profile_menu));
}

/**
 * capsule_application_list_containers:
 * @self: a #CapsuleApplication
 *
 * Gets a #GListModel of #CapsuleContainer.
 *
 * Returns: (transfer full) (not nullable): a #GListModel
 */
GListModel *
capsule_application_list_containers (CapsuleApplication *self)
{
  g_return_val_if_fail (CAPSULE_IS_APPLICATION (self), NULL);

  return g_object_ref (G_LIST_MODEL (self->containers));
}

CapsuleContainer *
capsule_application_lookup_container (CapsuleApplication *self,
                                      const char         *container_id)
{
  g_autoptr(GListModel) model = NULL;
  guint n_items;

  g_return_val_if_fail (CAPSULE_IS_APPLICATION (self), NULL);

  if (container_id == NULL || container_id[0] == 0)
    return NULL;

  model = capsule_application_list_containers (self);
  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(CapsuleContainer) container = g_list_model_get_item (model, i);
      const char *id = capsule_container_get_id (container);

      if (g_strcmp0 (id, container_id) == 0)
        return g_steal_pointer (&container);
    }

  return NULL;
}
