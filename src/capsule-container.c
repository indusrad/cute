/*
 * capsule-container.c
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

#include "capsule-container.h"
#include "capsule-util.h"

G_DEFINE_ABSTRACT_TYPE (CapsuleContainer, capsule_container, G_TYPE_OBJECT)

static void
capsule_container_class_init (CapsuleContainerClass *klass)
{
}

static void
capsule_container_init (CapsuleContainer *self)
{
}

void
capsule_container_spawn_async (CapsuleContainer    *self,
                               VtePty              *pty,
                               CapsuleProfile      *profile,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  CAPSULE_CONTAINER_GET_CLASS (self)->spawn_async (self, pty, profile, cancellable, callback, user_data);
}

GSubprocess *
capsule_container_spawn_finish (CapsuleContainer  *self,
                                GAsyncResult      *result,
                                GError           **error)
{
  return CAPSULE_CONTAINER_GET_CLASS (self)->spawn_finish (self, result, error);
}

void
capsule_container_prepare_run_context (CapsuleContainer  *self,
                                       CapsuleRunContext *run_context,
                                       CapsuleProfile    *profile)
{
  g_return_if_fail (CAPSULE_IS_CONTAINER (self));
  g_return_if_fail (CAPSULE_IS_RUN_CONTEXT (run_context));
  g_return_if_fail (CAPSULE_IS_PROFILE (profile));

  capsule_run_context_append_argv (run_context, capsule_get_user_shell ());
}
