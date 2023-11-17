/* prompt-unix-fd-map.h
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

#define PROMPT_TYPE_UNIX_FD_MAP (prompt_unix_fd_map_get_type())

G_DECLARE_FINAL_TYPE (PromptUnixFDMap, prompt_unix_fd_map, PROMPT, UNIX_FD_MAP, GObject)

PromptUnixFDMap *prompt_unix_fd_map_new             (void);
guint            prompt_unix_fd_map_get_length      (PromptUnixFDMap  *self);
int              prompt_unix_fd_map_peek_stdin      (PromptUnixFDMap  *self);
int              prompt_unix_fd_map_peek_stdout     (PromptUnixFDMap  *self);
int              prompt_unix_fd_map_peek_stderr     (PromptUnixFDMap  *self);
int              prompt_unix_fd_map_steal_stdin     (PromptUnixFDMap  *self);
int              prompt_unix_fd_map_steal_stdout    (PromptUnixFDMap  *self);
int              prompt_unix_fd_map_steal_stderr    (PromptUnixFDMap  *self);
gboolean         prompt_unix_fd_map_steal_from      (PromptUnixFDMap  *self,
                                                     PromptUnixFDMap  *other,
                                                     GError          **error);
int              prompt_unix_fd_map_peek            (PromptUnixFDMap  *self,
                                                     guint             index,
                                                     int              *dest_fd);
int              prompt_unix_fd_map_get             (PromptUnixFDMap  *self,
                                                     guint             index,
                                                     int              *dest_fd,
                                                     GError          **error);
int              prompt_unix_fd_map_steal           (PromptUnixFDMap  *self,
                                                     guint             index,
                                                     int              *dest_fd);
void             prompt_unix_fd_map_take            (PromptUnixFDMap  *self,
                                                     int               source_fd,
                                                     int               dest_fd);
gboolean         prompt_unix_fd_map_open_file       (PromptUnixFDMap  *self,
                                                     const char       *filename,
                                                     int               mode,
                                                     int               dest_fd,
                                                     GError          **error);
int              prompt_unix_fd_map_get_max_dest_fd (PromptUnixFDMap  *self);
gboolean         prompt_unix_fd_map_stdin_isatty    (PromptUnixFDMap  *self);
gboolean         prompt_unix_fd_map_stdout_isatty   (PromptUnixFDMap  *self);
gboolean         prompt_unix_fd_map_stderr_isatty   (PromptUnixFDMap  *self);
GIOStream       *prompt_unix_fd_map_create_stream   (PromptUnixFDMap  *self,
                                                     int               dest_read_fd,
                                                     int               dest_write_fd,
                                                     GError          **error);
gboolean         prompt_unix_fd_map_silence_fd      (PromptUnixFDMap  *self,
                                                     int               dest_fd,
                                                     GError          **error);

G_END_DECLS
