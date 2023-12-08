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

#include "prompt-agent-compat.h"
#include "prompt-agent-util.h"
#include "prompt-process-impl.h"
#include "prompt-session-container.h"

struct _PromptSessionContainer
{
  PromptIpcContainerSkeleton parent_instance;
  char **command_prefix;
};

static void container_iface_init (PromptIpcContainerIface *iface);

G_DEFINE_TYPE_WITH_CODE (PromptSessionContainer, prompt_session_container, PROMPT_IPC_TYPE_CONTAINER_SKELETON,
                         G_IMPLEMENT_INTERFACE (PROMPT_IPC_TYPE_CONTAINER, container_iface_init))

static void
prompt_session_container_finalize (GObject *object)
{
  PromptSessionContainer *self = (PromptSessionContainer *)object;

  g_clear_pointer (&self->command_prefix, g_strfreev);

  G_OBJECT_CLASS (prompt_session_container_parent_class)->finalize (object);
}

static void
prompt_session_container_class_init (PromptSessionContainerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = prompt_session_container_finalize;
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
prompt_session_container_handle_spawn (PromptIpcContainer    *container,
                                       GDBusMethodInvocation *invocation,
                                       GUnixFDList           *in_fd_list,
                                       const char            *cwd,
                                       const char * const    *argv,
                                       GVariant              *in_fds,
                                       GVariant              *in_env)
{
  PromptSessionContainer *self = (PromptSessionContainer *)container;
  g_autoptr(PromptRunContext) run_context = NULL;
  g_autoptr(PromptIpcProcess) process = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GUnixFDList) out_fd_list = NULL;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) env = NULL;
  GDBusConnection *connection;
  g_autofree char *object_path = NULL;
  g_autofree char *guid = NULL;

  g_assert (PROMPT_IS_SESSION_CONTAINER (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (G_IS_UNIX_FD_LIST (in_fd_list));
  g_assert (cwd != NULL);
  g_assert (argv != NULL);
  g_assert (in_fds != NULL);
  g_assert (in_env != NULL);

  /* Make sure CWD exists within the user session, it might have
   * come from another container that isn't the same or at a path
   * that is not accessible to the user (say from a sudo shell).
   */
  if (cwd[0] == 0 || !g_file_test (cwd, G_FILE_TEST_IS_DIR))
    cwd = g_get_home_dir ();

  run_context = prompt_run_context_new ();

  /* Place the process inside a new scope similar to what VTE would do. */
  prompt_run_context_push_scope (run_context);

  /* For the default session, we'll just inherit whatever the session gave us
   * as our environment. For other types of containers, that may be different
   * as you likely want to filter some stateful things out.
   */
  env = g_get_environ ();
  prompt_run_context_set_environ (run_context, (const char * const *)env);

  /* If a command prefix was specified, add that now */
  if (self->command_prefix != NULL)
    prompt_run_context_append_args (run_context, (const char * const *)self->command_prefix);

  /* Use the spawn helper to copy everything that matters after marshaling
   * out of GVariant format. This will be very much the same for other
   * container providers.
   */
  prompt_agent_push_spawn (run_context, in_fd_list, cwd, argv, in_fds, in_env);

  /* Spawn and export our object to the bus. Note that a weak reference is used
   * for the object on the bus so you must keep the object alive to ensure that
   * it is not removed fro the bus. The default PromptProcessImpl does that for
   * us by automatically waiting for the child to exit.
   */
  guid = g_dbus_generate_guid ();
  object_path = g_strdup_printf ("/org/gnome/Prompt/Process/%s", guid);
  out_fd_list = g_unix_fd_list_new ();
  connection = g_dbus_method_invocation_get_connection (invocation);
  if (!(subprocess = prompt_run_context_spawn (run_context, &error)) ||
      !(process = prompt_process_impl_new (connection, subprocess, object_path, &error)))
    g_dbus_method_invocation_return_gerror (g_steal_pointer (&invocation), error);
  else
    prompt_ipc_container_complete_spawn (container,
                                         g_steal_pointer (&invocation),
                                         out_fd_list,
                                         object_path);

  return TRUE;
}

static void
container_iface_init (PromptIpcContainerIface *iface)
{
  iface->handle_spawn = prompt_session_container_handle_spawn;
}

void
prompt_session_container_set_command_prefix (PromptSessionContainer *self,
                                             const char * const     *command_prefix)
{
  char **copy;

  g_return_if_fail (PROMPT_IS_SESSION_CONTAINER (self));

  copy = g_strdupv ((char **)command_prefix);
  g_strfreev (self->command_prefix);
  self->command_prefix = copy;
}
