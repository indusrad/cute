/* prompt-podman-container.c
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

typedef struct
{
  gpointer dummy;
} PromptPodmanContainerPrivate;

enum {
  PROP_0,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (PromptPodmanContainer, prompt_podman_container, PROMPT_TYPE_CONTAINER)

static GParamSpec *properties [N_PROPS];

static void
prompt_podman_container_finalize (GObject *object)
{
  G_OBJECT_CLASS (prompt_podman_container_parent_class)->finalize (object);
}

static void
prompt_podman_container_get_property (GObject    *object,
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
prompt_podman_container_set_property (GObject      *object,
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
prompt_podman_container_class_init (PromptPodmanContainerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = prompt_podman_container_finalize;
  object_class->get_property = prompt_podman_container_get_property;
  object_class->set_property = prompt_podman_container_set_property;
}

static void
prompt_podman_container_init (PromptPodmanContainer *self)
{
}

gboolean
prompt_podman_container_deserialize (PromptPodmanContainer  *self,
                                     JsonObject             *object,
                                     GError                **error)
{
  return PROMPT_PODMAN_CONTAINER_GET_CLASS (self)->deserialize (self, object, error);
}
