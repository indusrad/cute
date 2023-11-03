/* capsule-unix-fd-map.h
 *
 * Copyright 2022-2023 Christian Hergert <chergert@redhat.com>
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

#define CAPSULE_TYPE_UNIX_FD_MAP (capsule_unix_fd_map_get_type())

G_DECLARE_FINAL_TYPE (CapsuleUnixFDMap, capsule_unix_fd_map, CAPSULE, UNIX_FD_MAP, GObject)

CapsuleUnixFDMap *capsule_unix_fd_map_new             (void);
guint             capsule_unix_fd_map_get_length      (CapsuleUnixFDMap  *self);
int               capsule_unix_fd_map_peek_stdin      (CapsuleUnixFDMap  *self);
int               capsule_unix_fd_map_peek_stdout     (CapsuleUnixFDMap  *self);
int               capsule_unix_fd_map_peek_stderr     (CapsuleUnixFDMap  *self);
int               capsule_unix_fd_map_steal_stdin     (CapsuleUnixFDMap  *self);
int               capsule_unix_fd_map_steal_stdout    (CapsuleUnixFDMap  *self);
int               capsule_unix_fd_map_steal_stderr    (CapsuleUnixFDMap  *self);
gboolean          capsule_unix_fd_map_steal_from      (CapsuleUnixFDMap  *self,
                                                       CapsuleUnixFDMap  *other,
                                                       GError           **error);
int               capsule_unix_fd_map_peek            (CapsuleUnixFDMap  *self,
                                                       guint              index,
                                                       int               *dest_fd);
int               capsule_unix_fd_map_get             (CapsuleUnixFDMap  *self,
                                                       guint              index,
                                                       int               *dest_fd,
                                                       GError           **error);
int               capsule_unix_fd_map_steal           (CapsuleUnixFDMap  *self,
                                                       guint              index,
                                                       int               *dest_fd);
void              capsule_unix_fd_map_take            (CapsuleUnixFDMap  *self,
                                                       int                source_fd,
                                                       int                dest_fd);
gboolean          capsule_unix_fd_map_open_file       (CapsuleUnixFDMap  *self,
                                                       const char        *filename,
                                                       int                mode,
                                                       int                dest_fd,
                                                       GError           **error);
int               capsule_unix_fd_map_get_max_dest_fd (CapsuleUnixFDMap  *self);
gboolean          capsule_unix_fd_map_stdin_isatty    (CapsuleUnixFDMap  *self);
gboolean          capsule_unix_fd_map_stdout_isatty   (CapsuleUnixFDMap  *self);
gboolean          capsule_unix_fd_map_stderr_isatty   (CapsuleUnixFDMap  *self);
GIOStream        *capsule_unix_fd_map_create_stream   (CapsuleUnixFDMap  *self,
                                                       int                dest_read_fd,
                                                       int                dest_write_fd,
                                                       GError           **error);
gboolean          capsule_unix_fd_map_silence_fd      (CapsuleUnixFDMap  *self,
                                                       int                dest_fd,
                                                       GError           **error);

G_END_DECLS
