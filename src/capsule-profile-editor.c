/*
 * capsule-profile-editor.c
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

#include "capsule-profile-editor.h"

struct _CapsuleProfileEditor
{
  AdwNavigationPage  parent_instance;

  CapsuleProfile    *profile;

  AdwEntryRow       *label;
  AdwToastOverlay   *toasts;
  GtkLabel          *uuid;
};

enum {
  PROP_0,
  PROP_PROFILE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (CapsuleProfileEditor, capsule_profile_editor, ADW_TYPE_NAVIGATION_PAGE)

static GParamSpec *properties[N_PROPS];

static void
capsule_profile_editor_uuid_copy (GtkWidget  *widget,
                                  const char *action_name,
                                  GVariant   *param)
{
  CapsuleProfileEditor *self = (CapsuleProfileEditor *)widget;
  GdkClipboard *clipboard;
  AdwToast *toast;

  g_assert (CAPSULE_IS_PROFILE_EDITOR (self));

  clipboard = gtk_widget_get_clipboard (widget);
  gdk_clipboard_set_text (clipboard, capsule_profile_get_uuid (self->profile));

  toast = g_object_new (ADW_TYPE_TOAST,
                        "title", _("Copied to clipboard"),
                        "timeout", 3,
                        NULL);
  adw_toast_overlay_add_toast (self->toasts, toast);
}

static void
capsule_profile_editor_constructed (GObject *object)
{
  CapsuleProfileEditor *self = (CapsuleProfileEditor *)object;

  G_OBJECT_CLASS (capsule_profile_editor_parent_class)->constructed (object);

  g_object_bind_property (self->profile, "uuid",
                          self->uuid, "label",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (self->profile, "label",
                          self->label, "text",
                          G_BINDING_SYNC_CREATE);
}

static void
capsule_profile_editor_dispose (GObject *object)
{
  CapsuleProfileEditor *self = (CapsuleProfileEditor *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), CAPSULE_TYPE_PROFILE_EDITOR);

  g_clear_object (&self->profile);

  G_OBJECT_CLASS (capsule_profile_editor_parent_class)->dispose (object);
}

static void
capsule_profile_editor_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  CapsuleProfileEditor *self = CAPSULE_PROFILE_EDITOR (object);

  switch (prop_id)
    {
    case PROP_PROFILE:
      g_value_set_object (value, capsule_profile_editor_get_profile (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
capsule_profile_editor_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  CapsuleProfileEditor *self = CAPSULE_PROFILE_EDITOR (object);

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
capsule_profile_editor_class_init (CapsuleProfileEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = capsule_profile_editor_constructed;
  object_class->dispose = capsule_profile_editor_dispose;
  object_class->get_property = capsule_profile_editor_get_property;
  object_class->set_property = capsule_profile_editor_set_property;

  properties[PROP_PROFILE] =
    g_param_spec_object ("profile", NULL, NULL,
                         CAPSULE_TYPE_PROFILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Capsule/capsule-profile-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, CapsuleProfileEditor, label);
  gtk_widget_class_bind_template_child (widget_class, CapsuleProfileEditor, toasts);
  gtk_widget_class_bind_template_child (widget_class, CapsuleProfileEditor, uuid);

  gtk_widget_class_install_action (widget_class, "uuid.copy", NULL, capsule_profile_editor_uuid_copy);
}

static void
capsule_profile_editor_init (CapsuleProfileEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

CapsuleProfileEditor *
capsule_profile_editor_new (CapsuleProfile *profile)
{
  g_return_val_if_fail (CAPSULE_IS_PROFILE (profile), NULL);

  return g_object_new (CAPSULE_TYPE_PROFILE_EDITOR,
                       "profile", profile,
                       NULL);
}

CapsuleProfile *
capsule_profile_editor_get_profile (CapsuleProfileEditor *self)
{
  g_return_val_if_fail (CAPSULE_IS_PROFILE_EDITOR (self), NULL);

  return self->profile;
}
