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

#include "prompt-session-container.h"

struct _PromptSessionContainer
{
  PromptIpcContainerSkeleton parent_instance;
};

G_DEFINE_TYPE (PromptSessionContainer, prompt_session_container, PROMPT_IPC_TYPE_CONTAINER_SKELETON)

static void
prompt_session_container_class_init (PromptSessionContainerClass *klass)
{
}

static void
prompt_session_container_init (PromptSessionContainer *self)
{
}

PromptSessionContainer *
prompt_session_container_new (void)
{
  return g_object_new (PROMPT_TYPE_SESSION_CONTAINER, NULL);
}
