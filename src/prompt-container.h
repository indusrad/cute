/*
 * prompt-container.h
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

#include "prompt-profile.h"
#include "prompt-run-context.h"

G_BEGIN_DECLS

#define PROMPT_TYPE_CONTAINER (prompt_container_get_type())

G_DECLARE_DERIVABLE_TYPE (PromptContainer, prompt_container, PROMPT, CONTAINER, GObject)

struct _PromptContainerClass
{
  GObjectClass parent_class;

  const char  *(*get_id)         (PromptContainer     *self);
  void         (*prepare_async)  (PromptContainer     *self,
                                  PromptRunContext    *run_context,
                                  GCancellable         *cancellable,
                                  GAsyncReadyCallback   callback,
                                  gpointer              user_data);
  gboolean     (*prepare_finish) (PromptContainer     *self,
                                  GAsyncResult         *result,
                                  GError              **error);
};

const char  *prompt_container_get_id         (PromptContainer      *self);
void         prompt_container_prepare_async  (PromptContainer      *self,
                                              PromptRunContext     *run_context,
                                              GCancellable         *cancellable,
                                              GAsyncReadyCallback   callback,
                                              gpointer              user_data);
gboolean     prompt_container_prepare_finish (PromptContainer      *self,
                                              GAsyncResult         *result,
                                              GError              **error);
void         prompt_container_spawn_async    (PromptContainer      *self,
                                              VtePty               *pty,
                                              PromptProfile        *profile,
                                              const char           *current_directory_uri,
                                              GCancellable         *cancellable,
                                              GAsyncReadyCallback   callback,
                                              gpointer              user_data);
GSubprocess *prompt_container_spawn_finish   (PromptContainer      *self,
                                              GAsyncResult         *result,
                                              GError              **error);

G_END_DECLS
