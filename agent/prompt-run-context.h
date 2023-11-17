/* prompt-run-context.h
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

#include "prompt-unix-fd-map.h"

G_BEGIN_DECLS

#define PROMPT_TYPE_RUN_CONTEXT (prompt_run_context_get_type())

G_DECLARE_FINAL_TYPE (PromptRunContext, prompt_run_context, PROMPT, RUN_CONTEXT, GObject)

/**
 * PromptRunContextShell:
 * @PROMPT_RUN_CONTEXT_SHELL_DEFAULT: A basic shell with no user scripts
 * @PROMPT_RUN_CONTEXT_SHELL_LOGIN: A user login shell similar to `bash -l`
 * @PROMPT_RUN_CONTEXT_SHELL_INTERACTIVE: A user interactive shell similar to `bash -i`
 *
 * Describes the type of shell to be used within the context.
 */
typedef enum _PromptRunContextShell
{
  PROMPT_RUN_CONTEXT_SHELL_DEFAULT     = 0,
  PROMPT_RUN_CONTEXT_SHELL_LOGIN       = 1,
  PROMPT_RUN_CONTEXT_SHELL_INTERACTIVE = 2,
} PromptRunContextShell;

/**
 * PromptRunContextHandler:
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error must be set.
 */
typedef gboolean (*PromptRunContextHandler) (PromptRunContext    *run_context,
                                             const char * const  *argv,
                                             const char * const  *env,
                                             const char          *cwd,
                                             PromptUnixFDMap     *unix_fd_map,
                                             gpointer             user_data,
                                             GError             **error);

PromptRunContext    *prompt_run_context_new                     (void);
void                 prompt_run_context_push                    (PromptRunContext         *self,
                                                                 PromptRunContextHandler   handler,
                                                                 gpointer                  handler_data,
                                                                 GDestroyNotify            handler_data_destroy);
void                 prompt_run_context_push_at_base            (PromptRunContext         *self,
                                                                 PromptRunContextHandler   handler,
                                                                 gpointer                  handler_data,
                                                                 GDestroyNotify            handler_data_destroy);
void                 prompt_run_context_push_error              (PromptRunContext         *self,
                                                                 GError                   *error);
void                 prompt_run_context_push_shell              (PromptRunContext         *self,
                                                                 PromptRunContextShell     shell);
const char * const  *prompt_run_context_get_argv                (PromptRunContext         *self);
void                 prompt_run_context_set_argv                (PromptRunContext         *self,
                                                                 const char * const       *argv);
const char * const  *prompt_run_context_get_environ             (PromptRunContext         *self);
void                 prompt_run_context_set_environ             (PromptRunContext         *self,
                                                                 const char * const       *environ);
void                 prompt_run_context_add_environ             (PromptRunContext         *self,
                                                                 const char * const       *environ);
void                 prompt_run_context_add_minimal_environment (PromptRunContext         *self);
void                 prompt_run_context_environ_to_argv         (PromptRunContext         *self);
const char          *prompt_run_context_get_cwd                 (PromptRunContext         *self);
void                 prompt_run_context_set_cwd                 (PromptRunContext         *self,
                                                                 const char               *cwd);
void                 prompt_run_context_take_fd                 (PromptRunContext         *self,
                                                                 int                       source_fd,
                                                                 int                       dest_fd);
gboolean             prompt_run_context_merge_unix_fd_map       (PromptRunContext         *self,
                                                                 PromptUnixFDMap          *unix_fd_map,
                                                                 GError                  **error);
void                 prompt_run_context_prepend_argv            (PromptRunContext         *self,
                                                                 const char               *arg);
void                 prompt_run_context_prepend_args            (PromptRunContext         *self,
                                                                 const char * const       *args);
void                 prompt_run_context_append_argv             (PromptRunContext         *self,
                                                                 const char               *arg);
void                 prompt_run_context_append_args             (PromptRunContext         *self,
                                                                 const char * const       *args);
gboolean             prompt_run_context_append_args_parsed      (PromptRunContext         *self,
                                                                 const char               *args,
                                                                 GError                  **error);
void                 prompt_run_context_append_formatted        (PromptRunContext         *self,
                                                                 const char               *format,
                                                                 ...) G_GNUC_PRINTF (2, 3);
const char          *prompt_run_context_getenv                  (PromptRunContext         *self,
                                                                 const char               *key);
void                 prompt_run_context_setenv                  (PromptRunContext         *self,
                                                                 const char               *key,
                                                                 const char               *value);
void                 prompt_run_context_unsetenv                (PromptRunContext         *self,
                                                                 const char               *key);
GIOStream           *prompt_run_context_create_stdio_stream     (PromptRunContext         *self,
                                                                 GError                  **error);
GSubprocessLauncher *prompt_run_context_end                     (PromptRunContext         *self,
                                                                 GError                  **error);
GSubprocess         *prompt_run_context_spawn                   (PromptRunContext         *self,
                                                                 GError                  **error);

G_END_DECLS
