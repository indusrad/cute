/*
 * prompt-session-container.c
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

#include "prompt-agent-util.h"
#include "prompt-process-impl.h"
#include "prompt-session-container.h"

struct _PromptSessionContainer
{
  PromptIpcContainerSkeleton parent_instance;
};

static void container_iface_init (PromptIpcContainerIface *iface);

G_DEFINE_TYPE_WITH_CODE (PromptSessionContainer, prompt_session_container, PROMPT_IPC_TYPE_CONTAINER_SKELETON,
                         G_IMPLEMENT_INTERFACE (PROMPT_IPC_TYPE_CONTAINER, container_iface_init))

static void
prompt_session_container_class_init (PromptSessionContainerClass *klass)
{
}

static void
prompt_session_container_init (PromptSessionContainer *self)
{
  prompt_ipc_container_set_id (PROMPT_IPC_CONTAINER (self), "session");
  prompt_ipc_container_set_provider (PROMPT_IPC_CONTAINER (self), "session");
}

PromptSessionContainer *
prompt_session_container_new (void)
{
  return g_object_new (PROMPT_TYPE_SESSION_CONTAINER, NULL);
}

static gboolean
prompt_session_container_handle_create_pty (PromptIpcContainer    *container,
                                            GDBusMethodInvocation *invocation,
                                            GUnixFDList           *in_fd_list)
{
  g_autoptr(GUnixFDList) out_fd_list = NULL;
  g_autoptr(GError) error = NULL;
  int pty_fd;

  g_assert (PROMPT_IPC_IS_CONTAINER (container));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (!in_fd_list || G_IS_UNIX_FD_LIST (in_fd_list));

  if (-1 == (pty_fd = prompt_agent_pty_new (&error)))
    {
      g_dbus_method_invocation_return_gerror (g_steal_pointer (&invocation), error);
      return TRUE;
    }

  out_fd_list = g_unix_fd_list_new_from_array (&pty_fd, 1);
  prompt_ipc_container_complete_create_pty (container,
                                            g_steal_pointer (&invocation),
                                            out_fd_list,
                                            g_variant_new_handle (0));

  return TRUE;
}

static gboolean
prompt_session_container_handle_spawn (PromptIpcContainer    *container,
                                       GDBusMethodInvocation *invocation,
                                       GUnixFDList           *in_fd_list,
                                       const char            *cwd,
                                       const char * const    *argv,
                                       GVariant              *fds,
                                       GVariant              *env)
{
  g_autoptr(PromptRunContext) run_context = NULL;
  g_autoptr(PromptIpcProcess) process = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GUnixFDList) out_fd_list = NULL;
  g_autoptr(GError) error = NULL;
  GDBusConnection *connection;
  g_autofree char *object_path = NULL;
  g_autofree char *guid = NULL;

  g_assert (PROMPT_IPC_IS_CONTAINER (container));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (G_IS_UNIX_FD_LIST (in_fd_list));
  g_assert (cwd != NULL);
  g_assert (argv != NULL);
  g_assert (fds != NULL);
  g_assert (env != NULL);

  run_context = prompt_run_context_new ();
  prompt_agent_push_spawn (run_context, in_fd_list, cwd, argv, fds, env);

  guid = g_dbus_generate_guid ();
  object_path = g_strdup_printf ("/org/gnome/Prompt/Process/%s", guid);
  out_fd_list = g_unix_fd_list_new ();
  connection = g_dbus_method_invocation_get_connection (invocation);

  if (!(subprocess = prompt_run_context_spawn (run_context, &error)) ||
      !(process = prompt_process_impl_new (connection, subprocess, object_path, &error)))
    {
      g_dbus_method_invocation_return_gerror (g_steal_pointer (&invocation), error);
      return TRUE;
    }

  prompt_ipc_container_complete_spawn (container,
                                       g_steal_pointer (&invocation),
                                       out_fd_list,
                                       object_path);

  return TRUE;
}

static void
container_iface_init (PromptIpcContainerIface *iface)
{
  iface->handle_create_pty = prompt_session_container_handle_create_pty;
  iface->handle_spawn = prompt_session_container_handle_spawn;
}
