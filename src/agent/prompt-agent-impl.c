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

struct _PromptAgentImpl
{
  PromptIpcAgentSkeleton parent_instance;
  GPtrArray *containers;
};

G_DEFINE_TYPE (PromptAgentImpl, prompt_agent_impl, PROMPT_IPC_TYPE_AGENT_SKELETON)

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
prompt_agent_impl_new (void)
{
  return g_object_new (PROMPT_TYPE_AGENT_IMPL, NULL);
}
