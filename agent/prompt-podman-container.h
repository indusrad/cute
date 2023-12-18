/* prompt-podman-container.h
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

#include <json-glib/json-glib.h>

#include "prompt-agent-ipc.h"
#include "prompt-run-context.h"

G_BEGIN_DECLS

#define PROMPT_TYPE_PODMAN_CONTAINER (prompt_podman_container_get_type())

G_DECLARE_DERIVABLE_TYPE (PromptPodmanContainer, prompt_podman_container, PROMPT, PODMAN_CONTAINER, PromptIpcContainerSkeleton)

struct _PromptPodmanContainerClass
{
  PromptIpcContainerSkeletonClass parent_class;

  gboolean (*deserialize)         (PromptPodmanContainer  *self,
                                   JsonObject             *object,
                                   GError                **error);
  void     (*prepare_run_context) (PromptPodmanContainer  *self,
                                   PromptRunContext       *run_context);
};

gboolean prompt_podman_container_deserialize (PromptPodmanContainer  *self,
                                              JsonObject             *object,
                                              GError                **error);

G_END_DECLS
