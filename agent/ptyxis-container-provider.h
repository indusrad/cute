/* ptyxis-container-provider.h
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

#include "ptyxis-agent-ipc.h"

G_BEGIN_DECLS

#define PTYXIS_TYPE_CONTAINER_PROVIDER (ptyxis_container_provider_get_type())

G_DECLARE_DERIVABLE_TYPE (PtyxisContainerProvider, ptyxis_container_provider, PTYXIS, CONTAINER_PROVIDER, GObject)

struct _PtyxisContainerProviderClass
{
  GObjectClass parent_class;

  void (*added)   (PtyxisContainerProvider *self,
                   PtyxisIpcContainer      *container);
  void (*removed) (PtyxisContainerProvider *self,
                   PtyxisIpcContainer      *container);
};

void ptyxis_container_provider_emit_added   (PtyxisContainerProvider *self,
                                             PtyxisIpcContainer      *container);
void ptyxis_container_provider_emit_removed (PtyxisContainerProvider *self,
                                             PtyxisIpcContainer      *container);
void ptyxis_container_provider_merge        (PtyxisContainerProvider *self,
                                             GPtrArray               *containers);

G_END_DECLS
