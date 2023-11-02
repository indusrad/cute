/*
 * capsule-terminal.c
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

#include "capsule-terminal.h"

struct _CapsuleTerminal
{
  VteTerminal parent_instance;
  CapsuleProfile *profile;
};

enum {
  PROP_0,
  PROP_PROFILE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (CapsuleTerminal, capsule_terminal, VTE_TYPE_TERMINAL)

static GParamSpec *properties [N_PROPS];

static void
capsule_terminal_dispose (GObject *object)
{
  CapsuleTerminal *self = (CapsuleTerminal *)object;

  g_clear_object (&self->profile);

  G_OBJECT_CLASS (capsule_terminal_parent_class)->dispose (object);
}

static void
capsule_terminal_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  CapsuleTerminal *self = CAPSULE_TERMINAL (object);

  switch (prop_id)
    {
    case PROP_PROFILE:
      g_value_set_object (value, capsule_terminal_get_profile (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
capsule_terminal_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  CapsuleTerminal *self = CAPSULE_TERMINAL (object);

  switch (prop_id)
    {
    case PROP_PROFILE:
      self->profile = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
capsule_terminal_class_init (CapsuleTerminalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = capsule_terminal_dispose;
  object_class->get_property = capsule_terminal_get_property;
  object_class->set_property = capsule_terminal_set_property;

  properties [PROP_PROFILE] =
    g_param_spec_object ("profile", NULL, NULL,
                         CAPSULE_TYPE_PROFILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
capsule_terminal_init (CapsuleTerminal *self)
{
}

CapsuleTerminal *
capsule_terminal_new (CapsuleProfile *profile)
{
  g_return_val_if_fail (CAPSULE_IS_PROFILE (profile), NULL);

  return g_object_new (CAPSULE_TYPE_TERMINAL,
                       "profile", profile,
                       NULL);
}

/**
 * capsule_terminal_get_profile:
 *
 * Returns: (transfer none) (not nullable): a #CapsuleProfile
 */
CapsuleProfile *
capsule_terminal_get_profile (CapsuleTerminal *self)
{
  g_return_val_if_fail (CAPSULE_IS_TERMINAL (self), NULL);

  return self->profile;
}
