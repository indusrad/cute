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

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "prompt-agent-compat.h"
#include "prompt-process-impl.h"

struct _PromptProcessImpl
{
  PromptIpcProcessSkeleton parent_instance;
  GSubprocess *subprocess;
  int pty_fd;
};

static void process_iface_init (PromptIpcProcessIface *iface);

G_DEFINE_TYPE_WITH_CODE (PromptProcessImpl, prompt_process_impl, PROMPT_IPC_TYPE_PROCESS_SKELETON,
                         G_IMPLEMENT_INTERFACE (PROMPT_IPC_TYPE_PROCESS, process_iface_init))

static GHashTable *exec_to_kind;

static void
prompt_process_impl_finalize (GObject *object)
{
  PromptProcessImpl *self = (PromptProcessImpl *)object;

  g_clear_object (&self->subprocess);
  _g_clear_fd (&self->pty_fd, NULL);

  G_OBJECT_CLASS (prompt_process_impl_parent_class)->finalize (object);
}

static void
prompt_process_impl_class_init (PromptProcessImplClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = prompt_process_impl_finalize;

  exec_to_kind = g_hash_table_new (g_str_hash, g_str_equal);
#define ADD_MAPPING(name, kind) \
    g_hash_table_insert (exec_to_kind, (char *)name, (char *)kind)
  ADD_MAPPING ("docker", "container");
  ADD_MAPPING ("flatpak", "container");
  ADD_MAPPING ("podman", "container");
  ADD_MAPPING ("rlogin", "remote");
  ADD_MAPPING ("scp", "remote");
  ADD_MAPPING ("sftp", "remote");
  ADD_MAPPING ("slogin", "remote");
  ADD_MAPPING ("ssh", "remote");
  ADD_MAPPING ("telnet", "remote");
  ADD_MAPPING ("toolbox", "container");
#undef ADD_MAPPING
}

static void
prompt_process_impl_init (PromptProcessImpl *self)
{
  self->pty_fd = -1;
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

  g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (self));

  g_clear_object (&self->subprocess);
}

PromptIpcProcess *
prompt_process_impl_new (GDBusConnection  *connection,
                         GSubprocess      *subprocess,
                         int               pty_fd,
                         const char       *object_path,
                         GError          **error)
{
  g_autoptr(PromptProcessImpl) self = NULL;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);
  g_return_val_if_fail (G_IS_SUBPROCESS (subprocess), NULL);
  g_return_val_if_fail (object_path != NULL, NULL);

  self = g_object_new (PROMPT_TYPE_PROCESS_IMPL, NULL);
  self->pty_fd = pty_fd;
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

static gboolean
prompt_process_impl_handle_send_signal (PromptIpcProcess      *process,
                                        GDBusMethodInvocation *invocation,
                                        int                    signum)
{
  PromptProcessImpl *self = (PromptProcessImpl *)process;

  g_assert (PROMPT_IS_PROCESS_IMPL (process));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  if (self->subprocess != NULL)
    g_subprocess_send_signal (self->subprocess, signum);

  prompt_ipc_process_complete_send_signal (process, g_steal_pointer (&invocation));

  return TRUE;
}

static gboolean
prompt_process_impl_handle_get_leader_kind (PromptIpcProcess      *process,
                                            GDBusMethodInvocation *invocation)
{
  PromptProcessImpl *self = (PromptProcessImpl *)process;
  const char *leader_kind = NULL;

  g_assert (PROMPT_IS_PROCESS_IMPL (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  if (self->pty_fd != -1)
    {
      GPid pid = tcgetpgrp (self->pty_fd);

      if (pid > 0)
        {
          g_autofree char *path = g_strdup_printf ("/proc/%d/", pid);
          g_autofree char *exe = g_strdup_printf ("/proc/%d/exe", pid);
          char execpath[512];
          gssize len;
          struct stat st;

          if (stat (path, &st) == 0)
            {
              if (st.st_uid == 0)
                {
                  leader_kind = "superuser";
                  goto complete;
                }
            }

          len = readlink (exe, execpath, sizeof execpath-1);

          if (len > 0)
            {
              const char *end;

              execpath[len] = 0;

              if ((end = strrchr (execpath, '/')))
                leader_kind = g_hash_table_lookup (exec_to_kind, end+1);
            }
        }
    }

complete:
  if (leader_kind == NULL)
    leader_kind = "unknown";

  prompt_ipc_process_complete_get_leader_kind (process,
                                               g_steal_pointer (&invocation),
                                               leader_kind);

  return TRUE;
}

static void
process_iface_init (PromptIpcProcessIface *iface)
{
  iface->handle_send_signal = prompt_process_impl_handle_send_signal;
  iface->handle_get_leader_kind = prompt_process_impl_handle_get_leader_kind;
}
