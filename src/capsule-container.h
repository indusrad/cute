/*
 * capsule-container.h
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

#pragma once

#include <gio/gio.h>
#include <vte/vte.h>

#include "capsule-profile.h"
#include "capsule-run-context.h"

G_BEGIN_DECLS

#define CAPSULE_TYPE_CONTAINER (capsule_container_get_type())

G_DECLARE_DERIVABLE_TYPE (CapsuleContainer, capsule_container, CAPSULE, CONTAINER, GObject)

struct _CapsuleContainerClass
{
  GObjectClass parent_class;

  void         (*spawn_async)  (CapsuleContainer     *self,
                                VtePty               *pty,
                                CapsuleProfile       *profile,
                                GCancellable         *cancellable,
                                GAsyncReadyCallback   callback,
                                gpointer              user_data);
  GSubprocess *(*spawn_finish) (CapsuleContainer     *self,
                                GAsyncResult         *result,
                                GError              **error);
};

void         capsule_container_prepare_run_context (CapsuleContainer     *self,
                                                    CapsuleRunContext    *run_context,
                                                    CapsuleProfile       *profile);
void         capsule_container_spawn_async         (CapsuleContainer     *self,
                                                    VtePty               *pty,
                                                    CapsuleProfile       *profile,
                                                    GCancellable         *cancellable,
                                                    GAsyncReadyCallback   callback,
                                                    gpointer              user_data);
GSubprocess *capsule_container_spawn_finish        (CapsuleContainer     *self,
                                                    GAsyncResult         *result,
                                                    GError              **error);

G_END_DECLS
