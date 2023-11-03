/*
 * capsule-container.c
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

#include "capsule-container.h"
#include "capsule-user.h"
#include "capsule-util.h"

typedef struct _CapsuleContainerSpawn
{
  VtePty *pty;
  CapsuleProfile *profile;
  CapsuleRunContext *run_context;
} CapsuleContainerSpawn;

G_DEFINE_ABSTRACT_TYPE (CapsuleContainer, capsule_container, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ID,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
capsule_container_spawn_free (CapsuleContainerSpawn *spawn)
{
  g_clear_object (&spawn->pty);
  g_clear_object (&spawn->profile);
  g_clear_object (&spawn->run_context);
  g_free (spawn);
}

static void
capsule_container_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  CapsuleContainer *self = CAPSULE_CONTAINER (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_string (value, capsule_container_get_id (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
capsule_container_class_init (CapsuleContainerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = capsule_container_get_property;

  properties [PROP_ID] =
    g_param_spec_string ("id", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
capsule_container_init (CapsuleContainer *self)
{
}

const char *
capsule_container_get_id (CapsuleContainer *self)
{
  g_return_val_if_fail (CAPSULE_IS_CONTAINER (self), NULL);

  return CAPSULE_CONTAINER_GET_CLASS (self)->get_id (self);
}

void
capsule_container_prepare_async (CapsuleContainer    *self,
                                 CapsuleRunContext   *run_context,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  CAPSULE_CONTAINER_GET_CLASS (self)->prepare_async (self, run_context, cancellable, callback, user_data);
}

gboolean
capsule_container_prepare_finish (CapsuleContainer  *self,
                                  GAsyncResult      *result,
                                  GError           **error)
{
  return CAPSULE_CONTAINER_GET_CLASS (self)->prepare_finish (self, result, error);
}

static void
capsule_container_apply_profile (CapsuleContainer  *self,
                                 CapsuleRunContext *run_context,
                                 VtePty            *pty,
                                 CapsuleProfile    *profile,
                                 const char        *default_shell)
{
  g_assert (CAPSULE_IS_CONTAINER (self));
  g_assert (CAPSULE_IS_RUN_CONTEXT (run_context));
  g_assert (VTE_IS_PTY (pty));
  g_assert (CAPSULE_IS_PROFILE (profile));
  g_assert (default_shell != NULL);

  capsule_run_context_set_pty (run_context, pty);

  if (default_shell != NULL)
    capsule_run_context_append_argv (run_context, default_shell);
  else
    capsule_run_context_append_argv (run_context, "/bin/sh");

  capsule_run_context_set_cwd (run_context, g_get_home_dir ());
}

static void
capsule_container_spawn_discover_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  CapsuleUser *user = (CapsuleUser *)object;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  CapsuleContainerSpawn *state;
  g_autofree char *default_shell = NULL;

  g_assert (CAPSULE_IS_USER (user));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  state = g_task_get_task_data (task);

  g_assert (CAPSULE_IS_USER (user));
  g_assert (state != NULL);
  g_assert (VTE_IS_PTY (state->pty));
  g_assert (CAPSULE_IS_PROFILE (state->profile));
  g_assert (CAPSULE_IS_RUN_CONTEXT (state->run_context));

  default_shell = capsule_user_discover_shell_finish (user, result, NULL);

  capsule_container_apply_profile (g_task_get_source_object (task),
                                   state->run_context,
                                   state->pty,
                                   state->profile,
                                   default_shell);

  if (!(subprocess = capsule_run_context_spawn (state->run_context, &error)))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_pointer (task, g_steal_pointer (&subprocess), g_object_unref);
}

static void
capsule_container_spawn_prepare_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  CapsuleContainer *self = (CapsuleContainer *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  CapsuleContainerSpawn *state;
  CapsuleUser *user;

  g_assert (CAPSULE_IS_CONTAINER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  user = capsule_user_get_default ();
  state = g_task_get_task_data (task);

  g_assert (CAPSULE_IS_USER (user));
  g_assert (state != NULL);
  g_assert (VTE_IS_PTY (state->pty));
  g_assert (CAPSULE_IS_PROFILE (state->profile));
  g_assert (CAPSULE_IS_RUN_CONTEXT (state->run_context));

  if (!capsule_container_prepare_finish (self, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    capsule_user_discover_shell_async (user,
                                       g_task_get_cancellable (task),
                                       capsule_container_spawn_discover_cb,
                                       g_object_ref (task));
}

void
capsule_container_spawn_async (CapsuleContainer    *self,
                               VtePty              *pty,
                               CapsuleProfile      *profile,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_autoptr(CapsuleRunContext) run_context = NULL;
  g_autoptr(GTask) task = NULL;
  CapsuleContainerSpawn *state;

  g_return_if_fail (CAPSULE_IS_CONTAINER (self));
  g_return_if_fail (VTE_IS_PTY (pty));
  g_return_if_fail (CAPSULE_IS_PROFILE (profile));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  run_context = capsule_run_context_new ();

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, capsule_container_spawn_async);

  state = g_new0 (CapsuleContainerSpawn, 1);
  g_set_object (&state->pty, pty);
  g_set_object (&state->profile, profile);
  g_set_object (&state->run_context, run_context);
  g_task_set_task_data (task,
                        g_steal_pointer (&state),
                        (GDestroyNotify)capsule_container_spawn_free);

  capsule_container_prepare_async (self,
                                   run_context,
                                   cancellable,
                                   capsule_container_spawn_prepare_cb,
                                   g_steal_pointer (&task));
}

GSubprocess *
capsule_container_spawn_finish (CapsuleContainer  *self,
                                GAsyncResult      *result,
                                GError           **error)
{
  g_return_val_if_fail (CAPSULE_IS_CONTAINER (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}
