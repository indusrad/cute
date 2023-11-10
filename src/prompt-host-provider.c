/* prompt-host-provider.c
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

#include "prompt-host-container.h"
#include "prompt-host-provider.h"

struct _PromptHostProvider
{
  PromptContainerProvider  parent_instance;
  PromptHostContainer     *host;
};

G_DEFINE_FINAL_TYPE (PromptHostProvider, prompt_host_provider, PROMPT_TYPE_CONTAINER_PROVIDER)

static void
prompt_host_provider_constructed (GObject *object)
{
  PromptHostProvider *self = PROMPT_HOST_PROVIDER (object);

  G_OBJECT_CLASS (prompt_host_provider_parent_class)->constructed (object);

  prompt_container_provider_emit_added (PROMPT_CONTAINER_PROVIDER (self),
                                        PROMPT_CONTAINER (self->host));
}

static void
prompt_host_provider_finalize (GObject *object)
{
  PromptHostProvider *self = PROMPT_HOST_PROVIDER (object);

  g_clear_object (&self->host);

  G_OBJECT_CLASS (prompt_host_provider_parent_class)->finalize (object);
}

static void
prompt_host_provider_class_init (PromptHostProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = prompt_host_provider_constructed;
  object_class->finalize = prompt_host_provider_finalize;
}

static void
prompt_host_provider_init (PromptHostProvider *self)
{
  self->host = prompt_host_container_new ();
}

PromptContainerProvider *
prompt_host_provider_new (void)
{
  return g_object_new (PROMPT_TYPE_HOST_PROVIDER, NULL);
}
