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

#include <fcntl.h>

#include <json-glib/json-glib.h>

#include "prompt-agent-compat.h"
#include "prompt-agent-util.h"
#include "prompt-podman-container.h"
#include "prompt-podman-provider.h"
#include "prompt-run-context.h"

typedef struct _LabelToType
{
  const char *label;
  const char *value;
  GType type;
} LabelToType;

struct _PromptPodmanProvider
{
  PromptContainerProvider parent_instance;
  GFileMonitor *monitor;
  GArray *label_to_type;
  guint queued_update;
};

G_DEFINE_TYPE (PromptPodmanProvider, prompt_podman_provider, PROMPT_TYPE_CONTAINER_PROVIDER)

static void
prompt_podman_provider_constructed (GObject *object)
{
  PromptPodmanProvider *self = (PromptPodmanProvider *)object;
  g_autoptr(GFile) file = NULL;
  g_autofree char *data_dir = NULL;

  G_OBJECT_CLASS (prompt_podman_provider_parent_class)->constructed (object);

  _g_set_str (&data_dir, g_get_user_data_dir ());
  if (data_dir == NULL)
    data_dir = g_build_filename (g_get_home_dir (), ".local", "share", NULL);

  g_assert (data_dir != NULL);

  file = g_file_new_build_filename (data_dir,
                                    "containers", "storage", "overlay-containers",
                                    "containers.json",
                                    NULL);

  if ((self->monitor = g_file_monitor (file, G_FILE_MONITOR_NONE, NULL, NULL)))
    {
      g_file_monitor_set_rate_limit (self->monitor, 5000);
      g_signal_connect_object (self->monitor,
                               "changed",
                               G_CALLBACK (prompt_podman_provider_queue_update),
                               self,
                               G_CONNECT_SWAPPED);
    }

  prompt_podman_provider_queue_update (self);
}

static void
prompt_podman_provider_dispose (GObject *object)
{
  PromptPodmanProvider *self = (PromptPodmanProvider *)object;

  if (self->label_to_type->len > 0)
    g_array_remove_range (self->label_to_type, 0, self->label_to_type->len);

  g_clear_object (&self->monitor);
  g_clear_handle_id (&self->queued_update, g_source_remove);

  G_OBJECT_CLASS (prompt_podman_provider_parent_class)->dispose (object);
}

static void
prompt_podman_provider_finalize (GObject *object)
{
  PromptPodmanProvider *self = (PromptPodmanProvider *)object;

  g_clear_pointer (&self->label_to_type, g_array_unref);

  G_OBJECT_CLASS (prompt_podman_provider_parent_class)->finalize (object);
}

static void
prompt_podman_provider_class_init (PromptPodmanProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = prompt_podman_provider_constructed;
  object_class->dispose = prompt_podman_provider_dispose;
  object_class->finalize = prompt_podman_provider_finalize;
}

static void
prompt_podman_provider_init (PromptPodmanProvider *self)
{
  self->label_to_type = g_array_new (FALSE, FALSE, sizeof (LabelToType));
}

PromptContainerProvider *
prompt_podman_provider_new (void)
{
  PromptPodmanProvider *self;

  self = g_object_new (PROMPT_TYPE_PODMAN_PROVIDER, NULL);
  g_clear_handle_id (&self->queued_update, g_source_remove);
  prompt_podman_provider_update_sync (self, NULL, NULL);

  return PROMPT_CONTAINER_PROVIDER (self);
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

static gboolean
label_matches (JsonNode          *node,
               const LabelToType *l_to_t)
{
  if (l_to_t->value != NULL)
    return JSON_NODE_HOLDS_VALUE (node) &&
           json_node_get_value_type (node) == G_TYPE_STRING &&
           g_strcmp0 (l_to_t->value, json_node_get_string (node)) == 0;

  return TRUE;
}

static PromptPodmanContainer *
prompt_podman_provider_deserialize (PromptPodmanProvider *self,
                                    JsonObject           *object)
{
  g_autoptr(PromptPodmanContainer) container = NULL;
  g_autoptr(GError) error = NULL;
  JsonObject *labels_object;
  JsonNode *labels;
  GType gtype;

  g_assert (PROMPT_IS_PODMAN_PROVIDER (self));
  g_assert (object != NULL);

  gtype = PROMPT_TYPE_PODMAN_CONTAINER;

  if (json_object_has_member (object, "Labels") &&
      (labels = json_object_get_member (object, "Labels")) &&
      JSON_NODE_HOLDS_OBJECT (labels) &&
      (labels_object = json_node_get_object (labels)))
    {
      for (guint i = 0; i < self->label_to_type->len; i++)
        {
          const LabelToType *l_to_t = &g_array_index (self->label_to_type, LabelToType, i);

          if (json_object_has_member (labels_object, l_to_t->label))
            {
              JsonNode *match = json_object_get_member (labels_object, l_to_t->label);

              if (label_matches (match, l_to_t))
                {
                  gtype = l_to_t->type;
                  break;
                }
            }
        }
    }

  container = g_object_new (gtype, NULL);

  if (!prompt_podman_container_deserialize (container, object, &error))
    {
      g_critical ("Failed to deserialize container JSON: %s", error->message);
      return NULL;
    }

  return g_steal_pointer (&container);
}

static void
prompt_podman_provider_parse_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  JsonParser *parser = (JsonParser *)object;
  PromptPodmanProvider *self;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GPtrArray) containers = NULL;
  g_autoptr(GError) error = NULL;
  JsonArray *root_array;
  JsonNode *root;

  g_assert (JSON_IS_PARSER (parser));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);

  if (!json_parser_load_from_stream_finish (parser, result, &error))
    {
      g_critical ("Failed to load podman JSON: %s", error->message);
      g_task_return_boolean (task, FALSE);
      return;
    }

  containers = g_ptr_array_new_with_free_func (g_object_unref);

  if ((root = json_parser_get_root (parser)) &&
      JSON_NODE_HOLDS_ARRAY (root) &&
      (root_array = json_node_get_array (root)))
    {
      guint n_elements = json_array_get_length (root_array);

      for (guint i = 0; i < n_elements; i++)
        {
          g_autoptr(PromptPodmanContainer) container = NULL;
          JsonNode *element = json_array_get_element (root_array, i);
          JsonObject *element_object;

          if (JSON_NODE_HOLDS_OBJECT (element) &&
              (element_object = json_node_get_object (element)) &&
              (container = prompt_podman_provider_deserialize (self, element_object)))
            g_ptr_array_add (containers, g_steal_pointer (&container));
        }
    }

  prompt_container_provider_merge (PROMPT_CONTAINER_PROVIDER (self), containers);

  g_task_return_boolean (task, TRUE);
}

static gboolean
prompt_podman_provider_update_source_func (gpointer user_data)
{
  PromptPodmanProvider *self = user_data;
  g_autoptr(PromptRunContext) run_context = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GIOStream) stream = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(JsonParser) parser = NULL;
  g_autoptr(GTask) task = NULL;
  GOutputStream *stdin_stream;
  GInputStream *stdout_stream;

  g_assert (PROMPT_IS_PODMAN_PROVIDER (self));

  self->queued_update = 0;

  run_context = prompt_run_context_new ();
  prompt_run_context_append_argv (run_context, "podman");
  prompt_run_context_append_argv (run_context, "ps");
  prompt_run_context_append_argv (run_context, "--all");
  prompt_run_context_append_argv (run_context, "--format=json");

  prompt_run_context_take_fd (run_context,
                              open ("/dev/null", O_RDWR|O_CLOEXEC),
                              STDERR_FILENO);

  if (!(stream = prompt_run_context_create_stdio_stream (run_context, &error)) ||
      !(subprocess = prompt_run_context_spawn (run_context, &error)))
    {
      g_critical ("Failed to spawn subprocess: %s", error->message);
      return G_SOURCE_REMOVE;
    }

  stdout_stream = g_io_stream_get_input_stream (stream);
  stdin_stream = g_io_stream_get_output_stream (stream);

  g_assert (!g_io_stream_is_closed (stream));
  g_assert (!g_input_stream_is_closed (stdout_stream));
  g_assert (!g_output_stream_is_closed (stdin_stream));

  task = g_task_new (self, NULL, NULL, NULL);
  g_task_set_source_tag (task, prompt_podman_provider_update_source_func);
  g_task_set_task_data (task, g_object_ref (stream), g_object_unref);

  parser = json_parser_new ();

  json_parser_load_from_stream_async (parser,
                                      stdout_stream,
                                      NULL,
                                      prompt_podman_provider_parse_cb,
                                      g_steal_pointer (&task));

  return G_SOURCE_REMOVE;
}

void
prompt_podman_provider_queue_update (PromptPodmanProvider *self)
{
  g_return_if_fail (PROMPT_IS_PODMAN_PROVIDER (self));

  if (self->queued_update == 0)
    self->queued_update = g_idle_add_full (G_PRIORITY_LOW,
                                           prompt_podman_provider_update_source_func,
                                           self, NULL);
}

gboolean
prompt_podman_provider_update_sync (PromptPodmanProvider  *self,
                                    GCancellable          *cancellable,
                                    GError               **error)
{
  g_autoptr(PromptRunContext) run_context = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GIOStream) stream = NULL;
  g_autoptr(GPtrArray) containers = NULL;
  g_autoptr(JsonParser) parser = NULL;
  JsonArray *root_array;
  JsonNode *root;
  GOutputStream *stdin_stream;
  GInputStream *stdout_stream;

  g_return_val_if_fail (PROMPT_IS_PODMAN_PROVIDER (self), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  run_context = prompt_run_context_new ();
  prompt_run_context_append_argv (run_context, "podman");
  prompt_run_context_append_argv (run_context, "ps");
  prompt_run_context_append_argv (run_context, "--all");
  prompt_run_context_append_argv (run_context, "--format=json");

  prompt_run_context_take_fd (run_context,
                              open ("/dev/null", O_RDWR|O_CLOEXEC),
                              STDERR_FILENO);

  if (!(stream = prompt_run_context_create_stdio_stream (run_context, error)) ||
      !(subprocess = prompt_run_context_spawn (run_context, error)))
    return FALSE;

  stdout_stream = g_io_stream_get_input_stream (stream);
  stdin_stream = g_io_stream_get_output_stream (stream);

  g_assert (!g_io_stream_is_closed (stream));
  g_assert (!g_input_stream_is_closed (stdout_stream));
  g_assert (!g_output_stream_is_closed (stdin_stream));

  parser = json_parser_new ();

  if (!json_parser_load_from_stream (parser, stdout_stream, cancellable, error))
    return FALSE;

  containers = g_ptr_array_new_with_free_func (g_object_unref);

  if ((root = json_parser_get_root (parser)) &&
      JSON_NODE_HOLDS_ARRAY (root) &&
      (root_array = json_node_get_array (root)))
    {
      guint n_elements = json_array_get_length (root_array);

      for (guint i = 0; i < n_elements; i++)
        {
          g_autoptr(PromptPodmanContainer) container = NULL;
          JsonNode *element = json_array_get_element (root_array, i);
          JsonObject *element_object;

          if (JSON_NODE_HOLDS_OBJECT (element) &&
              (element_object = json_node_get_object (element)) &&
              (container = prompt_podman_provider_deserialize (self, element_object)))
            g_ptr_array_add (containers, g_steal_pointer (&container));
        }
    }

  prompt_container_provider_merge (PROMPT_CONTAINER_PROVIDER (self), containers);

  return TRUE;
}
