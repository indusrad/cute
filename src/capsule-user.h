/* capsule-user.h
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

#include <gio/gio.h>

G_BEGIN_DECLS

#define CAPSULE_TYPE_USER (capsule_user_get_type())

G_DECLARE_FINAL_TYPE (CapsuleUser, capsule_user, CAPSULE, USER, GObject)

CapsuleUser *capsule_user_get_default           (void);
void         capsule_user_discover_shell_async  (CapsuleUser          *self,
                                                 GCancellable         *cancellable,
                                                 GAsyncReadyCallback   callback,
                                                 gpointer              user_data);
char        *capsule_user_discover_shell_finish (CapsuleUser          *self,
                                                 GAsyncResult         *result,
                                                 GError              **error);

G_END_DECLS
