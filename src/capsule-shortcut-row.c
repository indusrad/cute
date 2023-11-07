/*
 * capsule-shortcut-row.c
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

#include "capsule-shortcut-row.h"

struct _CapsuleShortcutRow
{
  AdwActionRow parent_instance;

  char *accelerator;

  GtkLabel *label;
};

enum {
  PROP_0,
  PROP_ACCELERATOR,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (CapsuleShortcutRow, capsule_shortcut_row, ADW_TYPE_ACTION_ROW)

static GParamSpec *properties [N_PROPS];

static void
capsule_shortcut_row_dispose (GObject *object)
{
  CapsuleShortcutRow *self = (CapsuleShortcutRow *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), CAPSULE_TYPE_SHORTCUT_ROW);

  g_clear_pointer (&self->accelerator, g_free);

  G_OBJECT_CLASS (capsule_shortcut_row_parent_class)->dispose (object);
}

static void
capsule_shortcut_row_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  CapsuleShortcutRow *self = CAPSULE_SHORTCUT_ROW (object);

  switch (prop_id)
    {
    case PROP_ACCELERATOR:
      g_value_set_string (value, capsule_shortcut_row_get_accelerator (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
capsule_shortcut_row_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  CapsuleShortcutRow *self = CAPSULE_SHORTCUT_ROW (object);

  switch (prop_id)
    {
    case PROP_ACCELERATOR:
      capsule_shortcut_row_set_accelerator (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
capsule_shortcut_row_class_init (CapsuleShortcutRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = capsule_shortcut_row_dispose;
  object_class->get_property = capsule_shortcut_row_get_property;
  object_class->set_property = capsule_shortcut_row_set_property;

  properties[PROP_ACCELERATOR] =
    g_param_spec_string ("accelerator", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Capsule/capsule-shortcut-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CapsuleShortcutRow, label);
}

static void
capsule_shortcut_row_init (CapsuleShortcutRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

const char *
capsule_shortcut_row_get_accelerator (CapsuleShortcutRow *self)
{
  g_return_val_if_fail (CAPSULE_IS_SHORTCUT_ROW (self), NULL);

  return self->accelerator;
}

void
capsule_shortcut_row_set_accelerator (CapsuleShortcutRow *self,
                                      const char         *accelerator)
{
  g_return_if_fail (CAPSULE_IS_SHORTCUT_ROW (self));

  if (g_set_str (&self->accelerator, accelerator))
    {
      g_autofree char *label = NULL;
      GdkModifierType state;
      guint keyval;

      if (accelerator && accelerator[0] && gtk_accelerator_parse (accelerator, &keyval, &state))
        label = gtk_accelerator_get_label (keyval, state);

      if (label != NULL)
        {
          gtk_label_set_label (self->label, label);
          gtk_widget_remove_css_class (GTK_WIDGET (self->label), "dim-label");
        }
      else
        {
          gtk_label_set_label (self->label, _("disabled"));
          gtk_widget_add_css_class (GTK_WIDGET (self->label), "dim-label");
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACCELERATOR]);
    }
}
