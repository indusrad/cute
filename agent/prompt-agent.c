/*
 * prompt-agent.c
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

#ifdef LIBC_COMPAT
# include "libc-compat.h"
#endif

#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <glib-unix.h>
#include <glib/gstdio.h>

#include <gio/gio.h>

#include "prompt-agent-impl.h"
#include "prompt-distrobox-container.h"
#include "prompt-podman-provider.h"
#include "prompt-session-container.h"
#include "prompt-toolbox-container.h"

typedef struct _PromptAgent
{
  PromptAgentImpl   *impl;
  GSocket           *socket;
  GSocketConnection *stream;
  GDBusConnection   *bus;
  GMainLoop         *main_loop;
  int                exit_code;
} PromptAgent;

G_GNUC_UNUSED
static void
prompt_agent_quit (PromptAgent *agent,
                   int          exit_code)
{
  agent->exit_code = exit_code;
  g_main_loop_quit (agent->main_loop);
}

static gboolean
prompt_agent_init (PromptAgent  *agent,
                   int           socket_fd,
                   GError      **error)
{
  g_autoptr(PromptSessionContainer) session = NULL;
  g_autoptr(PromptContainerProvider) podman = NULL;

  memset (agent, 0, sizeof *agent);

  if (socket_fd <= 2)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVAL,
                   "socket-fd must be set to a FD > 2");
      return FALSE;
    }

  agent->main_loop = g_main_loop_new (NULL, FALSE);

  if (!(agent->socket = g_socket_new_from_fd (socket_fd, error)))
    {
      close (socket_fd);
      return FALSE;
    }

  agent->stream = g_socket_connection_factory_create_connection (agent->socket);

  g_assert (agent->stream != NULL);
  g_assert (G_IS_SOCKET_CONNECTION (agent->stream));

  if (!(agent->bus = g_dbus_connection_new_sync (G_IO_STREAM (agent->stream),
                                                 NULL,
                                                 (G_DBUS_CONNECTION_FLAGS_DELAY_MESSAGE_PROCESSING |
                                                  G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT),
                                                 NULL,
                                                 NULL,
                                                 error)) ||
      !(agent->impl = prompt_agent_impl_new (error)) ||
      !g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (agent->impl),
                                         agent->bus,
                                         "/org/gnome/Prompt/Agent",
                                         error))
    return FALSE;

  session = prompt_session_container_new ();
  prompt_agent_impl_add_container (agent->impl, PROMPT_IPC_CONTAINER (session));

  podman = prompt_podman_provider_new ();
  prompt_podman_provider_set_type_for_label (PROMPT_PODMAN_PROVIDER (podman),
                                             "com.github.containers.toolbox",
                                             NULL,
                                             PROMPT_TYPE_TOOLBOX_CONTAINER);
  prompt_podman_provider_set_type_for_label (PROMPT_PODMAN_PROVIDER (podman),
                                             "manager",
                                             "distrobox",
                                             PROMPT_TYPE_DISTROBOX_CONTAINER);
  prompt_podman_provider_update_sync (PROMPT_PODMAN_PROVIDER (podman), NULL, NULL);
  prompt_agent_impl_add_provider (agent->impl, podman);

  g_dbus_connection_start_message_processing (agent->bus);

  return TRUE;
}

static int
prompt_agent_run (PromptAgent *agent)
{
  g_main_loop_run (agent->main_loop);
  return agent->exit_code;
}

static void
prompt_agent_destroy (PromptAgent *agent)
{
  g_clear_object (&agent->impl);
  g_clear_object (&agent->socket);
  g_clear_object (&agent->stream);
  g_clear_object (&agent->bus);
  g_clear_pointer (&agent->main_loop, g_main_loop_unref);
}

int
main (int   argc,
      char *argv[])
{
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(GError) error = NULL;
  PromptAgent agent;
  int socket_fd = -1;
  int ret;

  const GOptionEntry entries[] = {
    { "socket-fd", 0, 0, G_OPTION_ARG_INT, &socket_fd, "The socketpair to communicate over", "FD" },
    { NULL }
  };

  g_set_prgname ("prompt-agent");
  g_set_application_name ("prompt-agent");

  context = g_option_context_new ("- terminal container agent");
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  if (!g_option_context_parse (context, &argc, &argv, &error) ||
      !prompt_agent_init (&agent, socket_fd, &error))
    {
      g_printerr ("usage: %s --socket-fd=FD\n", argv[0]);
      g_printerr ("\n");
      g_printerr ("%s\n", error->message);
      return EXIT_FAILURE;
    }

  ret = prompt_agent_run (&agent);
  prompt_agent_destroy (&agent);

  return ret;
}
