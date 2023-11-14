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
  GHashTable *labels;
} PromptPodmanContainerPrivate;

enum {
  PROP_0,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (PromptPodmanContainer, prompt_podman_container, PROMPT_TYPE_CONTAINER)

static GParamSpec *properties [N_PROPS];

static void
prompt_podman_container_deserialize_labels (PromptPodmanContainer *self,
                                            JsonObject            *labels)
{
  PromptPodmanContainerPrivate *priv = prompt_podman_container_get_instance_private (self);
  JsonObjectIter iter;
  const char *key;
  JsonNode *value;

  g_assert (PROMPT_IS_PODMAN_CONTAINER (self));
  g_assert (labels != NULL);

  json_object_iter_init (&iter, labels);

  while (json_object_iter_next (&iter, &key, &value))
    {
      if (JSON_NODE_HOLDS_VALUE (value) &&
          json_node_get_value_type (value) == G_TYPE_STRING)
        {
          const char *value_str = json_node_get_string (value);

          g_hash_table_insert (priv->labels, g_strdup (key), g_strdup (value_str));
        }
    }
}

static gboolean
prompt_podman_container_real_deserialize (PromptPodmanContainer  *self,
                                          JsonObject             *object,
                                          GError                **error)
{
  JsonObject *labels_object;
  JsonNode *labels;

  g_assert (PROMPT_IS_PODMAN_CONTAINER (self));
  g_assert (object != NULL);

  if (json_object_has_member (object, "Labels") &&
      (labels = json_object_get_member (object, "Labels")) &&
      JSON_NODE_HOLDS_OBJECT (labels) &&
      (labels_object = json_node_get_object (labels)))
    prompt_podman_container_deserialize_labels (self, labels_object);

  return TRUE;
}

static void
prompt_podman_container_dispose (GObject *object)
{
  PromptPodmanContainer *self = (PromptPodmanContainer *)object;
  PromptPodmanContainerPrivate *priv = prompt_podman_container_get_instance_private (self);

  g_hash_table_remove_all (priv->labels);

  G_OBJECT_CLASS (prompt_podman_container_parent_class)->dispose (object);
}

static void
prompt_podman_container_finalize (GObject *object)
{
  PromptPodmanContainer *self = (PromptPodmanContainer *)object;
  PromptPodmanContainerPrivate *priv = prompt_podman_container_get_instance_private (self);

  g_clear_pointer (&priv->labels, g_hash_table_unref);

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

  object_class->dispose = prompt_podman_container_dispose;
  object_class->finalize = prompt_podman_container_finalize;
  object_class->get_property = prompt_podman_container_get_property;
  object_class->set_property = prompt_podman_container_set_property;

  klass->deserialize = prompt_podman_container_real_deserialize;
}

static void
prompt_podman_container_init (PromptPodmanContainer *self)
{
  PromptPodmanContainerPrivate *priv = prompt_podman_container_get_instance_private (self);

  priv->labels = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
}

gboolean
prompt_podman_container_deserialize (PromptPodmanContainer  *self,
                                     JsonObject             *object,
                                     GError                **error)
{
  g_return_val_if_fail (PROMPT_IS_PODMAN_CONTAINER (self), FALSE);
  g_return_val_if_fail (object != NULL, FALSE);

  return PROMPT_PODMAN_CONTAINER_GET_CLASS (self)->deserialize (self, object, error);
}
