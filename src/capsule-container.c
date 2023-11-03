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
#include "capsule-util.h"

G_DEFINE_ABSTRACT_TYPE (CapsuleContainer, capsule_container, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ID,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

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
capsule_container_spawn_async (CapsuleContainer    *self,
                               VtePty              *pty,
                               CapsuleProfile      *profile,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  CAPSULE_CONTAINER_GET_CLASS (self)->spawn_async (self, pty, profile, cancellable, callback, user_data);
}

GSubprocess *
capsule_container_spawn_finish (CapsuleContainer  *self,
                                GAsyncResult      *result,
                                GError           **error)
{
  return CAPSULE_CONTAINER_GET_CLASS (self)->spawn_finish (self, result, error);
}

void
capsule_container_prepare_run_context (CapsuleContainer  *self,
                                       CapsuleRunContext *run_context,
                                       CapsuleProfile    *profile)
{
  const char *user_shell;

  g_return_if_fail (CAPSULE_IS_CONTAINER (self));
  g_return_if_fail (CAPSULE_IS_RUN_CONTEXT (run_context));
  g_return_if_fail (CAPSULE_IS_PROFILE (profile));

  user_shell = capsule_get_user_shell ();

  if (user_shell == NULL)
    user_shell = "/bin/sh";

  capsule_run_context_append_argv (run_context, user_shell);
}
