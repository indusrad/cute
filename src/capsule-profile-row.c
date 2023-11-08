/*
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
#include "capsule-profile-row.h"
#include "capsule-preferences-window.h"

struct _CapsuleProfileRow
{
  AdwActionRow    parent_instance;

  GtkImage       *checkmark;

  CapsuleProfile *profile;
};

enum {
  PROP_0,
  PROP_PROFILE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (CapsuleProfileRow, capsule_profile_row, ADW_TYPE_ACTION_ROW)

static GParamSpec *properties [N_PROPS];

static void
capsule_profile_row_duplicate (GtkWidget  *widget,
                               const char *action_name,
                               GVariant   *param)
{
  CapsuleProfileRow *self = (CapsuleProfileRow *)widget;
  g_autoptr(CapsuleProfile) profile = NULL;

  g_assert (CAPSULE_IS_PROFILE_ROW (self));

  profile = capsule_profile_duplicate (self->profile);
}

static void
capsule_profile_row_edit (GtkWidget  *widget,
                          const char *action_name,
                          GVariant   *param)
{
  adw_action_row_activate (ADW_ACTION_ROW (widget));
}

static void
capsule_profile_row_undo_clicked_cb (AdwToast       *toast,
                                     CapsuleProfile *profile)
{
  g_assert (ADW_IS_TOAST (toast));
  g_assert (CAPSULE_IS_PROFILE (profile));

  capsule_application_add_profile (CAPSULE_APPLICATION_DEFAULT, profile);
}

static void
capsule_profile_row_remove (GtkWidget  *widget,
                            const char *action_name,
                            GVariant   *param)
{
  CapsuleProfileRow *self = (CapsuleProfileRow *)widget;
  AdwPreferencesWindow *window;
  AdwToast *toast;

  g_assert (CAPSULE_IS_PROFILE_ROW (self));

  window = ADW_PREFERENCES_WINDOW (gtk_widget_get_ancestor (widget, ADW_TYPE_PREFERENCES_WINDOW));
  toast = adw_toast_new_format (_("Removed profile “%s”"),
                                capsule_profile_dup_label (self->profile));
  adw_toast_set_button_label (toast, _("Undo"));
  g_signal_connect_data (toast,
                         "button-clicked",
                         G_CALLBACK (capsule_profile_row_undo_clicked_cb),
                         g_object_ref (self->profile),
                         (GClosureNotify)g_object_unref,
                         0);

  capsule_application_remove_profile (CAPSULE_APPLICATION_DEFAULT, self->profile);

  adw_preferences_window_add_toast (window, toast);
}

static void
capsule_profile_row_make_default (GtkWidget  *widget,
                                  const char *action_name,
                                  GVariant   *param)
{
  CapsuleProfileRow *self = CAPSULE_PROFILE_ROW (widget);

  capsule_application_set_default_profile (CAPSULE_APPLICATION_DEFAULT, self->profile);
}

static void
capsule_profile_row_default_profile_changed_cb (CapsuleProfileRow *self,
                                                GParamSpec        *pspec,
                                                CapsuleSettings   *settings)
{
  const char *default_uuid;
  gboolean is_default;

  g_assert (CAPSULE_IS_PROFILE_ROW (self));
  g_assert (CAPSULE_IS_SETTINGS (settings));

  default_uuid = capsule_settings_dup_default_profile_uuid (settings);

  is_default = g_strcmp0 (default_uuid, capsule_profile_get_uuid (self->profile)) == 0;

  gtk_widget_set_visible (GTK_WIDGET (self->checkmark), is_default);
}

static void
capsule_profile_row_constructed (GObject *object)
{
  CapsuleProfileRow *self = (CapsuleProfileRow *)object;
  CapsuleApplication *app = CAPSULE_APPLICATION_DEFAULT;
  CapsuleSettings *settings = capsule_application_get_settings (app);

  G_OBJECT_CLASS (capsule_profile_row_parent_class)->constructed (object);

  g_signal_connect_object (settings,
                           "notify::default-profile-uuid",
                           G_CALLBACK (capsule_profile_row_default_profile_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  capsule_profile_row_default_profile_changed_cb (self, NULL, settings);

  g_object_bind_property (self->profile, "label", self, "title",
                          G_BINDING_SYNC_CREATE);
}

static void
capsule_profile_row_dispose (GObject *object)
{
  CapsuleProfileRow *self = (CapsuleProfileRow *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), CAPSULE_TYPE_PROFILE_ROW);

  g_clear_object (&self->profile);

  G_OBJECT_CLASS (capsule_profile_row_parent_class)->dispose (object);
}

static void
capsule_profile_row_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  CapsuleProfileRow *self = CAPSULE_PROFILE_ROW (object);

  switch (prop_id)
    {
    case PROP_PROFILE:
      g_value_set_object (value, capsule_profile_row_get_profile (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
capsule_profile_row_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  CapsuleProfileRow *self = CAPSULE_PROFILE_ROW (object);

  switch (prop_id)
    {
    case PROP_PROFILE:
      self->profile = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
capsule_profile_row_class_init (CapsuleProfileRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = capsule_profile_row_constructed;
  object_class->dispose = capsule_profile_row_dispose;
  object_class->get_property = capsule_profile_row_get_property;
  object_class->set_property = capsule_profile_row_set_property;

  properties[PROP_PROFILE] =
    g_param_spec_object ("profile", NULL, NULL,
                         CAPSULE_TYPE_PROFILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_install_action (widget_class,
                                   "profile.duplicate",
                                   NULL,
                                   capsule_profile_row_duplicate);
  gtk_widget_class_install_action (widget_class,
                                   "profile.edit",
                                   NULL,
                                   capsule_profile_row_edit);
  gtk_widget_class_install_action (widget_class,
                                   "profile.remove",
                                   NULL,
                                   capsule_profile_row_remove);
  gtk_widget_class_install_action (widget_class,
                                   "profile.make-default",
                                   NULL,
                                   capsule_profile_row_make_default);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Capsule/capsule-profile-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CapsuleProfileRow, checkmark);
}

static void
capsule_profile_row_init (CapsuleProfileRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
capsule_profile_row_new (CapsuleProfile *profile)
{
  g_return_val_if_fail (CAPSULE_IS_PROFILE (profile), NULL);

  return g_object_new (CAPSULE_TYPE_PROFILE_ROW,
                       "profile", profile,
                       NULL);
}

CapsuleProfile *
capsule_profile_row_get_profile (CapsuleProfileRow *self)
{
  g_return_val_if_fail (CAPSULE_IS_PROFILE_ROW (self), NULL);

  return self->profile;
}
