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

#include "prompt-agent-impl.h"
#include "prompt-session-container.h"

struct _PromptAgentImpl
{
  PromptIpcAgentSkeleton parent_instance;
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

  G_OBJECT_CLASS (prompt_agent_impl_parent_class)->finalize (object);
}

static void
prompt_agent_impl_class_init (PromptAgentImplClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = prompt_agent_impl_finalize;
}

static void
prompt_agent_impl_init (PromptAgentImpl *self)
{
  self->containers = g_ptr_array_new_with_free_func (g_object_unref);
}

PromptAgentImpl *
prompt_agent_impl_new (GError **error)
{
  return g_object_new (PROMPT_TYPE_AGENT_IMPL, NULL);
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
                                             (const char * const *)strv->data);

  self->has_listed_containers = TRUE;

  return TRUE;
}

static void
agent_iface_init (PromptIpcAgentIface *iface)
{
  iface->handle_list_containers = prompt_agent_impl_handle_list_containers;
}
