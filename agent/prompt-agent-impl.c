/*
 * prompt-agent-impl.c
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

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include "prompt-agent-compat.h"
#include "prompt-agent-impl.h"
#include "prompt-agent-util.h"
#include "prompt-run-context.h"
#include "prompt-session-container.h"

struct _PromptAgentImpl
{
  PromptIpcAgentSkeleton parent_instance;
  GPtrArray *providers;
  GPtrArray *containers;
  guint has_listed_containers : 1;
};

static void agent_iface_init (PromptIpcAgentIface *iface);

G_DEFINE_TYPE_WITH_CODE (PromptAgentImpl, prompt_agent_impl, PROMPT_IPC_TYPE_AGENT_SKELETON,
                         G_IMPLEMENT_INTERFACE (PROMPT_IPC_TYPE_AGENT, agent_iface_init))

static void
prompt_agent_impl_finalize (GObject *object)
{
  PromptAgentImpl *self = (PromptAgentImpl *)object;

  g_clear_pointer (&self->containers, g_ptr_array_unref);
  g_clear_pointer (&self->providers, g_ptr_array_unref);

  G_OBJECT_CLASS (prompt_agent_impl_parent_class)->finalize (object);
}

static void
prompt_agent_impl_load_os_release (PromptAgentImpl *self)
{
  g_autofree char *contents = NULL;
  gsize len;

  g_assert (PROMPT_IS_AGENT_IMPL (self));

  if (g_file_get_contents ("/etc/os-release", &contents, &len, NULL))
    {
      const char *begin = strstr (contents, "NAME=\"");

      if (begin != NULL)
        {
          const char *end;

          begin += strlen ("NAME=\"");

          if ((end = strchr (begin, '"')))
            {
              g_autofree char *name = g_strndup (begin, end - begin);
              prompt_ipc_agent_set_os_name (PROMPT_IPC_AGENT (self), name);
            }
        }
    }
}

static void
prompt_agent_impl_constructed (GObject *object)
{
  PromptAgentImpl *self = (PromptAgentImpl *)object;

  G_OBJECT_CLASS (prompt_agent_impl_parent_class)->constructed (object);

  prompt_agent_impl_load_os_release (self);
}

static void
prompt_agent_impl_class_init (PromptAgentImplClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = prompt_agent_impl_constructed;
  object_class->finalize = prompt_agent_impl_finalize;
}

static void
prompt_agent_impl_init (PromptAgentImpl *self)
{
  self->containers = g_ptr_array_new_with_free_func (g_object_unref);
  self->providers = g_ptr_array_new_with_free_func (g_object_unref);
}

PromptAgentImpl *
prompt_agent_impl_new (GError **error)
{
  return g_object_new (PROMPT_TYPE_AGENT_IMPL, NULL);
}

static void
prompt_agent_impl_provider_added_cb (PromptAgentImpl         *self,
                                     PromptIpcContainer      *container,
                                     PromptContainerProvider *provider)
{
  g_assert (PROMPT_IS_AGENT_IMPL (self));
  g_assert (PROMPT_IPC_IS_CONTAINER (container));
  g_assert (PROMPT_IS_CONTAINER_PROVIDER (provider));

  prompt_agent_impl_add_container (self, container);
}

static void
prompt_agent_impl_provider_removed_cb (PromptAgentImpl         *self,
                                       PromptIpcContainer      *container,
                                       PromptContainerProvider *provider)
{
  static const char * const empty[] = {NULL};
  const char *id;

  g_assert (PROMPT_IS_AGENT_IMPL (self));
  g_assert (PROMPT_IPC_IS_CONTAINER (container));
  g_assert (PROMPT_IS_CONTAINER_PROVIDER (provider));

  id = prompt_ipc_container_get_id (container);

  g_return_if_fail (id != NULL);

  for (guint i = 0; i < self->containers->len; i++)
    {
      PromptIpcContainer *element = g_ptr_array_index (self->containers, i);
      const char *element_id = prompt_ipc_container_get_id (element);

      if (g_strcmp0 (id, element_id) == 0)
        {
          g_ptr_array_remove_index (self->containers, i);

          if (self->has_listed_containers)
            prompt_ipc_agent_emit_containers_changed (PROMPT_IPC_AGENT (self), i, 1, empty);

          break;
        }
    }
}

void
prompt_agent_impl_add_provider (PromptAgentImpl         *self,
                                PromptContainerProvider *provider)
{
  guint n_items;

  g_assert (PROMPT_IS_AGENT_IMPL (self));
  g_assert (PROMPT_IS_CONTAINER_PROVIDER (provider));

  g_ptr_array_add (self->providers, g_object_ref (provider));

  g_signal_connect_object (provider,
                           "added",
                           G_CALLBACK (prompt_agent_impl_provider_added_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (provider,
                           "removed",
                           G_CALLBACK (prompt_agent_impl_provider_removed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (provider));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(PromptIpcContainer) container = g_list_model_get_item (G_LIST_MODEL (provider), i);

      prompt_agent_impl_provider_added_cb (self, container, provider);
    }
}

void
prompt_agent_impl_add_container (PromptAgentImpl    *self,
                                 PromptIpcContainer *container)
{
  g_autofree char *object_path = NULL;
  g_autofree char *guid = NULL;

  g_return_if_fail (PROMPT_IS_AGENT_IMPL (self));
  g_return_if_fail (PROMPT_IPC_IS_CONTAINER (container));

  guid = g_dbus_generate_guid ();
  object_path = g_strdup_printf ("/org/gnome/Prompt/Containers/%s", guid);

  g_ptr_array_add (self->containers, g_object_ref (container));

  g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (container),
                                    g_dbus_interface_skeleton_get_connection (G_DBUS_INTERFACE_SKELETON (self)),
                                    object_path,
                                    NULL);

  if (self->has_listed_containers)
    {
      const char * const object_paths[2] = {object_path, NULL};
      prompt_ipc_agent_emit_containers_changed (PROMPT_IPC_AGENT (self),
                                                self->containers->len-1,
                                                0,
                                                object_paths);
    }
}

static gboolean
prompt_agent_impl_handle_list_containers (PromptIpcAgent        *agent,
                                          GDBusMethodInvocation *invocation)
{
  PromptAgentImpl *self = (PromptAgentImpl *)agent;
  g_autoptr(GArray) strv = NULL;

  g_assert (PROMPT_IS_AGENT_IMPL (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  strv = g_array_new (TRUE, FALSE, sizeof (char*));

  for (guint i = 0; i < self->containers->len; i++)
    {
      GDBusInterfaceSkeleton *skeleton = g_ptr_array_index (self->containers, i);
      const char *object_path = g_dbus_interface_skeleton_get_object_path (skeleton);

      g_array_append_val (strv, object_path);
    }

  prompt_ipc_agent_complete_list_containers (agent,
                                             g_steal_pointer (&invocation),
                                             (const char * const *)(gpointer)strv->data);

  self->has_listed_containers = TRUE;

  return TRUE;
}

static gboolean
prompt_agent_impl_handle_create_pty (PromptIpcAgent        *agent,
                                     GDBusMethodInvocation *invocation,
                                     GUnixFDList           *in_fd_list)
{
  g_autoptr(GUnixFDList) out_fd_list = NULL;
  g_autoptr(GError) error = NULL;
  _g_autofd int pty_fd = -1;
  int handle;

  g_assert (PROMPT_IS_AGENT_IMPL (agent));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (!in_fd_list || G_IS_UNIX_FD_LIST (in_fd_list));

  out_fd_list = g_unix_fd_list_new ();

  if (-1 == (pty_fd = prompt_agent_pty_new (&error)) ||
      -1 == (handle = g_unix_fd_list_append (out_fd_list, pty_fd, &error)))
    g_dbus_method_invocation_return_gerror (g_steal_pointer (&invocation), error);
  else
    prompt_ipc_agent_complete_create_pty (agent,
                                          g_steal_pointer (&invocation),
                                          out_fd_list,
                                          g_variant_new_handle (handle));

  return TRUE;
}

static gboolean
prompt_agent_impl_handle_create_pty_producer (PromptIpcAgent        *agent,
                                              GDBusMethodInvocation *invocation,
                                              GUnixFDList           *in_fd_list,
                                              GVariant              *in_pty_fd)
{
  g_autoptr(GUnixFDList) out_fd_list = NULL;
  g_autoptr(GError) error = NULL;
  _g_autofd int consumer_fd = -1;
  _g_autofd int producer_fd = -1;
  int out_handle;
  int in_handle;

  g_assert (PROMPT_IS_AGENT_IMPL (agent));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (!in_fd_list || G_IS_UNIX_FD_LIST (in_fd_list));

  in_handle = g_variant_get_handle (in_pty_fd);
  if (-1 == (consumer_fd = g_unix_fd_list_get (in_fd_list, in_handle, &error)))
    goto return_gerror;

  if (-1 == (producer_fd = prompt_agent_pty_new_producer (consumer_fd, &error)))
    goto return_gerror;

  out_fd_list = g_unix_fd_list_new ();
  if (-1 == (out_handle = g_unix_fd_list_append (out_fd_list, producer_fd, &error)))
    goto return_gerror;

  prompt_ipc_agent_complete_create_pty (agent,
                                        g_steal_pointer (&invocation),
                                        out_fd_list,
                                        g_variant_new_handle (out_handle));

  return TRUE;

return_gerror:
  g_dbus_method_invocation_return_gerror (g_steal_pointer (&invocation), error);

  return TRUE;
}

static gboolean
prompt_agent_impl_handle_get_preferred_shell (PromptIpcAgent        *agent,
                                              GDBusMethodInvocation *invocation)
{
  const char *default_shell = "/bin/sh";
  struct passwd *pw;

  g_assert (PROMPT_IS_AGENT_IMPL (agent));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  if ((pw = getpwuid (getuid ())))
    {
      if (access (pw->pw_shell, X_OK) == 0)
        default_shell = pw->pw_shell;
    }

  prompt_ipc_agent_complete_get_preferred_shell (agent,
                                                 g_steal_pointer (&invocation),
                                                 default_shell);

  return TRUE;
}

static gboolean
prompt_agent_impl_handle_discover_current_container (PromptIpcAgent        *agent,
                                                     GDBusMethodInvocation *invocation,
                                                     GUnixFDList           *in_fd_list,
                                                     GVariant              *in_pty_fd)
{
  PromptAgentImpl *self = (PromptAgentImpl *)agent;
  g_autoptr(GError) error = NULL;
  _g_autofd int consumer_fd = -1;
  g_autofree char *container_id = NULL;
  int in_handle;
  GPid pid;

  g_assert (PROMPT_IS_AGENT_IMPL (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  in_handle = g_variant_get_handle (in_pty_fd);
  if (-1 == (consumer_fd = g_unix_fd_list_get (in_fd_list, in_handle, &error)))
    goto return_gerror;

  pid = tcgetpgrp (consumer_fd);

  g_print ("pid: %d\n", pid);

  if (pid > 0)
    {
#if 0
      g_autofree char *containerenv_path = g_strdup_printf ("/proc/%u/root/var/run/.containerenv", pid);
      g_autofree char *contents = NULL;
      gsize len = 0;

      /* Currently this doesn't work because our parent process (toolbox enter) doesn't
       * have anything mapped in to key off of. But I _really_ want to avoid custom VTE
       * patches like g-t does in Fedora.
       */

      /* At this point, we might find that the foreground process is something like
       * podman or toolbox, or something else injecting a new PTY between us and
       * the foreground process we really want to track. Podman should give us a
       * /var/run/.containerenv in the root directory of the process with enough
       * info to determine the id.
       */

      if (g_file_get_contents (containerenv_path, &contents, &len, NULL))
        {
          const char *ideq;

          g_print ("%s\n", contents);

          /* g_file_get_contents() ensures we always have a trailing \0 */

          if ((ideq = strstr (contents, "\nid=\"")))
            {
              const char *begin = ideq + strlen ("\nid=\"");
              const char *end = strstr (begin, "\"\n");

              if (end != NULL)
                container_id = g_strndup (begin, end - begin);
            }
        }
#endif
    }

  if (container_id == NULL)
    container_id = g_strdup ("session");

  for (guint i = 0; i < self->containers->len; i++)
    {
      PromptIpcContainer *container = g_ptr_array_index (self->containers, i);
      const char *id = prompt_ipc_container_get_id (container);

      if (g_strcmp0 (container_id, id) == 0)
        {
          const char *object_path = g_dbus_interface_skeleton_get_object_path (G_DBUS_INTERFACE_SKELETON (container));
          prompt_ipc_agent_complete_discover_current_container (agent,
                                                                g_steal_pointer (&invocation),
                                                                NULL,
                                                                object_path);
          return TRUE;
        }
    }

  g_set_error (&error,
               G_IO_ERROR,
               G_IO_ERROR_NOT_FOUND,
               "No such container \"%s\"", container_id);

return_gerror:
  g_dbus_method_invocation_return_gerror (g_steal_pointer (&invocation), error);

  return TRUE;
}

static void
agent_iface_init (PromptIpcAgentIface *iface)
{
  iface->handle_create_pty = prompt_agent_impl_handle_create_pty;
  iface->handle_create_pty_producer = prompt_agent_impl_handle_create_pty_producer;
  iface->handle_get_preferred_shell = prompt_agent_impl_handle_get_preferred_shell;
  iface->handle_list_containers = prompt_agent_impl_handle_list_containers;
  iface->handle_discover_current_container = prompt_agent_impl_handle_discover_current_container;
}
