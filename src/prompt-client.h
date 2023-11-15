/*
 * prompt-client.h
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
#include <vte/vte.h>

#include "prompt-agent-ipc.h"

#include "prompt-profile.h"

G_BEGIN_DECLS

#define PROMPT_TYPE_CLIENT (prompt_client_get_type())

G_DECLARE_FINAL_TYPE (PromptClient, prompt_client, PROMPT, CLIENT, GObject)

PromptClient     *prompt_client_new                   (GError              **error);
void              prompt_client_force_exit            (PromptClient         *self);
VtePty           *prompt_client_create_pty            (PromptClient         *self,
                                                       GError              **error);
void              prompt_client_discover_shell_async  (PromptClient         *self,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);
char             *prompt_client_discover_shell_finish (PromptClient         *client,
                                                       GAsyncResult         *result,
                                                       GError              **error);
void              prompt_client_spawn_async           (PromptClient         *self,
                                                       PromptIpcContainer   *container,
                                                       PromptProfile        *profile,
                                                       const char           *default_shell,
                                                       const char           *last_working_directory_uri,
                                                       VtePty               *pty,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);
PromptIpcProcess *prompt_client_spawn_finish          (PromptClient         *self,
                                                       GAsyncResult         *result,
                                                       GError              **error);

G_END_DECLS
