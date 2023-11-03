/*
 * capsule-profile.c
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

#include <glib/gi18n.h>

#include <gio/gio.h>

#include "capsule-application.h"
#include "capsule-profile.h"

#define CAPSULE_PROFILE_KEY_AUDIBLE_BELL        "audible-bell"
#define CAPSULE_PROFILE_KEY_FONT_NAME           "font-name"
#define CAPSULE_PROFILE_KEY_LABEL               "label"
#define CAPSULE_PROFILE_KEY_SCROLL_ON_KEYSTROKE "scroll-on-keystroke"
#define CAPSULE_PROFILE_KEY_SCROLL_ON_OUTPUT    "scroll-on-output"
#define CAPSULE_PROFILE_KEY_USE_SYSTEM_FONT     "use-system-font"

struct _CapsuleProfile
{
  GObject parent_instance;
  GSettings *settings;
  char *uuid;
};

enum {
  PROP_0,
  PROP_AUDIBLE_BELL,
  PROP_FONT_DESC,
  PROP_FONT_NAME,
  PROP_LABEL,
  PROP_SCROLL_ON_KEYSTROKE,
  PROP_SCROLL_ON_OUTPUT,
  PROP_USE_SYSTEM_FONT,
  PROP_UUID,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (CapsuleProfile, capsule_profile, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
capsule_profile_changed_cb (CapsuleProfile *self,
                            const char     *key,
                            GSettings      *settings)
{
  g_assert (CAPSULE_IS_PROFILE (self));
  g_assert (key != NULL);
  g_assert (G_IS_SETTINGS (settings));

  if (FALSE) {}
  else if (g_str_equal (key, CAPSULE_PROFILE_KEY_FONT_NAME))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FONT_NAME]);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FONT_DESC]);
    }
  else if (g_str_equal (key, CAPSULE_PROFILE_KEY_USE_SYSTEM_FONT))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_USE_SYSTEM_FONT]);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FONT_DESC]);
    }
  else if (g_str_equal (key, CAPSULE_PROFILE_KEY_LABEL))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LABEL]);
    }
  else if (g_str_equal (key, CAPSULE_PROFILE_KEY_AUDIBLE_BELL))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_AUDIBLE_BELL]);
    }
}

static void
capsule_profile_constructed (GObject *object)
{
  CapsuleProfile *self = (CapsuleProfile *)object;
  g_autofree char *path = NULL;

  G_OBJECT_CLASS (capsule_profile_parent_class)->constructed (object);

  if (self->uuid == NULL)
    self->uuid = g_dbus_generate_guid ();

  path = g_strdup_printf (APP_SCHEMA_PATH"Profiles/%s/", self->uuid);
  self->settings = g_settings_new_with_path (APP_SCHEMA_PROFILE_ID, path);

  g_signal_connect_object (self->settings,
                           "changed",
                           G_CALLBACK (capsule_profile_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
capsule_profile_dispose (GObject *object)
{
  CapsuleProfile *self = (CapsuleProfile *)object;

  g_clear_object (&self->settings);
  g_clear_pointer (&self->uuid, g_free);

  G_OBJECT_CLASS (capsule_profile_parent_class)->dispose (object);
}

static void
capsule_profile_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  CapsuleProfile *self = CAPSULE_PROFILE (object);

  switch (prop_id)
    {
    case PROP_AUDIBLE_BELL:
      g_value_set_boolean (value, capsule_profile_get_audible_bell (self));
      break;

    case PROP_FONT_DESC:
      g_value_take_boxed (value, capsule_profile_dup_font_desc (self));
      break;

    case PROP_FONT_NAME:
      g_value_take_string (value, capsule_profile_dup_font_name (self));
      break;

    case PROP_LABEL:
      g_value_take_string (value, capsule_profile_dup_label (self));
      break;

    case PROP_SCROLL_ON_KEYSTROKE:
      g_value_set_boolean (value, capsule_profile_get_scroll_on_keystroke (self));
      break;

    case PROP_SCROLL_ON_OUTPUT:
      g_value_set_boolean (value, capsule_profile_get_scroll_on_output (self));
      break;

    case PROP_USE_SYSTEM_FONT:
      g_value_set_boolean (value, capsule_profile_get_use_system_font (self));
      break;

    case PROP_UUID:
      g_value_set_string (value, capsule_profile_get_uuid (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
capsule_profile_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  CapsuleProfile *self = CAPSULE_PROFILE (object);

  switch (prop_id)
    {
    case PROP_AUDIBLE_BELL:
      capsule_profile_set_audible_bell (self, g_value_get_boolean (value));
      break;

    case PROP_FONT_DESC:
      capsule_profile_set_font_desc (self, g_value_get_boxed (value));
      break;

    case PROP_FONT_NAME:
      capsule_profile_set_font_name (self, g_value_get_string (value));
      break;

    case PROP_LABEL:
      capsule_profile_set_label (self, g_value_get_string (value));
      break;

    case PROP_SCROLL_ON_KEYSTROKE:
      capsule_profile_set_scroll_on_keystroke (self, g_value_get_boolean (value));
      break;

    case PROP_SCROLL_ON_OUTPUT:
      capsule_profile_set_scroll_on_output (self, g_value_get_boolean (value));
      break;

    case PROP_USE_SYSTEM_FONT:
      capsule_profile_set_use_system_font (self, g_value_get_boolean (value));
      break;

    case PROP_UUID:
      self->uuid = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
capsule_profile_class_init (CapsuleProfileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = capsule_profile_constructed;
  object_class->dispose = capsule_profile_dispose;
  object_class->get_property = capsule_profile_get_property;
  object_class->set_property = capsule_profile_set_property;

  properties[PROP_AUDIBLE_BELL] =
    g_param_spec_boolean ("audible-bell", NULL, NULL,
                          FALSE,
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
    g_param_spec_string ("font-name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_LABEL] =
    g_param_spec_string ("label", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_SCROLL_ON_KEYSTROKE] =
    g_param_spec_boolean ("scroll-on-keystroke", NULL, NULL,
                         FALSE,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_SCROLL_ON_OUTPUT] =
    g_param_spec_boolean ("scroll-on-output", NULL, NULL,
                         FALSE,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_USE_SYSTEM_FONT] =
    g_param_spec_boolean ("use-system-font", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_UUID] =
    g_param_spec_string ("uuid", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
capsule_profile_init (CapsuleProfile *self)
{
}

CapsuleProfile *
capsule_profile_new (const char *uuid)
{
  return g_object_new (CAPSULE_TYPE_PROFILE,
                       "uuid", uuid,
                       NULL);
}

/**
 * capsule_profile_get_uuid:
 * @self: a #CapsuleProfile
 *
 * Gets the UUID for the profile.
 *
 * Returns: (not nullable): the profile UUID
 */
const char *
capsule_profile_get_uuid (CapsuleProfile *self)
{
  g_return_val_if_fail (CAPSULE_IS_PROFILE (self), NULL);

  return self->uuid;
}

char *
capsule_profile_dup_font_name (CapsuleProfile *self)
{
  g_return_val_if_fail (CAPSULE_IS_PROFILE (self), NULL);

  return g_settings_get_string (self->settings, CAPSULE_PROFILE_KEY_FONT_NAME);
}

void
capsule_profile_set_font_name (CapsuleProfile *self,
                               const char     *font_name)
{
  g_return_if_fail (CAPSULE_IS_PROFILE (self));

  if (font_name == NULL)
    font_name = "";

  g_settings_set_string (self->settings, CAPSULE_PROFILE_KEY_FONT_NAME, font_name);
}

gboolean
capsule_profile_get_use_system_font (CapsuleProfile *self)
{
  g_return_val_if_fail (CAPSULE_IS_PROFILE (self), FALSE);

  return g_settings_get_boolean (self->settings, CAPSULE_PROFILE_KEY_USE_SYSTEM_FONT);
}

void
capsule_profile_set_use_system_font (CapsuleProfile *self,
                                     gboolean        use_system_font)
{
  g_return_if_fail (CAPSULE_IS_PROFILE (self));

  g_settings_set_boolean (self->settings, CAPSULE_PROFILE_KEY_USE_SYSTEM_FONT, use_system_font);
}

PangoFontDescription *
capsule_profile_dup_font_desc (CapsuleProfile *self)
{
  CapsuleApplication *app;
  g_autofree char *font_name = NULL;
  const char *system_font_name;

  g_return_val_if_fail (CAPSULE_IS_PROFILE (self), NULL);

  app = CAPSULE_APPLICATION_DEFAULT;
  system_font_name = capsule_application_get_system_font_name (app);

  if (capsule_profile_get_use_system_font (self) ||
      !(font_name = capsule_profile_dup_font_name (self)) ||
      font_name[0] == 0)
    return pango_font_description_from_string (system_font_name);

  return pango_font_description_from_string (font_name);
}

void
capsule_profile_set_font_desc (CapsuleProfile             *self,
                               const PangoFontDescription *font_desc)
{
  g_autofree char *font_name = NULL;

  g_return_if_fail (CAPSULE_IS_PROFILE (self));

  if (font_desc != NULL)
    font_name = pango_font_description_to_string (font_desc);

  capsule_profile_set_font_name (self, font_name);
}

char *
capsule_profile_dup_label (CapsuleProfile *self)
{
  g_autofree char *label = NULL;

  g_return_val_if_fail (CAPSULE_IS_PROFILE (self), NULL);

  label = g_settings_get_string (self->settings, CAPSULE_PROFILE_KEY_LABEL);

  if (label == NULL || label[0] == 0)
    g_set_str (&label, _("Untitled Profile"));

  return g_steal_pointer (&label);
}

void
capsule_profile_set_label (CapsuleProfile *self,
                           const char     *label)
{
  g_return_if_fail (CAPSULE_IS_PROFILE (self));

  if (label == NULL)
    label = "";

  g_settings_set_string (self->settings, CAPSULE_PROFILE_KEY_LABEL, label);
}

gboolean
capsule_profile_get_audible_bell (CapsuleProfile *self)
{
  g_return_val_if_fail (CAPSULE_IS_PROFILE (self), FALSE);

  return g_settings_get_boolean (self->settings, CAPSULE_PROFILE_KEY_AUDIBLE_BELL);
}

void
capsule_profile_set_audible_bell (CapsuleProfile *self,
                                  gboolean        audible_bell)
{
  g_return_if_fail (CAPSULE_IS_PROFILE (self));

  g_settings_set_boolean (self->settings,
                          CAPSULE_PROFILE_KEY_AUDIBLE_BELL,
                          audible_bell);
}

gboolean
capsule_profile_get_scroll_on_keystroke (CapsuleProfile *self)
{
  g_return_val_if_fail (CAPSULE_IS_PROFILE (self), FALSE);

  return g_settings_get_boolean (self->settings, CAPSULE_PROFILE_KEY_SCROLL_ON_KEYSTROKE);
}

void
capsule_profile_set_scroll_on_keystroke (CapsuleProfile *self,
                                         gboolean        scroll_on_keystroke)
{
  g_return_if_fail (CAPSULE_IS_PROFILE (self));

  g_settings_set_boolean (self->settings,
                          CAPSULE_PROFILE_KEY_SCROLL_ON_KEYSTROKE,
                          scroll_on_keystroke);
}

gboolean
capsule_profile_get_scroll_on_output (CapsuleProfile *self)
{
  g_return_val_if_fail (CAPSULE_IS_PROFILE (self), FALSE);

  return g_settings_get_boolean (self->settings, CAPSULE_PROFILE_KEY_SCROLL_ON_OUTPUT);
}

void
capsule_profile_set_scroll_on_output (CapsuleProfile *self,
                                      gboolean        scroll_on_output)
{
  g_return_if_fail (CAPSULE_IS_PROFILE (self));

  g_settings_set_boolean (self->settings,
                          CAPSULE_PROFILE_KEY_SCROLL_ON_OUTPUT,
                          scroll_on_output);
}
