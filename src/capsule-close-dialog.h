/*
 * capsule-close-dialog.h
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

#include "capsule-tab.h"

G_BEGIN_DECLS

#define CAPSULE_TYPE_CLOSE_DIALOG (capsule_close_dialog_get_type())

G_DECLARE_FINAL_TYPE (CapsuleCloseDialog, capsule_close_dialog, CAPSULE, CLOSE_DIALOG, AdwMessageDialog)

CapsuleCloseDialog *capsule_close_dialog_new            (void);
void                capsule_close_dialog_confirm        (CapsuleCloseDialog   *self,
                                                         CapsuleTab           *tab,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
gboolean            capsule_close_dialog_confirm_finish (CapsuleCloseDialog   *self,
                                                         GAsyncResult         *result,
                                                         GError              **error);

G_END_DECLS
