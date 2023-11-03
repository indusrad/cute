/* capsule-util.h
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

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
  CAPSULE_PROCESS_KIND_HOST    = 0,
  CAPSULE_PROCESS_KIND_FLATPAK = 1,
} CapsuleProcessKind;

CapsuleProcessKind  capsule_get_process_kind    (void) G_GNUC_CONST;
const char * const *capsule_host_environ        (void) G_GNUC_CONST;
char               *capsule_path_expand         (const char *path);
char               *capsule_path_collapse       (const char *path);
gboolean            capsule_pty_create_producer (int         consumer_fd,
                                                 gboolean    nonblock);

G_END_DECLS
