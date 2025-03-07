/*
 * ptyxis-preferences-window.h
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

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define PTYXIS_TYPE_PREFERENCES_WINDOW (ptyxis_preferences_window_get_type())

G_DECLARE_FINAL_TYPE (PtyxisPreferencesWindow, ptyxis_preferences_window, PTYXIS, PREFERENCES_WINDOW, AdwPreferencesWindow)

PtyxisPreferencesWindow *ptyxis_preferences_window_get_default    (void);
GtkWindow               *ptyxis_preferences_window_new            (GtkApplication          *application);
void                     ptyxis_preferences_window_edit_profile   (PtyxisPreferencesWindow *self,
                                                                   PtyxisProfile           *profile);
void                     ptyxis_preferences_window_edit_shortcuts (PtyxisPreferencesWindow *self);

G_END_DECLS
