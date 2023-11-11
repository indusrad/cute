/* prompt-toolbox-container.c
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

#include "prompt-toolbox-container.h"

struct _PromptToolboxContainer
{
  PromptPodmanContainer parent_instance;
};

enum {
  PROP_0,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PromptToolboxContainer, prompt_toolbox_container, PROMPT_TYPE_PODMAN_CONTAINER)

static GParamSpec *properties [N_PROPS];

static gboolean
prompt_toolbox_container_deserialize (PromptPodmanContainer  *container,
                                      JsonObject             *object,
                                      GError                **error)
{
  /* TODO */
  return TRUE;
}

static void
prompt_toolbox_container_finalize (GObject *object)
{
  G_OBJECT_CLASS (prompt_toolbox_container_parent_class)->finalize (object);
}

static void
prompt_toolbox_container_get_property (GObject    *object,
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
prompt_toolbox_container_set_property (GObject      *object,
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
prompt_toolbox_container_class_init (PromptToolboxContainerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  PromptPodmanContainerClass *podman_container_class = PROMPT_PODMAN_CONTAINER_CLASS (klass);

  object_class->finalize = prompt_toolbox_container_finalize;
  object_class->get_property = prompt_toolbox_container_get_property;
  object_class->set_property = prompt_toolbox_container_set_property;

  podman_container_class->deserialize = prompt_toolbox_container_deserialize;
}

static void
prompt_toolbox_container_init (PromptToolboxContainer *self)
{
}
