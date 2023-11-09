/*
 * prompt-shortcut-row.c
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

#include "prompt-shortcut-accel-dialog.h"
#include "prompt-shortcut-row.h"

struct _PromptShortcutRow
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

G_DEFINE_FINAL_TYPE (PromptShortcutRow, prompt_shortcut_row, ADW_TYPE_ACTION_ROW)

static GParamSpec *properties [N_PROPS];

static void
prompt_shortcut_row_dialog_shortcut_set_cb (PromptShortcutRow         *self,
                                            const char                *accelerator,
                                            PromptShortcutAccelDialog *dialog)
{
  g_assert (PROMPT_IS_SHORTCUT_ROW (self));
  g_assert (PROMPT_IS_SHORTCUT_ACCEL_DIALOG (dialog));

  prompt_shortcut_row_set_accelerator (self, accelerator ? accelerator : "");
}

static void
prompt_shortcut_row_select_shortcut (GtkWidget  *widget,
                                     const char *action,
                                     GVariant   *param)
{
  PromptShortcutRow *self = (PromptShortcutRow *)widget;
  PromptShortcutAccelDialog *dialog;
  const char *title;

  g_assert (PROMPT_IS_SHORTCUT_ROW (self));

  title = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (self));
  dialog = g_object_new (PROMPT_TYPE_SHORTCUT_ACCEL_DIALOG,
                         "accelerator", self->accelerator,
                         "shortcut-title", title,
                         "title", _("Set Shortcut"),
                         "transient-for", gtk_widget_get_root (widget),
                         NULL);
  g_signal_connect_object (dialog,
                           "shortcut-set",
                           G_CALLBACK (prompt_shortcut_row_dialog_shortcut_set_cb),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_window_present (GTK_WINDOW (dialog));
}

static void
prompt_shortcut_row_dispose (GObject *object)
{
  PromptShortcutRow *self = (PromptShortcutRow *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), PROMPT_TYPE_SHORTCUT_ROW);

  g_clear_pointer (&self->accelerator, g_free);

  G_OBJECT_CLASS (prompt_shortcut_row_parent_class)->dispose (object);
}

static void
prompt_shortcut_row_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  PromptShortcutRow *self = PROMPT_SHORTCUT_ROW (object);

  switch (prop_id)
    {
    case PROP_ACCELERATOR:
      g_value_set_string (value, prompt_shortcut_row_get_accelerator (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_shortcut_row_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  PromptShortcutRow *self = PROMPT_SHORTCUT_ROW (object);

  switch (prop_id)
    {
    case PROP_ACCELERATOR:
      prompt_shortcut_row_set_accelerator (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_shortcut_row_class_init (PromptShortcutRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = prompt_shortcut_row_dispose;
  object_class->get_property = prompt_shortcut_row_get_property;
  object_class->set_property = prompt_shortcut_row_set_property;

  properties[PROP_ACCELERATOR] =
    g_param_spec_string ("accelerator", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Prompt/prompt-shortcut-row.ui");
  gtk_widget_class_bind_template_child (widget_class, PromptShortcutRow, label);
  gtk_widget_class_install_action (widget_class, "shortcut.select", NULL, prompt_shortcut_row_select_shortcut);
}

static void
prompt_shortcut_row_init (PromptShortcutRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

const char *
prompt_shortcut_row_get_accelerator (PromptShortcutRow *self)
{
  g_return_val_if_fail (PROMPT_IS_SHORTCUT_ROW (self), NULL);

  return self->accelerator;
}

void
prompt_shortcut_row_set_accelerator (PromptShortcutRow *self,
                                     const char        *accelerator)
{
  g_return_if_fail (PROMPT_IS_SHORTCUT_ROW (self));

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
