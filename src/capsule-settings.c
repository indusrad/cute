/*
 * capsule-settings.c
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

#include <gio/gio.h>

#include "capsule-settings.h"

#define CAPSULE_SETTING_KEY_DEFAULT_PROFILE_UUID "default-profile-uuid"
#define CAPSULE_SETTING_KEY_PROFILE_UUIDS        "profile-uuids"

struct _CapsuleSettings
{
  GObject    parent_instance;
  GSettings *settings;
};

enum {
  PROP_0,
  PROP_DEFAULT_PROFILE_UUID,
  PROP_PROFILE_UUIDS,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (CapsuleSettings, capsule_settings, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
capsule_settings_changed_cb (CapsuleSettings *self,
                             const char      *key,
                             GSettings       *settings)
{
  g_assert (CAPSULE_IS_SETTINGS (self));
  g_assert (key != NULL);
  g_assert (G_IS_SETTINGS (settings));

  if (g_str_equal (key, CAPSULE_SETTING_KEY_DEFAULT_PROFILE_UUID))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DEFAULT_PROFILE_UUID]);
  else if (g_str_equal (key, CAPSULE_SETTING_KEY_PROFILE_UUIDS))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PROFILE_UUIDS]);
}

static void
capsule_settings_dispose (GObject *object)
{
  CapsuleSettings *self = (CapsuleSettings *)object;

  g_clear_object (&self->settings);

  G_OBJECT_CLASS (capsule_settings_parent_class)->dispose (object);
}

static void
capsule_settings_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  CapsuleSettings *self = CAPSULE_SETTINGS (object);

  switch (prop_id)
    {
    case PROP_DEFAULT_PROFILE_UUID:
      g_value_take_string (value, capsule_settings_dup_default_profile_uuid (self));
      break;

    case PROP_PROFILE_UUIDS:
      g_value_take_boxed (value, capsule_settings_dup_profile_uuids (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
capsule_settings_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  CapsuleSettings *self = CAPSULE_SETTINGS (object);

  switch (prop_id)
    {
    case PROP_DEFAULT_PROFILE_UUID:
      capsule_settings_set_default_profile_uuid (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
capsule_settings_class_init (CapsuleSettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = capsule_settings_dispose;
  object_class->get_property = capsule_settings_get_property;
  object_class->set_property = capsule_settings_set_property;

  properties [PROP_DEFAULT_PROFILE_UUID] =
    g_param_spec_string ("default-profile-uuid", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_PROFILE_UUIDS] =
    g_param_spec_boxed ("profile-uuids", NULL, NULL,
                        G_TYPE_STRV,
                        (G_PARAM_READABLE |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
capsule_settings_init (CapsuleSettings *self)
{
  self->settings = g_settings_new (APP_SCHEMA_ID);

  g_signal_connect_object (self->settings,
                           "changed",
                           G_CALLBACK (capsule_settings_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  /* Prime all settings to ensure we get change notifications. GSettings
   * backends should only notify for keys we've requested already.
   */
  g_free (g_settings_get_string (self->settings, CAPSULE_SETTING_KEY_DEFAULT_PROFILE_UUID));
  g_strfreev (g_settings_get_strv (self->settings, CAPSULE_SETTING_KEY_PROFILE_UUIDS));
}

CapsuleSettings *
capsule_settings_new (void)
{
  return g_object_new (CAPSULE_TYPE_SETTINGS, NULL);
}

void
capsule_settings_set_default_profile_uuid (CapsuleSettings *self,
                                           const char      *default_profile_uuid)
{
  g_return_if_fail (CAPSULE_IS_SETTINGS (self));
  g_return_if_fail (default_profile_uuid != NULL);

  g_settings_set_string (self->settings,
                         CAPSULE_SETTING_KEY_DEFAULT_PROFILE_UUID,
                         default_profile_uuid);
}

char *
capsule_settings_dup_default_profile_uuid (CapsuleSettings *self)
{
  g_return_val_if_fail (CAPSULE_IS_SETTINGS (self), NULL);

  return g_settings_get_string (self->settings,
                                CAPSULE_SETTING_KEY_DEFAULT_PROFILE_UUID);
}

char **
capsule_settings_dup_profile_uuids (CapsuleSettings *self)
{
  g_return_val_if_fail (CAPSULE_IS_SETTINGS (self), NULL);

  return g_settings_get_strv (self->settings,
                              CAPSULE_SETTING_KEY_PROFILE_UUIDS);
}

void
capsule_settings_add_profile_uuid (CapsuleSettings *self,
                                   const char      *uuid)
{
  g_auto(GStrv) profiles = NULL;
  gsize len;

  g_return_if_fail (CAPSULE_IS_SETTINGS (self));
  g_return_if_fail (uuid != NULL);

  profiles = g_settings_get_strv (self->settings,
                                  CAPSULE_SETTING_KEY_PROFILE_UUIDS);

  if (g_strv_contains ((const char * const *)profiles, uuid))
    return;

  len = g_strv_length (profiles);
  profiles = g_realloc_n (profiles, len + 2, sizeof (char *));
  profiles[len] = g_strdup (uuid);
  profiles[len+1] = NULL;

  g_settings_set_strv (self->settings,
                       CAPSULE_SETTING_KEY_PROFILE_UUIDS,
                       (const char * const *)profiles);
}

void
capsule_settings_remove_profile_uuid (CapsuleSettings *self,
                                      const char      *uuid)
{
  g_auto(GStrv) profiles = NULL;
  g_autoptr(GStrvBuilder) builder = NULL;
  g_autofree char *default_profile_uuid = NULL;

  g_return_if_fail (CAPSULE_IS_SETTINGS (self));
  g_return_if_fail (uuid != NULL);

  default_profile_uuid = g_settings_get_string (self->settings,
                                                CAPSULE_SETTING_KEY_DEFAULT_PROFILE_UUID);
  profiles = g_settings_get_strv (self->settings,
                                  CAPSULE_SETTING_KEY_PROFILE_UUIDS);

  builder = g_strv_builder_new ();
  for (guint i = 0; profiles[i]; i++)
    {
      if (!g_str_equal (profiles[i], uuid))
        g_strv_builder_add (builder, profiles[i]);
    }

  g_clear_pointer (&profiles, g_strfreev);
  profiles = g_strv_builder_end (builder);

  g_settings_set_strv (self->settings,
                       CAPSULE_SETTING_KEY_PROFILE_UUIDS,
                       (const char * const *)profiles);
  if (g_str_equal (uuid, default_profile_uuid))
    g_settings_set_string (self->settings,
                           CAPSULE_SETTING_KEY_DEFAULT_PROFILE_UUID,
                           "");
}
