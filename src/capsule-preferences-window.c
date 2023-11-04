/*
 * capsule-preferences-window.c
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#include "capsule-application.h"
#include "capsule-preferences-list-item.h"
#include "capsule-preferences-window.h"
#include "capsule-profile-row.h"

struct _CapsulePreferencesWindow
{
  AdwPreferencesWindow  parent_instance;

  GtkListBoxRow        *add_profile_row;
  AdwSwitchRow         *audible_bell;
  GtkListBox           *profiles_list_box;
  AdwComboRow          *tab_position;
  GListModel           *tab_positions;
  AdwSwitchRow         *visual_bell;
};

G_DEFINE_FINAL_TYPE (CapsulePreferencesWindow, capsule_preferences_window, ADW_TYPE_PREFERENCES_WINDOW)

static void
capsule_preferences_window_add_profile (GtkWidget  *widget,
                                        const char *action_name,
                                        GVariant   *param)
{
  CapsulePreferencesWindow *self = (CapsulePreferencesWindow *)widget;
  g_autoptr(CapsuleProfile) profile = NULL;

  g_assert (CAPSULE_IS_PREFERENCES_WINDOW (self));

  profile = capsule_profile_new (NULL);

  capsule_application_add_profile (CAPSULE_APPLICATION_DEFAULT, profile);
}

static void
capsule_preferences_window_profile_row_activated_cb (CapsulePreferencesWindow *self,
                                                     CapsuleProfileRow        *row)
{
  CapsuleProfile *profile;

  g_assert (CAPSULE_IS_PREFERENCES_WINDOW (self));
  g_assert (CAPSULE_IS_PROFILE_ROW (row));

  profile = capsule_profile_row_get_profile (row);

  capsule_preferences_window_edit_profile (self, profile);
}

static GtkWidget *
capsule_preferences_window_create_profile_row_cb (gpointer item,
                                                  gpointer user_data)
{
  return capsule_profile_row_new (CAPSULE_PROFILE (item));
}

static gboolean
string_to_index (GValue   *value,
                 GVariant *variant,
                 gpointer  user_data)
{
  GListModel *model = G_LIST_MODEL (user_data);
  guint n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(CapsulePreferencesListItem) item = CAPSULE_PREFERENCES_LIST_ITEM (g_list_model_get_item (model, i));
      GVariant *item_value = capsule_preferences_list_item_get_value (item);

      if (g_variant_equal (variant, item_value))
        {
          g_value_set_uint (value, i);
          return TRUE;
        }
    }

  return FALSE;
}

static GVariant *
index_to_string (const GValue       *value,
                 const GVariantType *type,
                 gpointer            user_data)
{
  guint index = g_value_get_uint (value);
  GListModel *model = G_LIST_MODEL (user_data);
  g_autoptr(CapsulePreferencesListItem) item = CAPSULE_PREFERENCES_LIST_ITEM (g_list_model_get_item (model, index));

  if (item != NULL)
    return g_variant_ref (capsule_preferences_list_item_get_value (item));

  return NULL;
}

static void
capsule_preferences_window_constructed (GObject *object)
{
  CapsulePreferencesWindow *self = (CapsulePreferencesWindow *)object;
  CapsuleApplication *app = CAPSULE_APPLICATION_DEFAULT;
  CapsuleSettings *settings = capsule_application_get_settings (app);
  GSettings *gsettings = capsule_settings_get_settings (settings);
  g_autoptr(GListModel) profiles = NULL;

  G_OBJECT_CLASS (capsule_preferences_window_parent_class)->constructed (object);

  g_settings_bind_with_mapping (gsettings,
                                CAPSULE_SETTING_KEY_NEW_TAB_POSITION,
                                self->tab_position,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                string_to_index,
                                index_to_string,
                                g_object_ref (self->tab_positions),
                                g_object_unref);

  profiles = capsule_application_list_profiles (app);

  gtk_list_box_bind_model (self->profiles_list_box,
                           profiles,
                           capsule_preferences_window_create_profile_row_cb,
                           NULL, NULL);

  g_object_bind_property (settings, "audible-bell",
                          self->audible_bell, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (settings, "visual-bell",
                          self->visual_bell, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
}

static void
capsule_preferences_window_dispose (GObject *object)
{
  CapsulePreferencesWindow *self = (CapsulePreferencesWindow *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), CAPSULE_TYPE_PREFERENCES_WINDOW);

  G_OBJECT_CLASS (capsule_preferences_window_parent_class)->dispose (object);
}

static void
capsule_preferences_window_class_init (CapsulePreferencesWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = capsule_preferences_window_constructed;
  object_class->dispose = capsule_preferences_window_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Capsule/capsule-preferences-window.ui");

  gtk_widget_class_bind_template_child (widget_class, CapsulePreferencesWindow, add_profile_row);
  gtk_widget_class_bind_template_child (widget_class, CapsulePreferencesWindow, audible_bell);
  gtk_widget_class_bind_template_child (widget_class, CapsulePreferencesWindow, profiles_list_box);
  gtk_widget_class_bind_template_child (widget_class, CapsulePreferencesWindow, tab_position);
  gtk_widget_class_bind_template_child (widget_class, CapsulePreferencesWindow, tab_positions);
  gtk_widget_class_bind_template_child (widget_class, CapsulePreferencesWindow, visual_bell);

  gtk_widget_class_install_action (widget_class,
                                   "profile.add",
                                   NULL,
                                   capsule_preferences_window_add_profile);

  g_type_ensure (CAPSULE_TYPE_PREFERENCES_LIST_ITEM);
  //g_type_ensure (CAPSULE_TYPE_PROFILE_EDITOR);
}

static void
capsule_preferences_window_init (CapsulePreferencesWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWindow *
capsule_preferences_window_new (GtkApplication* application)
{
  return g_object_new (CAPSULE_TYPE_PREFERENCES_WINDOW,
                       NULL);
}

void
capsule_preferences_window_edit_profile (CapsulePreferencesWindow *self,
                                         CapsuleProfile           *profile)
{
#if 0
  GtkWidget *editor;

  g_return_if_fail (CAPSULE_IS_PREFERENCES_WINDOW (self));
  g_return_if_fail (G_IS_SETTINGS (settings));

  editor = capsule_profile_editor_new (settings);

  adw_preferences_window_push_subpage (ADW_PREFERENCES_WINDOW (self),
                                       ADW_NAVIGATION_PAGE (editor));
#endif
}

/**
 * capsule_preferences_window_get_default:
 *
 * Gets the default preferences window for the process.
 *
 * Returns: (transfer none): a #CapsulePreferencesWindow
 */
CapsulePreferencesWindow *
capsule_preferences_window_get_default (void)
{
  static CapsulePreferencesWindow *instance;

  if (instance == NULL)
    {
      instance = g_object_new (CAPSULE_TYPE_PREFERENCES_WINDOW, NULL);
      g_object_add_weak_pointer (G_OBJECT (instance), (gpointer *)&instance);
    }

  return instance;
}
