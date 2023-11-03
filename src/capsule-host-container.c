/*
 * capsule-host-container.c
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

#include "capsule-host-container.h"
#include "capsule-run-context.h"
#include "capsule-util.h"

struct _CapsuleHostContainer
{
  CapsuleContainer parent_instance;
};

G_DEFINE_FINAL_TYPE (CapsuleHostContainer, capsule_host_container, CAPSULE_TYPE_CONTAINER)

static void
capsule_host_container_spawn_async (CapsuleContainer    *container,
                                    VtePty              *pty,
                                    CapsuleProfile      *profile,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(CapsuleRunContext) run_context = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = NULL;

  g_assert (CAPSULE_IS_HOST_CONTAINER (container));
  g_assert (VTE_IS_PTY (pty));
  g_assert (CAPSULE_IS_PROFILE (profile));

  task = g_task_new (container, cancellable, callback, user_data);
  g_task_set_source_tag (task, capsule_host_container_spawn_async);

  run_context = capsule_run_context_new ();

  capsule_run_context_push_host (run_context);
  capsule_run_context_set_pty (run_context, pty);

  capsule_container_prepare_run_context (container, run_context, profile);

  if (!(subprocess = capsule_run_context_spawn (run_context, &error)))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_pointer (task, g_steal_pointer (&subprocess), g_object_unref);
}

static GSubprocess *
capsule_host_container_spawn_finish (CapsuleContainer  *container,
                                     GAsyncResult      *result,
                                     GError           **error)
{
  g_assert (CAPSULE_IS_HOST_CONTAINER (container));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}

static const char *
capsule_host_container_get_id (CapsuleContainer *container)
{
  g_assert (CAPSULE_IS_HOST_CONTAINER (container));

  return "host";
}

static void
capsule_host_container_class_init (CapsuleHostContainerClass *klass)
{
  CapsuleContainerClass *container_class = CAPSULE_CONTAINER_CLASS (klass);

  container_class->get_id = capsule_host_container_get_id;
  container_class->spawn_async = capsule_host_container_spawn_async;
  container_class->spawn_finish = capsule_host_container_spawn_finish;
}

static void
capsule_host_container_init (CapsuleHostContainer *self)
{
}

CapsuleContainer *
capsule_host_container_new (void)
{
  return g_object_new (CAPSULE_TYPE_HOST_CONTAINER, NULL);
}
