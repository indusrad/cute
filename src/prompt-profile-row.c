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

#include "prompt-application.h"
#include "prompt-profile-row.h"
#include "prompt-preferences-window.h"

struct _PromptProfileRow
{
  AdwActionRow    parent_instance;

  GtkImage       *checkmark;

  PromptProfile *profile;
};

enum {
  PROP_0,
  PROP_PROFILE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PromptProfileRow, prompt_profile_row, ADW_TYPE_ACTION_ROW)

static GParamSpec *properties [N_PROPS];

static void
prompt_profile_row_duplicate (GtkWidget  *widget,
                              const char *action_name,
                              GVariant   *param)
{
  PromptProfileRow *self = (PromptProfileRow *)widget;
  g_autoptr(PromptProfile) profile = NULL;

  g_assert (PROMPT_IS_PROFILE_ROW (self));

  profile = prompt_profile_duplicate (self->profile);
}

static void
prompt_profile_row_edit (GtkWidget  *widget,
                         const char *action_name,
                         GVariant   *param)
{
  adw_action_row_activate (ADW_ACTION_ROW (widget));
}

static void
prompt_profile_row_undo_clicked_cb (AdwToast      *toast,
                                    PromptProfile *profile)
{
  g_assert (ADW_IS_TOAST (toast));
  g_assert (PROMPT_IS_PROFILE (profile));

  prompt_application_add_profile (PROMPT_APPLICATION_DEFAULT, profile);
}

static void
prompt_profile_row_remove (GtkWidget  *widget,
                           const char *action_name,
                           GVariant   *param)
{
  PromptProfileRow *self = (PromptProfileRow *)widget;
  AdwPreferencesWindow *window;
  AdwToast *toast;

  g_assert (PROMPT_IS_PROFILE_ROW (self));

  window = ADW_PREFERENCES_WINDOW (gtk_widget_get_ancestor (widget, ADW_TYPE_PREFERENCES_WINDOW));
  toast = adw_toast_new_format (_("Removed profile “%s”"),
                                prompt_profile_dup_label (self->profile));
  adw_toast_set_button_label (toast, _("Undo"));
  g_signal_connect_data (toast,
                         "button-clicked",
                         G_CALLBACK (prompt_profile_row_undo_clicked_cb),
                         g_object_ref (self->profile),
                         (GClosureNotify)g_object_unref,
                         0);

  prompt_application_remove_profile (PROMPT_APPLICATION_DEFAULT, self->profile);

  adw_preferences_window_add_toast (window, toast);
}

static void
prompt_profile_row_make_default (GtkWidget  *widget,
                                 const char *action_name,
                                 GVariant   *param)
{
  PromptProfileRow *self = PROMPT_PROFILE_ROW (widget);

  prompt_application_set_default_profile (PROMPT_APPLICATION_DEFAULT, self->profile);
}

static void
prompt_profile_row_default_profile_changed_cb (PromptProfileRow *self,
                                               GParamSpec       *pspec,
                                               PromptSettings   *settings)
{
  const char *default_uuid;
  gboolean is_default;

  g_assert (PROMPT_IS_PROFILE_ROW (self));
  g_assert (PROMPT_IS_SETTINGS (settings));

  default_uuid = prompt_settings_dup_default_profile_uuid (settings);

  is_default = g_strcmp0 (default_uuid, prompt_profile_get_uuid (self->profile)) == 0;

  gtk_widget_set_visible (GTK_WIDGET (self->checkmark), is_default);
}

static void
prompt_profile_row_constructed (GObject *object)
{
  PromptProfileRow *self = (PromptProfileRow *)object;
  PromptApplication *app = PROMPT_APPLICATION_DEFAULT;
  PromptSettings *settings = prompt_application_get_settings (app);

  G_OBJECT_CLASS (prompt_profile_row_parent_class)->constructed (object);

  g_signal_connect_object (settings,
                           "notify::default-profile-uuid",
                           G_CALLBACK (prompt_profile_row_default_profile_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  prompt_profile_row_default_profile_changed_cb (self, NULL, settings);

  g_object_bind_property (self->profile, "label", self, "title",
                          G_BINDING_SYNC_CREATE);
}

static void
prompt_profile_row_dispose (GObject *object)
{
  PromptProfileRow *self = (PromptProfileRow *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), PROMPT_TYPE_PROFILE_ROW);

  g_clear_object (&self->profile);

  G_OBJECT_CLASS (prompt_profile_row_parent_class)->dispose (object);
}

static void
prompt_profile_row_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  PromptProfileRow *self = PROMPT_PROFILE_ROW (object);

  switch (prop_id)
    {
    case PROP_PROFILE:
      g_value_set_object (value, prompt_profile_row_get_profile (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_profile_row_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  PromptProfileRow *self = PROMPT_PROFILE_ROW (object);

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
prompt_profile_row_class_init (PromptProfileRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = prompt_profile_row_constructed;
  object_class->dispose = prompt_profile_row_dispose;
  object_class->get_property = prompt_profile_row_get_property;
  object_class->set_property = prompt_profile_row_set_property;

  properties[PROP_PROFILE] =
    g_param_spec_object ("profile", NULL, NULL,
                         PROMPT_TYPE_PROFILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_install_action (widget_class,
                                   "profile.duplicate",
                                   NULL,
                                   prompt_profile_row_duplicate);
  gtk_widget_class_install_action (widget_class,
                                   "profile.edit",
                                   NULL,
                                   prompt_profile_row_edit);
  gtk_widget_class_install_action (widget_class,
                                   "profile.remove",
                                   NULL,
                                   prompt_profile_row_remove);
  gtk_widget_class_install_action (widget_class,
                                   "profile.make-default",
                                   NULL,
                                   prompt_profile_row_make_default);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Prompt/prompt-profile-row.ui");

  gtk_widget_class_bind_template_child (widget_class, PromptProfileRow, checkmark);
}

static void
prompt_profile_row_init (PromptProfileRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
prompt_profile_row_new (PromptProfile *profile)
{
  g_return_val_if_fail (PROMPT_IS_PROFILE (profile), NULL);

  return g_object_new (PROMPT_TYPE_PROFILE_ROW,
                       "profile", profile,
                       NULL);
}

PromptProfile *
prompt_profile_row_get_profile (PromptProfileRow *self)
{
  g_return_val_if_fail (PROMPT_IS_PROFILE_ROW (self), NULL);

  return self->profile;
}
