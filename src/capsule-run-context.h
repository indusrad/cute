/* capsule-run-context.h
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
#include <vte/vte.h>

#include "capsule-unix-fd-map.h"

G_BEGIN_DECLS

#define CAPSULE_TYPE_RUN_CONTEXT (capsule_run_context_get_type())

G_DECLARE_FINAL_TYPE (CapsuleRunContext, capsule_run_context, CAPSULE, RUN_CONTEXT, GObject)

/**
 * CapsuleRunContextShell:
 * @CAPSULE_RUN_CONTEXT_SHELL_DEFAULT: A basic shell with no user scripts
 * @CAPSULE_RUN_CONTEXT_SHELL_LOGIN: A user login shell similar to `bash -l`
 * @CAPSULE_RUN_CONTEXT_SHELL_INTERACTIVE: A user interactive shell similar to `bash -i`
 *
 * Describes the type of shell to be used within the context.
 */
typedef enum _CapsuleRunContextShell
{
  CAPSULE_RUN_CONTEXT_SHELL_DEFAULT     = 0,
  CAPSULE_RUN_CONTEXT_SHELL_LOGIN       = 1,
  CAPSULE_RUN_CONTEXT_SHELL_INTERACTIVE = 2,
} CapsuleRunContextShell;

/**
 * CapsuleRunContextHandler:
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error must be set.
 */
typedef gboolean (*CapsuleRunContextHandler) (CapsuleRunContext   *run_context,
                                              const char * const  *argv,
                                              const char * const  *env,
                                              const char          *cwd,
                                              CapsuleUnixFDMap    *unix_fd_map,
                                              gpointer             user_data,
                                              GError             **error);

CapsuleRunContext     *capsule_run_context_new                     (void);
void                   capsule_run_context_push                    (CapsuleRunContext         *self,
                                                                    CapsuleRunContextHandler   handler,
                                                                    gpointer                   handler_data,
                                                                    GDestroyNotify             handler_data_destroy);
void                   capsule_run_context_push_at_base            (CapsuleRunContext         *self,
                                                                    CapsuleRunContextHandler   handler,
                                                                    gpointer                   handler_data,
                                                                    GDestroyNotify             handler_data_destroy);
void                   capsule_run_context_push_error              (CapsuleRunContext         *self,
                                                                    GError                    *error);
void                   capsule_run_context_push_host               (CapsuleRunContext         *self);
void                   capsule_run_context_push_shell              (CapsuleRunContext         *self,
                                                                    CapsuleRunContextShell     shell);
void                   capsule_run_context_push_expansion          (CapsuleRunContext         *self,
                                                                    const char * const        *environ);
const char * const    *capsule_run_context_get_argv                (CapsuleRunContext         *self);
void                   capsule_run_context_set_argv                (CapsuleRunContext         *self,
                                                                    const char * const        *argv);
const char * const    *capsule_run_context_get_environ             (CapsuleRunContext         *self);
void                   capsule_run_context_set_environ             (CapsuleRunContext         *self,
                                                                    const char * const        *environ);
void                   capsule_run_context_add_environ             (CapsuleRunContext         *self,
                                                                    const char * const        *environ);
void                   capsule_run_context_add_minimal_environment (CapsuleRunContext         *self);
void                   capsule_run_context_environ_to_argv         (CapsuleRunContext         *self);
const char            *capsule_run_context_get_cwd                 (CapsuleRunContext         *self);
void                   capsule_run_context_set_cwd                 (CapsuleRunContext         *self,
                                                                    const char                *cwd);
void                   capsule_run_context_set_pty_fd              (CapsuleRunContext         *self,
                                                                    int                        consumer_fd);
void                   capsule_run_context_set_pty                 (CapsuleRunContext         *self,
                                                                    VtePty                    *pty);
void                   capsule_run_context_take_fd                 (CapsuleRunContext         *self,
                                                                    int                        source_fd,
                                                                    int                        dest_fd);
gboolean               capsule_run_context_merge_unix_fd_map       (CapsuleRunContext         *self,
                                                                    CapsuleUnixFDMap          *unix_fd_map,
                                                                    GError                   **error);
void                   capsule_run_context_prepend_argv            (CapsuleRunContext         *self,
                                                                    const char                *arg);
void                   capsule_run_context_prepend_args            (CapsuleRunContext         *self,
                                                                    const char * const        *args);
void                   capsule_run_context_append_argv             (CapsuleRunContext         *self,
                                                                    const char                *arg);
void                   capsule_run_context_append_args             (CapsuleRunContext         *self,
                                                                    const char * const        *args);
gboolean               capsule_run_context_append_args_parsed      (CapsuleRunContext         *self,
                                                                    const char                *args,
                                                                    GError                   **error);
void                   capsule_run_context_append_formatted        (CapsuleRunContext         *self,
                                                                    const char                *format,
                                                                    ...) G_GNUC_PRINTF (2, 3);
const char            *capsule_run_context_getenv                  (CapsuleRunContext         *self,
                                                                    const char                *key);
void                   capsule_run_context_setenv                  (CapsuleRunContext         *self,
                                                                    const char                *key,
                                                                    const char                *value);
void                   capsule_run_context_unsetenv                (CapsuleRunContext         *self,
                                                                    const char                *key);
GIOStream             *capsule_run_context_create_stdio_stream     (CapsuleRunContext         *self,
                                                                    GError                   **error);
GSubprocessLauncher   *capsule_run_context_end                     (CapsuleRunContext         *self,
                                                                    GError                   **error);
GSubprocess           *capsule_run_context_spawn                   (CapsuleRunContext         *self,
                                                                    GError                   **error);

G_END_DECLS
