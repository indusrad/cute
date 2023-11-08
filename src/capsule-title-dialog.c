/*
 * capsule-title-dialog.c
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

#include "capsule-title-dialog.h"
#include "capsule-tab.h"

struct _CapsuleTitleDialog
{
  AdwWindow parent_instance;

  CapsuleTab *tab;

  GtkEntry *entry;
};

enum {
  PROP_0,
  PROP_TAB,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (CapsuleTitleDialog, capsule_title_dialog, ADW_TYPE_WINDOW)

static GParamSpec *properties [N_PROPS];

static void
capsule_title_dialog_constructed (GObject *object)
{
  CapsuleTitleDialog *self = (CapsuleTitleDialog *)object;

  g_assert (CAPSULE_IS_TITLE_DIALOG (self));

  G_OBJECT_CLASS (capsule_title_dialog_parent_class)->constructed (object);

  g_object_bind_property (self->tab, "title-prefix", self->entry, "text",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
}

static void
capsule_title_dialog_dispose (GObject *object)
{
  CapsuleTitleDialog *self = (CapsuleTitleDialog *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), CAPSULE_TYPE_TITLE_DIALOG);

  g_clear_object (&self->tab);

  G_OBJECT_CLASS (capsule_title_dialog_parent_class)->dispose (object);
}

static void
capsule_title_dialog_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  CapsuleTitleDialog *self = CAPSULE_TITLE_DIALOG (object);

  switch (prop_id)
    {
    case PROP_TAB:
      g_value_set_object (value, self->tab);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
capsule_title_dialog_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  CapsuleTitleDialog *self = CAPSULE_TITLE_DIALOG (object);

  switch (prop_id)
    {
    case PROP_TAB:
      self->tab = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
capsule_title_dialog_class_init (CapsuleTitleDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = capsule_title_dialog_constructed;
  object_class->dispose = capsule_title_dialog_dispose;
  object_class->get_property = capsule_title_dialog_get_property;
  object_class->set_property = capsule_title_dialog_set_property;

  properties[PROP_TAB] =
    g_param_spec_object ("tab", NULL, NULL,
                         CAPSULE_TYPE_TAB,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Capsule/capsule-title-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CapsuleTitleDialog, entry);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);
}

static void
capsule_title_dialog_init (CapsuleTitleDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
