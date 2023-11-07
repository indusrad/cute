/*
 * capsule-close-dialog.c
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

#include "capsule-close-dialog.h"

struct _CapsuleCloseDialog
{
  AdwMessageDialog parent_instance;
  AdwPreferencesGroup *group;
  AdwPreferencesPage *page;
};

G_DEFINE_FINAL_TYPE (CapsuleCloseDialog, capsule_close_dialog, ADW_TYPE_MESSAGE_DIALOG)

static void
capsule_close_dialog_response_cancel_cb (CapsuleCloseDialog *self,
                                         const char         *response)
{
  g_assert (CAPSULE_IS_CLOSE_DIALOG (self));

}

static void
capsule_close_dialog_response_discard_cb (CapsuleCloseDialog *self,
                                          const char         *response)
{
  g_assert (CAPSULE_IS_CLOSE_DIALOG (self));

}

static void
capsule_close_dialog_dispose (GObject *object)
{
  CapsuleCloseDialog *self = (CapsuleCloseDialog *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), CAPSULE_TYPE_CLOSE_DIALOG);

  G_OBJECT_CLASS (capsule_close_dialog_parent_class)->dispose (object);
}

static void
capsule_close_dialog_class_init (CapsuleCloseDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = capsule_close_dialog_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Capsule/capsule-close-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CapsuleCloseDialog, group);
  gtk_widget_class_bind_template_child (widget_class, CapsuleCloseDialog, page);

  gtk_widget_class_bind_template_callback (widget_class, capsule_close_dialog_response_cancel_cb);
  gtk_widget_class_bind_template_callback (widget_class, capsule_close_dialog_response_discard_cb);
}

static void
capsule_close_dialog_init (CapsuleCloseDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

CapsuleCloseDialog *
capsule_close_dialog_new (void)
{
  return g_object_new (CAPSULE_TYPE_CLOSE_DIALOG, NULL);
}
