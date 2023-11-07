/* capsule-shortcut-accel-dialog.h
 *
 * Copyright 2017-2023 Christian Hergert <chergert@redhat.com>
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

#define CAPSULE_TYPE_SHORTCUT_ACCEL_DIALOG (capsule_shortcut_accel_dialog_get_type())

G_DECLARE_FINAL_TYPE (CapsuleShortcutAccelDialog, capsule_shortcut_accel_dialog, CAPSULE, SHORTCUT_ACCEL_DIALOG, AdwWindow)

GtkWidget              *capsule_shortcut_accel_dialog_new                (void);
char                   *capsule_shortcut_accel_dialog_get_accelerator    (CapsuleShortcutAccelDialog *self);
void                    capsule_shortcut_accel_dialog_set_accelerator    (CapsuleShortcutAccelDialog *self,
                                                                          const char                 *accelerator);
const char             *capsule_shortcut_accel_dialog_get_shortcut_title (CapsuleShortcutAccelDialog *self);
void                    capsule_shortcut_accel_dialog_set_shortcut_title (CapsuleShortcutAccelDialog *self,
                                                                          const char                 *title);

G_END_DECLS

