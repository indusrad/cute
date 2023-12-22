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

#include <unistd.h>

#include <glib/gstdio.h>

#include "prompt-agent-util.h"
#include "prompt-distrobox-container.h"
#include "prompt-podman-container.h"
#include "prompt-process-impl.h"
#include "prompt-run-context.h"
#include "prompt-toolbox-container.h"

typedef struct
{
  GHashTable *labels;
  gboolean has_started;
} PromptPodmanContainerPrivate;

static void container_iface_init (PromptIpcContainerIface *iface);

G_DEFINE_TYPE_WITH_CODE (PromptPodmanContainer, prompt_podman_container, PROMPT_IPC_TYPE_CONTAINER_SKELETON,
                         G_ADD_PRIVATE (PromptPodmanContainer)
                         G_IMPLEMENT_INTERFACE (PROMPT_IPC_TYPE_CONTAINER, container_iface_init))

static void
maybe_start_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
  GSubprocess *subprocess = (GSubprocess *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!g_subprocess_wait_check_finish (subprocess, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);
}

static void
maybe_start (PromptPodmanContainer *self,
             GCancellable          *cancellable,
             GAsyncReadyCallback    callback,
             gpointer               user_data)
{
  PromptPodmanContainerPrivate *priv = prompt_podman_container_get_instance_private (self);
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = NULL;
  const char *id;

  g_assert (PROMPT_IS_PODMAN_CONTAINER (self));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, maybe_start);

  id = prompt_ipc_container_get_id (PROMPT_IPC_CONTAINER (self));
  g_assert (id != NULL && id[0] != 0);

  if (priv->has_started)
    {
      g_task_return_boolean (task, TRUE);
      return;
    }

  priv->has_started = TRUE;

  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDERR_SILENCE |
                                        G_SUBPROCESS_FLAGS_STDOUT_SILENCE);

  /* Wait so that we don't try to run before the pod has started */
  if ((subprocess = g_subprocess_launcher_spawn (launcher, &error, "podman", "start", id, NULL)))
    g_subprocess_wait_check_async (subprocess,
                                   cancellable,
                                   maybe_start_cb,
                                   g_steal_pointer (&task));
  else
    g_task_return_error (task, g_steal_pointer (&error));
}

static gboolean
maybe_start_finish (PromptPodmanContainer  *self,
                    GAsyncResult           *result,
                    GError                **error)
{
  g_assert (PROMPT_IS_PODMAN_CONTAINER (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

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

static void
prompt_podman_container_deserialize_name (PromptPodmanContainer *self,
                                          JsonArray             *names)
{
  g_assert (PROMPT_IS_PODMAN_CONTAINER (self));
  g_assert (names != NULL);

  if (json_array_get_length (names) > 0)
    {
      JsonNode *element = json_array_get_element (names, 0);

      if (element != NULL &&
          JSON_NODE_HOLDS_VALUE (element) &&
          json_node_get_value_type (element) == G_TYPE_STRING)
        prompt_ipc_container_set_display_name (PROMPT_IPC_CONTAINER (self),
                                               json_node_get_string (element));
    }
}

static gboolean
prompt_podman_container_real_deserialize (PromptPodmanContainer  *self,
                                          JsonObject             *object,
                                          GError                **error)
{
  JsonObject *labels_object;
  JsonArray *names_array;
  JsonNode *names;
  JsonNode *labels;
  JsonNode *id;

  g_assert (PROMPT_IS_PODMAN_CONTAINER (self));
  g_assert (object != NULL);

  if (!(json_object_has_member (object, "Id") &&
        (id = json_object_get_member (object, "Id")) &&
        JSON_NODE_HOLDS_VALUE (id) &&
        json_node_get_value_type (id) == G_TYPE_STRING))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "Failed to locate Id in podman container description");
      return FALSE;
    }

  prompt_ipc_container_set_id (PROMPT_IPC_CONTAINER (self),
                               json_node_get_string (id));

  if (json_object_has_member (object, "Labels") &&
      (labels = json_object_get_member (object, "Labels")) &&
      JSON_NODE_HOLDS_OBJECT (labels) &&
      (labels_object = json_node_get_object (labels)))
    prompt_podman_container_deserialize_labels (self, labels_object);

  if (json_object_has_member (object, "Names") &&
      (names = json_object_get_member (object, "Names")) &&
      JSON_NODE_HOLDS_ARRAY (names) &&
      (names_array = json_node_get_array (names)))
    prompt_podman_container_deserialize_name (self, names_array);

  return TRUE;
}

static gboolean
prompt_podman_container_run_context_cb (PromptRunContext    *run_context,
                                        const char * const  *argv,
                                        const char * const  *env,
                                        const char          *cwd,
                                        PromptUnixFDMap     *unix_fd_map,
                                        gpointer             user_data,
                                        GError             **error)
{
  PromptPodmanContainer *self = user_data;
  const char *id;
  gboolean has_tty = FALSE;
  int max_dest_fd;

  g_assert (PROMPT_IS_PODMAN_CONTAINER (self));
  g_assert (PROMPT_IS_RUN_CONTEXT (run_context));
  g_assert (argv != NULL);
  g_assert (env != NULL);
  g_assert (PROMPT_IS_UNIX_FD_MAP (unix_fd_map));

  id = prompt_ipc_container_get_id (PROMPT_IPC_CONTAINER (self));

  /* Make sure that we request TTY ioctls if necessary */
  if (prompt_unix_fd_map_stdin_isatty (unix_fd_map) ||
      prompt_unix_fd_map_stdout_isatty (unix_fd_map) ||
      prompt_unix_fd_map_stderr_isatty (unix_fd_map))
    has_tty = TRUE;

  /* Make sure we can pass the FDs down */
  if (!prompt_run_context_merge_unix_fd_map (run_context, unix_fd_map, error))
    return FALSE;

  /* Setup basic podman-exec command */
  prompt_run_context_append_argv (run_context, "podman");
  prompt_run_context_append_argv (run_context, "exec");
  prompt_run_context_append_argv (run_context, "--privileged");
  prompt_run_context_append_argv (run_context, "--interactive");

  /* Make sure that we request TTY ioctls if necessary */
  if (has_tty)
    prompt_run_context_append_argv (run_context, "--tty");

  /* If there is a CWD specified, then apply it. However, podman containers
   * wont necessarily have the user home directory in them except for when
   * using toolbox/distrobox. So only apply in those cases.
   */
  if (PROMPT_IS_TOOLBOX_CONTAINER (self) || PROMPT_IS_DISTROBOX_CONTAINER (self))
    {
      prompt_run_context_append_formatted (run_context, "--user=%s", g_get_user_name ());
      if (cwd != NULL)
        prompt_run_context_append_formatted (run_context, "--workdir=%s", cwd);
    }

  /* From podman-exec(1):
   *
   * Pass down to the process N additional file descriptors (in addition to
   * 0, 1, 2).  The total FDs will be 3+N.
   */
  if ((max_dest_fd = prompt_unix_fd_map_get_max_dest_fd (unix_fd_map)) > 2)
    prompt_run_context_append_formatted (run_context, "--preserve-fds=%d", max_dest_fd-2);

  /* Append --env=FOO=BAR environment variables */
  for (guint i = 0; env[i]; i++)
    prompt_run_context_append_formatted (run_context, "--env=%s", env[i]);

  /* Now specify our runtime identifier */
  prompt_run_context_append_argv (run_context, id);

  /* Finally, propagate the upper layer's command arguments */
  prompt_run_context_append_args (run_context, argv);

  return TRUE;
}

static void
prompt_podman_container_real_prepare_run_context (PromptPodmanContainer *self,
                                                  PromptRunContext      *run_context)
{
  g_assert (PROMPT_IS_PODMAN_CONTAINER (self));
  g_assert (PROMPT_IS_RUN_CONTEXT (run_context));

  prompt_run_context_push (run_context,
                           prompt_podman_container_run_context_cb,
                           g_object_ref (self),
                           g_object_unref);

  /* Give acces to some minimal state in the environment from
   * our host system.
   */
  prompt_run_context_add_minimal_environment (run_context);

  /* We don't want HOME propagated because it could be different
   * inside the toolbox/distrobox and that can set it up for us.
   */
  prompt_run_context_setenv (run_context, "HOME", NULL);
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
prompt_podman_container_class_init (PromptPodmanContainerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = prompt_podman_container_dispose;
  object_class->finalize = prompt_podman_container_finalize;

  klass->deserialize = prompt_podman_container_real_deserialize;
  klass->prepare_run_context = prompt_podman_container_real_prepare_run_context;
}

static void
prompt_podman_container_init (PromptPodmanContainer *self)
{
  PromptPodmanContainerPrivate *priv = prompt_podman_container_get_instance_private (self);

  priv->labels = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  prompt_ipc_container_set_icon_name (PROMPT_IPC_CONTAINER (self), "container-podman-symbolic");
  prompt_ipc_container_set_provider (PROMPT_IPC_CONTAINER (self), "podman");
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

static void
prompt_podman_container_handle_spawn_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  PromptPodmanContainer *self = (PromptPodmanContainer *)object;
  g_autoptr(PromptRunContext) run_context = user_data;
  g_autoptr(PromptIpcProcess) process = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GUnixFDList) out_fd_list = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *object_path = NULL;
  g_autofree char *guid = NULL;
  GDBusMethodInvocation *invocation;
  GDBusConnection *connection;

  g_assert (PROMPT_IS_PODMAN_CONTAINER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (PROMPT_IS_RUN_CONTEXT (run_context));

  invocation = g_object_get_data (G_OBJECT (run_context), "INVOCATION");
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  connection = g_dbus_method_invocation_get_connection (invocation);
  g_assert (G_IS_DBUS_CONNECTION (connection));

  out_fd_list = g_unix_fd_list_new ();
  guid = g_dbus_generate_guid ();
  object_path = g_strdup_printf ("/org/gnome/Prompt/Process/%s", guid);

  if (!maybe_start_finish (self, result, &error) ||
      !(subprocess = prompt_run_context_spawn (run_context, &error)) ||
      !(process = prompt_process_impl_new (connection, subprocess, object_path, &error)))
    {
      g_dbus_method_invocation_return_gerror (g_object_ref (invocation), error);
      return;
    }

  prompt_ipc_container_complete_spawn (PROMPT_IPC_CONTAINER (self),
                                       g_object_ref (invocation),
                                       out_fd_list,
                                       object_path);
}

static gboolean
prompt_podman_container_handle_spawn (PromptIpcContainer    *container,
                                      GDBusMethodInvocation *invocation,
                                      GUnixFDList           *in_fd_list,
                                      const char            *cwd,
                                      const char * const    *argv,
                                      GVariant              *in_fds,
                                      GVariant              *in_env)
{
  PromptPodmanContainer *self = (PromptPodmanContainer *)container;
  g_autoptr(PromptRunContext) run_context = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (PROMPT_IS_PODMAN_CONTAINER (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (G_IS_UNIX_FD_LIST (in_fd_list));
  g_assert (cwd != NULL);
  g_assert (argv != NULL);
  g_assert (in_fds != NULL);
  g_assert (in_env != NULL);

  run_context = prompt_run_context_new ();

  /* Allow subclass to hook up different execution strategy */
  PROMPT_PODMAN_CONTAINER_GET_CLASS (self)->prepare_run_context (self, run_context);

  /* Now do our normal handling of the layer requested by the user. */
  prompt_agent_push_spawn (run_context, in_fd_list, cwd, argv, in_fds, in_env);

  g_object_set_data_full (G_OBJECT (run_context),
                          "INVOCATION",
                          g_steal_pointer (&invocation),
                          g_object_unref);

  maybe_start (PROMPT_PODMAN_CONTAINER (container),
               NULL,
               prompt_podman_container_handle_spawn_cb,
               g_steal_pointer (&run_context));

  return TRUE;
}

typedef struct
{
  GDBusMethodInvocation *invocation;
  PromptPodmanContainer *container;
  char *id;
  char *program;
} FindContainerInPath;

static void
find_container_in_path_free (FindContainerInPath *state)
{
  g_clear_object (&state->container);
  g_clear_object (&state->invocation);
  g_clear_pointer (&state->id, g_free);
  g_clear_pointer (&state->program, g_free);
  g_free (state);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (FindContainerInPath, find_container_in_path_free)

static void
prompt_podman_container_which_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  GSubprocess *subprocess = (GSubprocess *)object;
  g_autoptr(FindContainerInPath) state = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree char *stdout_buf = NULL;

  g_assert (G_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (state != NULL);
  g_assert (PROMPT_IS_PODMAN_CONTAINER (state->container));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (state->invocation));
  g_assert (state->id != NULL);
  g_assert (state->program != NULL);

  if (!g_subprocess_communicate_utf8_finish (subprocess, result, &stdout_buf, NULL, &error))
    g_dbus_method_invocation_return_gerror (g_steal_pointer (&state->invocation),
                                            g_steal_pointer (&error));
  else
    prompt_ipc_container_complete_find_program_in_path (PROMPT_IPC_CONTAINER (state->container),
                                                        g_steal_pointer (&state->invocation),
                                                        g_strstrip (stdout_buf));
}

static void
prompt_podman_container_find_program_in_path_start_cb (GObject      *object,
                                                       GAsyncResult *result,
                                                       gpointer      user_data)
{
  PromptPodmanContainer *self = (PromptPodmanContainer *)object;
  g_autoptr(FindContainerInPath) state = user_data;
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (PROMPT_IS_PODMAN_CONTAINER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (state != NULL);
  g_assert (PROMPT_IS_PODMAN_CONTAINER (state->container));
  g_assert (state->container == self);
  g_assert (G_IS_DBUS_METHOD_INVOCATION (state->invocation));
  g_assert (state->id != NULL);
  g_assert (state->program != NULL);

  if (!maybe_start_finish (self, result, &error))
    {
      g_dbus_method_invocation_return_gerror (g_steal_pointer (&state->invocation),
                                              g_steal_pointer (&error));
      return;
    }

  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE);
  subprocess = g_subprocess_launcher_spawn (launcher, &error,
                                            "podman", "exec", state->id,
                                            "which", state->program,
                                            NULL);

  if (subprocess == NULL)
    g_dbus_method_invocation_return_gerror (g_steal_pointer (&state->invocation),
                                            g_steal_pointer (&error));
  else
    g_subprocess_communicate_utf8_async (subprocess,
                                         NULL,
                                         NULL,
                                         prompt_podman_container_which_cb,
                                         g_steal_pointer (&state));
}

static gboolean
prompt_podman_container_handle_find_program_in_path (PromptIpcContainer    *container,
                                                     GDBusMethodInvocation *invocation,
                                                     const char            *program)
{
  FindContainerInPath *state;

  g_autoptr(GError) error = NULL;
  const char *id;

  g_assert (PROMPT_IS_PODMAN_CONTAINER (container));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  id = prompt_ipc_container_get_id (container);

  g_assert (id != NULL);
  g_assert (id[0] != 0);

  state = g_new0 (FindContainerInPath, 1);
  state->invocation = g_steal_pointer (&invocation);
  state->id = g_strdup (prompt_ipc_container_get_id (container));
  state->program = g_strdup (program);
  state->container = g_object_ref (PROMPT_PODMAN_CONTAINER (container));

  maybe_start (PROMPT_PODMAN_CONTAINER (container),
               NULL,
               prompt_podman_container_find_program_in_path_start_cb,
               state);

  return TRUE;
}

static void
container_iface_init (PromptIpcContainerIface *iface)
{
  iface->handle_spawn = prompt_podman_container_handle_spawn;
  iface->handle_find_program_in_path = prompt_podman_container_handle_find_program_in_path;
}
