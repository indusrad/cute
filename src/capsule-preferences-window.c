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

#include "capsule-preferences-window.h"

struct _CapsulePreferencesWindow
{
  AdwPreferencesWindow parent_instance;
};

G_DEFINE_FINAL_TYPE (CapsulePreferencesWindow, capsule_preferences_window, ADW_TYPE_PREFERENCES_WINDOW)

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

  object_class->dispose = capsule_preferences_window_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Capsule/capsule-preferences-window.ui");
}

static void
capsule_preferences_window_init (CapsulePreferencesWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

#if DEVELOPMENT_BUILD
  gtk_widget_add_css_class (GTK_WIDGET (self), "devel");
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
