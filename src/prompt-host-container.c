/*
 * prompt-host-container.c
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

#include <glib/gstdio.h>

#include "prompt-host-container.h"
#include "prompt-run-context.h"
#include "prompt-util.h"

struct _PromptHostContainer
{
  PromptContainer parent_instance;
};

G_DEFINE_FINAL_TYPE (PromptHostContainer, prompt_host_container, PROMPT_TYPE_CONTAINER)

static void
prompt_host_container_prepare_async (PromptContainer     *container,
                                     PromptRunContext    *run_context,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_assert (PROMPT_IS_HOST_CONTAINER (container));
  g_assert (PROMPT_IS_RUN_CONTEXT (run_context));

  prompt_run_context_push_host (run_context);

  task = g_task_new (container, cancellable, callback, user_data);
  g_task_set_source_tag (task, prompt_host_container_prepare_async);
  g_task_return_boolean (task, TRUE);
}

static gboolean
prompt_host_container_prepare_finish (PromptContainer  *container,
                                      GAsyncResult     *result,
                                      GError          **error)
{
  g_assert (PROMPT_IS_HOST_CONTAINER (container));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

static const char *
prompt_host_container_get_id (PromptContainer *container)
{
  g_assert (PROMPT_IS_HOST_CONTAINER (container));

  return "host";
}

static void
prompt_host_container_class_init (PromptHostContainerClass *klass)
{
  PromptContainerClass *container_class = PROMPT_CONTAINER_CLASS (klass);

  container_class->get_id = prompt_host_container_get_id;
  container_class->prepare_async = prompt_host_container_prepare_async;
  container_class->prepare_finish = prompt_host_container_prepare_finish;
}

static void
prompt_host_container_init (PromptHostContainer *self)
{
}

PromptHostContainer *
prompt_host_container_new (void)
{
  return g_object_new (PROMPT_TYPE_HOST_CONTAINER, NULL);
}
