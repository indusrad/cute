/* prompt-podman-provider.c
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

#include "prompt-podman-container.h"
#include "prompt-podman-provider.h"

typedef struct _LabelToType
{
  const char *label;
  const char *value;
  GType type;
} LabelToType;

struct _PromptPodmanProvider
{
  PromptContainerProvider parent_instance;
  GArray *label_to_type;
};

enum {
  PROP_0,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PromptPodmanProvider, prompt_podman_provider, PROMPT_TYPE_CONTAINER_PROVIDER)

static GParamSpec *properties [N_PROPS];

static void
prompt_podman_provider_finalize (GObject *object)
{
  PromptPodmanProvider *self = (PromptPodmanProvider *)object;

  g_clear_pointer (&self->label_to_type, g_array_unref);

  G_OBJECT_CLASS (prompt_podman_provider_parent_class)->finalize (object);
}

static void
prompt_podman_provider_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_podman_provider_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_podman_provider_class_init (PromptPodmanProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = prompt_podman_provider_finalize;
  object_class->get_property = prompt_podman_provider_get_property;
  object_class->set_property = prompt_podman_provider_set_property;
}

static void
prompt_podman_provider_init (PromptPodmanProvider *self)
{
  self->label_to_type = g_array_new (FALSE, FALSE, sizeof (LabelToType));
}

PromptContainerProvider *
prompt_podman_provider_new (void)
{
  return g_object_new (PROMPT_TYPE_PODMAN_PROVIDER, NULL);
}

void
prompt_podman_provider_set_type_for_label (PromptPodmanProvider *self,
                                           const char           *key,
                                           const char           *value,
                                           GType                 container_type)
{
  LabelToType map;

  g_return_if_fail (PROMPT_IS_PODMAN_PROVIDER (self));
  g_return_if_fail (key != NULL);
  g_return_if_fail (g_type_is_a (container_type, PROMPT_TYPE_PODMAN_CONTAINER));

  map.label = g_intern_string (key);
  map.value = g_intern_string (value);
  map.type = container_type;

  g_array_append_val (self->label_to_type, map);
}
