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

#include "capsule-application.h"
#include "capsule-enums.h"
#include "capsule-settings.h"

struct _CapsuleSettings
{
  GObject    parent_instance;
  GSettings *settings;
};

enum {
  PROP_0,
  PROP_AUDIBLE_BELL,
  PROP_CURSOR_BLINK_MODE,
  PROP_CURSOR_SHAPE,
  PROP_DEFAULT_PROFILE_UUID,
  PROP_FONT_DESC,
  PROP_FONT_NAME,
  PROP_NEW_TAB_POSITION,
  PROP_PROFILE_UUIDS,
  PROP_USE_SYSTEM_FONT,
  PROP_VISUAL_BELL,
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
  else if (g_str_equal (key, CAPSULE_SETTING_KEY_NEW_TAB_POSITION))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NEW_TAB_POSITION]);
  else if (g_str_equal (key, CAPSULE_SETTING_KEY_AUDIBLE_BELL))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_AUDIBLE_BELL]);
  else if (g_str_equal (key, CAPSULE_SETTING_KEY_VISUAL_BELL))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VISUAL_BELL]);
  else if (g_str_equal (key, CAPSULE_SETTING_KEY_CURSOR_SHAPE))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CURSOR_SHAPE]);
  else if (g_str_equal (key, CAPSULE_SETTING_KEY_CURSOR_BLINK_MODE))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CURSOR_BLINK_MODE]);
  else if (g_str_equal (key, CAPSULE_SETTING_KEY_FONT_NAME))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FONT_NAME]);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FONT_DESC]);
    }
  else if (g_str_equal (key, CAPSULE_SETTING_KEY_USE_SYSTEM_FONT))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_USE_SYSTEM_FONT]);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FONT_DESC]);
    }
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
    case PROP_AUDIBLE_BELL:
      g_value_set_boolean (value, capsule_settings_get_audible_bell (self));
      break;

    case PROP_CURSOR_BLINK_MODE:
      g_value_set_enum (value, capsule_settings_get_cursor_blink_mode (self));
      break;

    case PROP_CURSOR_SHAPE:
      g_value_set_enum (value, capsule_settings_get_cursor_shape (self));
      break;

    case PROP_DEFAULT_PROFILE_UUID:
      g_value_take_string (value, capsule_settings_dup_default_profile_uuid (self));
      break;

    case PROP_FONT_DESC:
      g_value_take_boxed (value, capsule_settings_dup_font_desc (self));
      break;

    case PROP_FONT_NAME:
      g_value_take_string (value, capsule_settings_dup_font_name (self));
      break;

    case PROP_NEW_TAB_POSITION:
      g_value_set_enum (value, capsule_settings_get_new_tab_position (self));
      break;

    case PROP_PROFILE_UUIDS:
      g_value_take_boxed (value, capsule_settings_dup_profile_uuids (self));
      break;

    case PROP_USE_SYSTEM_FONT:
      g_value_set_boolean (value, capsule_settings_get_use_system_font (self));
      break;

    case PROP_VISUAL_BELL:
      g_value_set_boolean (value, capsule_settings_get_visual_bell (self));
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
    case PROP_AUDIBLE_BELL:
      capsule_settings_set_audible_bell (self, g_value_get_boolean (value));
      break;

    case PROP_CURSOR_BLINK_MODE:
      capsule_settings_set_cursor_blink_mode (self, g_value_get_enum (value));
      break;

    case PROP_CURSOR_SHAPE:
      capsule_settings_set_cursor_shape (self, g_value_get_enum (value));
      break;

    case PROP_FONT_DESC:
      capsule_settings_set_font_desc (self, g_value_get_boxed (value));
      break;

    case PROP_FONT_NAME:
      capsule_settings_set_font_name (self, g_value_get_string (value));
      break;

    case PROP_NEW_TAB_POSITION:
      capsule_settings_set_new_tab_position (self, g_value_get_enum (value));
      break;

    case PROP_DEFAULT_PROFILE_UUID:
      capsule_settings_set_default_profile_uuid (self, g_value_get_string (value));
      break;

    case PROP_USE_SYSTEM_FONT:
      capsule_settings_set_use_system_font (self, g_value_get_boolean (value));
      break;

    case PROP_VISUAL_BELL:
      capsule_settings_set_visual_bell (self, g_value_get_boolean (value));
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

  properties[PROP_AUDIBLE_BELL] =
    g_param_spec_boolean ("audible-bell", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_CURSOR_BLINK_MODE] =
    g_param_spec_enum ("cursor-blink-mode", NULL, NULL,
                       VTE_TYPE_CURSOR_BLINK_MODE,
                       VTE_CURSOR_BLINK_SYSTEM,
                       (G_PARAM_READWRITE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_CURSOR_SHAPE] =
    g_param_spec_enum ("cursor-shape", NULL, NULL,
                       VTE_TYPE_CURSOR_SHAPE,
                       VTE_CURSOR_SHAPE_BLOCK,
                       (G_PARAM_READWRITE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_FONT_DESC] =
    g_param_spec_boxed ("font-desc", NULL, NULL,
                        PANGO_TYPE_FONT_DESCRIPTION,
                        (G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  properties[PROP_FONT_NAME] =
    g_param_spec_string ("font-string", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_NEW_TAB_POSITION] =
    g_param_spec_enum ("new-tab-position", NULL, NULL,
                       CAPSULE_TYPE_NEW_TAB_POSITION,
                       CAPSULE_NEW_TAB_POSITION_LAST,
                       (G_PARAM_READWRITE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_DEFAULT_PROFILE_UUID] =
    g_param_spec_string ("default-profile-uuid", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_PROFILE_UUIDS] =
    g_param_spec_boxed ("profile-uuids", NULL, NULL,
                        G_TYPE_STRV,
                        (G_PARAM_READABLE |
                         G_PARAM_STATIC_STRINGS));

  properties[PROP_USE_SYSTEM_FONT] =
    g_param_spec_boolean ("use-system-font", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_VISUAL_BELL] =
    g_param_spec_boolean ("visual-bell", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
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

  /* Make sure we have at least one profile */
  if (profiles[0] == NULL)
    {
      profiles = g_realloc_n (profiles, 2, sizeof (char *));
      profiles[0] = g_dbus_generate_guid ();
      profiles[1] = NULL;
    }

  g_settings_set_strv (self->settings,
                       CAPSULE_SETTING_KEY_PROFILE_UUIDS,
                       (const char * const *)profiles);

  if (g_str_equal (uuid, default_profile_uuid))
    g_settings_set_string (self->settings,
                           CAPSULE_SETTING_KEY_DEFAULT_PROFILE_UUID,
                           profiles[0]);
}

CapsuleNewTabPosition
capsule_settings_get_new_tab_position (CapsuleSettings *self)
{
  g_return_val_if_fail (CAPSULE_IS_SETTINGS (self), 0);

  return g_settings_get_enum (self->settings, CAPSULE_SETTING_KEY_NEW_TAB_POSITION);
}

void
capsule_settings_set_new_tab_position (CapsuleSettings       *self,
                                       CapsuleNewTabPosition  new_tab_position)
{
  g_return_if_fail (CAPSULE_IS_SETTINGS (self));

  g_settings_set_enum (self->settings,
                       CAPSULE_SETTING_KEY_NEW_TAB_POSITION,
                       new_tab_position);
}

GSettings *
capsule_settings_get_settings (CapsuleSettings *self)
{
  g_return_val_if_fail (CAPSULE_IS_SETTINGS (self), NULL);

  return self->settings;
}

gboolean
capsule_settings_get_audible_bell (CapsuleSettings *self)
{
  g_return_val_if_fail (CAPSULE_IS_SETTINGS (self), FALSE);

  return g_settings_get_boolean (self->settings, CAPSULE_SETTING_KEY_AUDIBLE_BELL);
}

void
capsule_settings_set_audible_bell (CapsuleSettings *self,
                                   gboolean         audible_bell)
{
  g_return_if_fail (CAPSULE_IS_SETTINGS (self));

  g_settings_set_boolean (self->settings,
                          CAPSULE_SETTING_KEY_AUDIBLE_BELL,
                          audible_bell);
}

gboolean
capsule_settings_get_visual_bell (CapsuleSettings *self)
{
  g_return_val_if_fail (CAPSULE_IS_SETTINGS (self), FALSE);

  return g_settings_get_boolean (self->settings, CAPSULE_SETTING_KEY_VISUAL_BELL);
}

void
capsule_settings_set_visual_bell (CapsuleSettings *self,
                                  gboolean         visual_bell)
{
  g_return_if_fail (CAPSULE_IS_SETTINGS (self));

  g_settings_set_boolean (self->settings,
                          CAPSULE_SETTING_KEY_VISUAL_BELL,
                          visual_bell);
}

VteCursorBlinkMode
capsule_settings_get_cursor_blink_mode (CapsuleSettings *self)
{
  g_return_val_if_fail (CAPSULE_IS_SETTINGS (self), 0);

  return g_settings_get_enum (self->settings, CAPSULE_SETTING_KEY_CURSOR_BLINK_MODE);
}

void
capsule_settings_set_cursor_blink_mode (CapsuleSettings    *self,
                                        VteCursorBlinkMode  cursor_blink_mode)
{
  g_return_if_fail (CAPSULE_IS_SETTINGS (self));

  g_settings_set_enum (self->settings,
                       CAPSULE_SETTING_KEY_CURSOR_BLINK_MODE,
                       cursor_blink_mode);
}

VteCursorShape
capsule_settings_get_cursor_shape (CapsuleSettings *self)
{
  g_return_val_if_fail (CAPSULE_IS_SETTINGS (self), 0);

  return g_settings_get_enum (self->settings, CAPSULE_SETTING_KEY_CURSOR_SHAPE);
}

void
capsule_settings_set_cursor_shape (CapsuleSettings *self,
                                   VteCursorShape   cursor_shape)
{
  g_return_if_fail (CAPSULE_IS_SETTINGS (self));

  g_settings_set_enum (self->settings,
                       CAPSULE_SETTING_KEY_CURSOR_SHAPE,
                       cursor_shape);
}

char *
capsule_settings_dup_font_name (CapsuleSettings *self)
{
  g_return_val_if_fail (CAPSULE_IS_SETTINGS (self), NULL);

  return g_settings_get_string (self->settings, CAPSULE_SETTING_KEY_FONT_NAME);
}

void
capsule_settings_set_font_name (CapsuleSettings *self,
                                const char      *font_name)
{
  g_return_if_fail (CAPSULE_IS_SETTINGS (self));

  if (font_name == NULL)
    font_name = "";

  g_settings_set_string (self->settings, CAPSULE_SETTING_KEY_FONT_NAME, font_name);
}

gboolean
capsule_settings_get_use_system_font (CapsuleSettings *self)
{
  g_return_val_if_fail (CAPSULE_IS_SETTINGS (self), FALSE);

  return g_settings_get_boolean (self->settings, CAPSULE_SETTING_KEY_USE_SYSTEM_FONT);
}

void
capsule_settings_set_use_system_font (CapsuleSettings *self,
                                      gboolean         use_system_font)
{
  g_return_if_fail (CAPSULE_IS_SETTINGS (self));

  g_settings_set_boolean (self->settings, CAPSULE_SETTING_KEY_USE_SYSTEM_FONT, use_system_font);
}

PangoFontDescription *
capsule_settings_dup_font_desc (CapsuleSettings *self)
{
  CapsuleApplication *app;
  g_autofree char *font_name = NULL;
  const char *system_font_name;

  g_return_val_if_fail (CAPSULE_IS_SETTINGS (self), NULL);

  app = CAPSULE_APPLICATION_DEFAULT;
  system_font_name = capsule_application_get_system_font_name (app);

  if (capsule_settings_get_use_system_font (self) ||
      !(font_name = capsule_settings_dup_font_name (self)) ||
      font_name[0] == 0)
    return pango_font_description_from_string (system_font_name);

  return pango_font_description_from_string (font_name);
}

void
capsule_settings_set_font_desc (CapsuleSettings            *self,
                                const PangoFontDescription *font_desc)
{
  g_autofree char *font_name = NULL;

  g_return_if_fail (CAPSULE_IS_SETTINGS (self));

  if (font_desc != NULL)
    font_name = pango_font_description_to_string (font_desc);

  capsule_settings_set_font_name (self, font_name);
}

