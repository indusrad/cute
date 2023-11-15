/*
 * prompt-process-impl.c
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

#include "config.h"

#include "prompt-process-impl.h"

struct _PromptProcessImpl
{
  PromptIpcProcessSkeleton parent_instance;
  GSubprocess *subprocess;
};

G_DEFINE_TYPE (PromptProcessImpl, prompt_process_impl, PROMPT_IPC_TYPE_PROCESS)

static void
prompt_process_impl_finalize (GObject *object)
{
  PromptProcessImpl *self = (PromptProcessImpl *)object;

  g_clear_object (&self->subprocess);

  G_OBJECT_CLASS (prompt_process_impl_parent_class)->finalize (object);
}

static void
prompt_process_impl_class_init (PromptProcessImplClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = prompt_process_impl_finalize;
}

static void
prompt_process_impl_init (PromptProcessImpl *self)
{
}

static void
prompt_process_impl_wait_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  GSubprocess *subprocess = (GSubprocess *)object;
  g_autoptr(PromptProcessImpl) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));

  g_subprocess_wait_finish (subprocess, result, &error);

  if (g_subprocess_get_if_signaled (subprocess))
    prompt_ipc_process_emit_signaled (PROMPT_IPC_PROCESS (self),
                                      g_subprocess_get_term_sig (subprocess));
  else
    prompt_ipc_process_emit_exited (PROMPT_IPC_PROCESS (self),
                                    g_subprocess_get_exit_status (subprocess));
}

PromptIpcProcess *
prompt_process_impl_new (GDBusConnection  *connection,
                         GSubprocess      *subprocess,
                         const char       *object_path,
                         GError          **error)
{
  g_autoptr(PromptProcessImpl) self = NULL;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);
  g_return_val_if_fail (G_IS_SUBPROCESS (subprocess), NULL);
  g_return_val_if_fail (object_path != NULL, NULL);

  self = g_object_new (PROMPT_TYPE_PROCESS_IMPL, NULL);
  g_set_object (&self->subprocess, subprocess);

  g_subprocess_wait_async (subprocess,
                           NULL,
                           prompt_process_impl_wait_cb,
                           g_object_ref (self));

  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self),
                                         connection,
                                         object_path,
                                         error))
    return NULL;

  return g_steal_pointer (&self);
}
