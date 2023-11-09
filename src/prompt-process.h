/* prompt-process.h
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

G_BEGIN_DECLS

typedef enum _PromptProcessLeader
{
  PROMPT_PROCESS_LEADER_KIND_UNKNOWN,
  PROMPT_PROCESS_LEADER_KIND_SUPERUSER,
  PROMPT_PROCESS_LEADER_KIND_REMOTE,
  PROMPT_PROCESS_LEADER_KIND_CONTAINER,
} PromptProcessLeaderKind;

#define PROMPT_TYPE_PROCESS (prompt_process_get_type())

G_DECLARE_DERIVABLE_TYPE (PromptProcess, prompt_process, PROMPT, PROCESS, GObject)

struct _PromptProcessClass
{
  GObjectClass parent_class;

  void                     (*force_exit)        (PromptProcess        *self);
  PromptProcessLeaderKind  (*get_leader_kind)   (PromptProcess        *self);
  gboolean                 (*get_if_exited)     (PromptProcess        *self);
  gboolean                 (*get_if_signaled)   (PromptProcess        *self);
  int                      (*get_exit_status)   (PromptProcess        *self);
  int                      (*get_term_sig)      (PromptProcess        *self);
  gboolean                 (*has_leader)        (PromptProcess        *self);
  void                     (*wait_check_async)  (PromptProcess        *self,
                                                 GCancellable         *cancellable,
                                                 GAsyncReadyCallback   callback,
                                                 gpointer              user_data);
  gboolean                 (*wait_check_finish) (PromptProcess        *self,
                                                 GAsyncResult         *result,
                                                 GError              **error);
};

PromptProcess           *prompt_process_new               (GSubprocess          *subprocess,
                                                           VtePty               *pty);
void                     prompt_process_force_exit        (PromptProcess        *self);
PromptProcessLeaderKind  prompt_process_get_leader_kind   (PromptProcess        *self);
gboolean                 prompt_process_get_if_exited     (PromptProcess        *self);
gboolean                 prompt_process_get_if_signaled   (PromptProcess        *self);
int                      prompt_process_get_exit_status   (PromptProcess        *self);
int                      prompt_process_get_term_sig      (PromptProcess        *self);
gboolean                 prompt_process_has_leader        (PromptProcess        *self);
void                     prompt_process_wait_check_async  (PromptProcess        *self,
                                                           GCancellable         *cancellable,
                                                           GAsyncReadyCallback   callback,
                                                           gpointer              user_data);
gboolean                 prompt_process_wait_check_finish (PromptProcess        *self,
                                                           GAsyncResult         *result,
                                                           GError              **error);

G_END_DECLS
