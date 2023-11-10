/*
 * prompt-container.c
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

#include "prompt-application.h"
#include "prompt-container.h"
#include "prompt-settings.h"
#include "prompt-user.h"
#include "prompt-util.h"

typedef struct _PromptContainerSpawn
{
  VtePty            *pty;
  PromptProfile    *profile;
  PromptRunContext *run_context;
  char              *current_directory_uri;
} PromptContainerSpawn;

G_DEFINE_ABSTRACT_TYPE (PromptContainer, prompt_container, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ID,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
prompt_container_spawn_free (PromptContainerSpawn *spawn)
{
  g_clear_object (&spawn->pty);
  g_clear_object (&spawn->profile);
  g_clear_object (&spawn->run_context);
  g_clear_pointer (&spawn->current_directory_uri, g_free);
  g_free (spawn);
}

static void
prompt_container_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  PromptContainer *self = PROMPT_CONTAINER (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_string (value, prompt_container_get_id (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_container_class_init (PromptContainerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = prompt_container_get_property;

  properties [PROP_ID] =
    g_param_spec_string ("id", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
prompt_container_init (PromptContainer *self)
{
}

const char *
prompt_container_get_id (PromptContainer *self)
{
  g_return_val_if_fail (PROMPT_IS_CONTAINER (self), NULL);

  return PROMPT_CONTAINER_GET_CLASS (self)->get_id (self);
}

void
prompt_container_prepare_async (PromptContainer     *self,
                                PromptRunContext    *run_context,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  PROMPT_CONTAINER_GET_CLASS (self)->prepare_async (self, run_context, cancellable, callback, user_data);
}

gboolean
prompt_container_prepare_finish (PromptContainer  *self,
                                 GAsyncResult     *result,
                                 GError          **error)
{
  return PROMPT_CONTAINER_GET_CLASS (self)->prepare_finish (self, result, error);
}

static void
prompt_container_spawn_discover_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  PromptUser *user = (PromptUser *)object;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  PromptContainerSpawn *state;
  PromptSettings *settings;
  g_autofree char *default_shell = NULL;
  g_auto(GStrv) proxyenv = NULL;

  g_assert (PROMPT_IS_USER (user));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  state = g_task_get_task_data (task);

  g_assert (PROMPT_IS_USER (user));
  g_assert (state != NULL);
  g_assert (VTE_IS_PTY (state->pty));
  g_assert (PROMPT_IS_PROFILE (state->profile));
  g_assert (PROMPT_IS_RUN_CONTEXT (state->run_context));

  default_shell = prompt_user_discover_shell_finish (user, result, NULL);

  settings = prompt_application_get_settings (PROMPT_APPLICATION_DEFAULT);
  proxyenv = prompt_settings_get_proxy_environment (settings);
  if (proxyenv != NULL)
    prompt_run_context_add_environ (state->run_context,
                                    (const char * const *)proxyenv);

  if (!prompt_profile_apply (state->profile,
                             state->run_context,
                             state->pty,
                             state->current_directory_uri,
                             default_shell,
                             &error) ||
      !(subprocess = prompt_run_context_spawn (state->run_context, &error)))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_pointer (task, g_steal_pointer (&subprocess), g_object_unref);
}

static void
prompt_container_spawn_prepare_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  PromptContainer *self = (PromptContainer *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  PromptContainerSpawn *state;
  PromptUser *user;

  g_assert (PROMPT_IS_CONTAINER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  user = prompt_user_get_default ();
  state = g_task_get_task_data (task);

  g_assert (PROMPT_IS_USER (user));
  g_assert (state != NULL);
  g_assert (VTE_IS_PTY (state->pty));
  g_assert (PROMPT_IS_PROFILE (state->profile));
  g_assert (PROMPT_IS_RUN_CONTEXT (state->run_context));

  if (!prompt_container_prepare_finish (self, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    prompt_user_discover_shell_async (user,
                                       g_task_get_cancellable (task),
                                       prompt_container_spawn_discover_cb,
                                       g_object_ref (task));
}

void
prompt_container_spawn_async (PromptContainer     *self,
                              VtePty              *pty,
                              PromptProfile       *profile,
                              const char          *current_directory_uri,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  g_autoptr(PromptRunContext) run_context = NULL;
  g_autoptr(GTask) task = NULL;
  PromptContainerSpawn *state;

  g_return_if_fail (PROMPT_IS_CONTAINER (self));
  g_return_if_fail (VTE_IS_PTY (pty));
  g_return_if_fail (PROMPT_IS_PROFILE (profile));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  run_context = prompt_run_context_new ();

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, prompt_container_spawn_async);

  state = g_new0 (PromptContainerSpawn, 1);
  g_set_object (&state->pty, pty);
  g_set_object (&state->profile, profile);
  g_set_object (&state->run_context, run_context);
  state->current_directory_uri = g_strdup (current_directory_uri);
  g_task_set_task_data (task,
                        g_steal_pointer (&state),
                        (GDestroyNotify)prompt_container_spawn_free);

  prompt_container_prepare_async (self,
                                   run_context,
                                   cancellable,
                                   prompt_container_spawn_prepare_cb,
                                   g_steal_pointer (&task));
}

GSubprocess *
prompt_container_spawn_finish (PromptContainer  *self,
                               GAsyncResult     *result,
                               GError          **error)
{
  g_return_val_if_fail (PROMPT_IS_CONTAINER (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}
