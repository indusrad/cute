/*
 * prompt-settings.c
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

#include "prompt-application.h"
#include "prompt-enums.h"
#include "prompt-settings.h"
#include "prompt-util.h"

struct _PromptSettings
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
  PROP_INTERFACE_STYLE,
  PROP_NEW_TAB_POSITION,
  PROP_PROFILE_UUIDS,
  PROP_RESTORE_SESSION,
  PROP_RESTORE_WINDOW_SIZE,
  PROP_SCROLLBAR_POLICY,
  PROP_TEXT_BLINK_MODE,
  PROP_TOAST_ON_COPY_CLIPBOARD,
  PROP_USE_SYSTEM_FONT,
  PROP_VISUAL_BELL,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PromptSettings, prompt_settings, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static inline gboolean
strempty (const char *s)
{
  return !s || !s[0];
}

static void
prompt_settings_changed_cb (PromptSettings *self,
                            const char     *key,
                            GSettings      *settings)
{
  g_assert (PROMPT_IS_SETTINGS (self));
  g_assert (key != NULL);
  g_assert (G_IS_SETTINGS (settings));

  if (g_str_equal (key, PROMPT_SETTING_KEY_DEFAULT_PROFILE_UUID))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DEFAULT_PROFILE_UUID]);
  else if (g_str_equal (key, PROMPT_SETTING_KEY_PROFILE_UUIDS))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PROFILE_UUIDS]);
  else if (g_str_equal (key, PROMPT_SETTING_KEY_NEW_TAB_POSITION))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NEW_TAB_POSITION]);
  else if (g_str_equal (key, PROMPT_SETTING_KEY_AUDIBLE_BELL))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_AUDIBLE_BELL]);
  else if (g_str_equal (key, PROMPT_SETTING_KEY_VISUAL_BELL))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VISUAL_BELL]);
  else if (g_str_equal (key, PROMPT_SETTING_KEY_CURSOR_SHAPE))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CURSOR_SHAPE]);
  else if (g_str_equal (key, PROMPT_SETTING_KEY_CURSOR_BLINK_MODE))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CURSOR_BLINK_MODE]);
  else if (g_str_equal (key, PROMPT_SETTING_KEY_SCROLLBAR_POLICY))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SCROLLBAR_POLICY]);
  else if (g_str_equal (key, PROMPT_SETTING_KEY_TEXT_BLINK_MODE))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TEXT_BLINK_MODE]);
  else if (g_str_equal (key, PROMPT_SETTING_KEY_INTERFACE_STYLE))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INTERFACE_STYLE]);
  else if (g_str_equal (key, PROMPT_SETTING_KEY_RESTORE_SESSION))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RESTORE_SESSION]);
  else if (g_str_equal (key, PROMPT_SETTING_KEY_RESTORE_WINDOW_SIZE))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RESTORE_WINDOW_SIZE]);
  else if (g_str_equal (key, PROMPT_SETTING_KEY_TOAST_ON_COPY_CLIPBOARD))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TOAST_ON_COPY_CLIPBOARD]);
  else if (g_str_equal (key, PROMPT_SETTING_KEY_FONT_NAME))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FONT_NAME]);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FONT_DESC]);
    }
  else if (g_str_equal (key, PROMPT_SETTING_KEY_USE_SYSTEM_FONT))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_USE_SYSTEM_FONT]);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FONT_DESC]);
    }
}

static void
prompt_settings_dispose (GObject *object)
{
  PromptSettings *self = (PromptSettings *)object;

  g_clear_object (&self->settings);

  G_OBJECT_CLASS (prompt_settings_parent_class)->dispose (object);
}

static void
prompt_settings_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  PromptSettings *self = PROMPT_SETTINGS (object);

  switch (prop_id)
    {
    case PROP_AUDIBLE_BELL:
      g_value_set_boolean (value, prompt_settings_get_audible_bell (self));
      break;

    case PROP_CURSOR_BLINK_MODE:
      g_value_set_enum (value, prompt_settings_get_cursor_blink_mode (self));
      break;

    case PROP_CURSOR_SHAPE:
      g_value_set_enum (value, prompt_settings_get_cursor_shape (self));
      break;

    case PROP_DEFAULT_PROFILE_UUID:
      g_value_take_string (value, prompt_settings_dup_default_profile_uuid (self));
      break;

    case PROP_FONT_DESC:
      g_value_take_boxed (value, prompt_settings_dup_font_desc (self));
      break;

    case PROP_INTERFACE_STYLE:
      g_value_set_enum (value, prompt_settings_get_interface_style (self));
      break;

    case PROP_FONT_NAME:
      g_value_take_string (value, prompt_settings_dup_font_name (self));
      break;

    case PROP_NEW_TAB_POSITION:
      g_value_set_enum (value, prompt_settings_get_new_tab_position (self));
      break;

    case PROP_PROFILE_UUIDS:
      g_value_take_boxed (value, prompt_settings_dup_profile_uuids (self));
      break;

    case PROP_RESTORE_SESSION:
      g_value_set_boolean (value, prompt_settings_get_restore_session (self));
      break;

    case PROP_RESTORE_WINDOW_SIZE:
      g_value_set_boolean (value, prompt_settings_get_restore_window_size (self));
      break;

    case PROP_SCROLLBAR_POLICY:
      g_value_set_enum (value, prompt_settings_get_scrollbar_policy (self));
      break;

    case PROP_TEXT_BLINK_MODE:
      g_value_set_enum (value, prompt_settings_get_text_blink_mode (self));
      break;

    case PROP_TOAST_ON_COPY_CLIPBOARD:
      g_value_set_boolean (value, prompt_settings_get_toast_on_copy_clipboard (self));
      break;

    case PROP_USE_SYSTEM_FONT:
      g_value_set_boolean (value, prompt_settings_get_use_system_font (self));
      break;

    case PROP_VISUAL_BELL:
      g_value_set_boolean (value, prompt_settings_get_visual_bell (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_settings_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  PromptSettings *self = PROMPT_SETTINGS (object);

  switch (prop_id)
    {
    case PROP_AUDIBLE_BELL:
      prompt_settings_set_audible_bell (self, g_value_get_boolean (value));
      break;

    case PROP_CURSOR_BLINK_MODE:
      prompt_settings_set_cursor_blink_mode (self, g_value_get_enum (value));
      break;

    case PROP_CURSOR_SHAPE:
      prompt_settings_set_cursor_shape (self, g_value_get_enum (value));
      break;

    case PROP_FONT_DESC:
      prompt_settings_set_font_desc (self, g_value_get_boxed (value));
      break;

    case PROP_FONT_NAME:
      prompt_settings_set_font_name (self, g_value_get_string (value));
      break;

    case PROP_INTERFACE_STYLE:
      prompt_settings_set_interface_style (self, g_value_get_enum (value));
      break;

    case PROP_NEW_TAB_POSITION:
      prompt_settings_set_new_tab_position (self, g_value_get_enum (value));
      break;

    case PROP_DEFAULT_PROFILE_UUID:
      prompt_settings_set_default_profile_uuid (self, g_value_get_string (value));
      break;

    case PROP_RESTORE_SESSION:
      prompt_settings_set_restore_session (self, g_value_get_boolean (value));
      break;

    case PROP_RESTORE_WINDOW_SIZE:
      prompt_settings_set_restore_window_size (self, g_value_get_boolean (value));
      break;

    case PROP_SCROLLBAR_POLICY:
      prompt_settings_set_scrollbar_policy (self, g_value_get_enum (value));
      break;

    case PROP_TEXT_BLINK_MODE:
      prompt_settings_set_text_blink_mode (self, g_value_get_enum (value));
      break;

    case PROP_TOAST_ON_COPY_CLIPBOARD:
      prompt_settings_set_toast_on_copy_clipboard (self, g_value_get_boolean (value));
      break;

    case PROP_USE_SYSTEM_FONT:
      prompt_settings_set_use_system_font (self, g_value_get_boolean (value));
      break;

    case PROP_VISUAL_BELL:
      prompt_settings_set_visual_bell (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_settings_class_init (PromptSettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = prompt_settings_dispose;
  object_class->get_property = prompt_settings_get_property;
  object_class->set_property = prompt_settings_set_property;

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
    g_param_spec_string ("font-name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_INTERFACE_STYLE] =
    g_param_spec_enum ("interface-style", NULL, NULL,
                       ADW_TYPE_COLOR_SCHEME,
                       ADW_COLOR_SCHEME_DEFAULT,
                       (G_PARAM_READWRITE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_NEW_TAB_POSITION] =
    g_param_spec_enum ("new-tab-position", NULL, NULL,
                       PROMPT_TYPE_NEW_TAB_POSITION,
                       PROMPT_NEW_TAB_POSITION_LAST,
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

  properties[PROP_RESTORE_SESSION] =
    g_param_spec_boolean ("restore-session", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_RESTORE_WINDOW_SIZE] =
    g_param_spec_boolean ("restore-window-size", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_SCROLLBAR_POLICY] =
    g_param_spec_enum ("scrollbar-policy", NULL, NULL,
                       PROMPT_TYPE_SCROLLBAR_POLICY,
                       PROMPT_SCROLLBAR_POLICY_SYSTEM,
                       (G_PARAM_READWRITE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_TEXT_BLINK_MODE] =
    g_param_spec_enum ("text-blink-mode", NULL, NULL,
                       VTE_TYPE_TEXT_BLINK_MODE,
                       VTE_TEXT_BLINK_ALWAYS,
                       (G_PARAM_READWRITE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_TOAST_ON_COPY_CLIPBOARD] =
    g_param_spec_boolean ("toast-on-copy-clipboard", NULL, NULL,
                          TRUE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
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
prompt_settings_init (PromptSettings *self)
{
  self->settings = g_settings_new (APP_SCHEMA_ID);

  g_signal_connect_object (self->settings,
                           "changed",
                           G_CALLBACK (prompt_settings_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

PromptSettings *
prompt_settings_new (void)
{
  return g_object_new (PROMPT_TYPE_SETTINGS, NULL);
}

void
prompt_settings_set_default_profile_uuid (PromptSettings *self,
                                          const char     *default_profile_uuid)
{
  g_return_if_fail (PROMPT_IS_SETTINGS (self));
  g_return_if_fail (default_profile_uuid != NULL);

  g_settings_set_string (self->settings,
                         PROMPT_SETTING_KEY_DEFAULT_PROFILE_UUID,
                         default_profile_uuid);
}

char *
prompt_settings_dup_default_profile_uuid (PromptSettings *self)
{
  g_return_val_if_fail (PROMPT_IS_SETTINGS (self), NULL);

  return g_settings_get_string (self->settings,
                                PROMPT_SETTING_KEY_DEFAULT_PROFILE_UUID);
}

char **
prompt_settings_dup_profile_uuids (PromptSettings *self)
{
  g_return_val_if_fail (PROMPT_IS_SETTINGS (self), NULL);

  return g_settings_get_strv (self->settings,
                              PROMPT_SETTING_KEY_PROFILE_UUIDS);
}

void
prompt_settings_add_profile_uuid (PromptSettings *self,
                                  const char     *uuid)
{
  g_auto(GStrv) profiles = NULL;
  gsize len;

  g_return_if_fail (PROMPT_IS_SETTINGS (self));
  g_return_if_fail (uuid != NULL);

  profiles = g_settings_get_strv (self->settings,
                                  PROMPT_SETTING_KEY_PROFILE_UUIDS);

  if (g_strv_contains ((const char * const *)profiles, uuid))
    return;

  len = g_strv_length (profiles);
  profiles = g_realloc_n (profiles, len + 2, sizeof (char *));
  profiles[len] = g_strdup (uuid);
  profiles[len+1] = NULL;

  g_settings_set_strv (self->settings,
                       PROMPT_SETTING_KEY_PROFILE_UUIDS,
                       (const char * const *)profiles);
}

void
prompt_settings_remove_profile_uuid (PromptSettings *self,
                                     const char     *uuid)
{
  g_auto(GStrv) profiles = NULL;
  g_autoptr(GStrvBuilder) builder = NULL;
  g_autofree char *default_profile_uuid = NULL;

  g_return_if_fail (PROMPT_IS_SETTINGS (self));
  g_return_if_fail (uuid != NULL);

  default_profile_uuid = g_settings_get_string (self->settings,
                                                PROMPT_SETTING_KEY_DEFAULT_PROFILE_UUID);
  profiles = g_settings_get_strv (self->settings,
                                  PROMPT_SETTING_KEY_PROFILE_UUIDS);

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
                       PROMPT_SETTING_KEY_PROFILE_UUIDS,
                       (const char * const *)profiles);

  if (g_str_equal (uuid, default_profile_uuid))
    g_settings_set_string (self->settings,
                           PROMPT_SETTING_KEY_DEFAULT_PROFILE_UUID,
                           profiles[0]);
}

PromptNewTabPosition
prompt_settings_get_new_tab_position (PromptSettings *self)
{
  g_return_val_if_fail (PROMPT_IS_SETTINGS (self), 0);

  return g_settings_get_enum (self->settings, PROMPT_SETTING_KEY_NEW_TAB_POSITION);
}

void
prompt_settings_set_new_tab_position (PromptSettings       *self,
                                      PromptNewTabPosition  new_tab_position)
{
  g_return_if_fail (PROMPT_IS_SETTINGS (self));

  g_settings_set_enum (self->settings,
                       PROMPT_SETTING_KEY_NEW_TAB_POSITION,
                       new_tab_position);
}

GSettings *
prompt_settings_get_settings (PromptSettings *self)
{
  g_return_val_if_fail (PROMPT_IS_SETTINGS (self), NULL);

  return self->settings;
}

gboolean
prompt_settings_get_audible_bell (PromptSettings *self)
{
  g_return_val_if_fail (PROMPT_IS_SETTINGS (self), FALSE);

  return g_settings_get_boolean (self->settings, PROMPT_SETTING_KEY_AUDIBLE_BELL);
}

void
prompt_settings_set_audible_bell (PromptSettings *self,
                                  gboolean        audible_bell)
{
  g_return_if_fail (PROMPT_IS_SETTINGS (self));

  g_settings_set_boolean (self->settings,
                          PROMPT_SETTING_KEY_AUDIBLE_BELL,
                          audible_bell);
}

gboolean
prompt_settings_get_visual_bell (PromptSettings *self)
{
  g_return_val_if_fail (PROMPT_IS_SETTINGS (self), FALSE);

  return g_settings_get_boolean (self->settings, PROMPT_SETTING_KEY_VISUAL_BELL);
}

void
prompt_settings_set_visual_bell (PromptSettings *self,
                                 gboolean        visual_bell)
{
  g_return_if_fail (PROMPT_IS_SETTINGS (self));

  g_settings_set_boolean (self->settings,
                          PROMPT_SETTING_KEY_VISUAL_BELL,
                          visual_bell);
}

VteCursorBlinkMode
prompt_settings_get_cursor_blink_mode (PromptSettings *self)
{
  g_return_val_if_fail (PROMPT_IS_SETTINGS (self), 0);

  return g_settings_get_enum (self->settings, PROMPT_SETTING_KEY_CURSOR_BLINK_MODE);
}

void
prompt_settings_set_cursor_blink_mode (PromptSettings     *self,
                                       VteCursorBlinkMode  cursor_blink_mode)
{
  g_return_if_fail (PROMPT_IS_SETTINGS (self));

  g_settings_set_enum (self->settings,
                       PROMPT_SETTING_KEY_CURSOR_BLINK_MODE,
                       cursor_blink_mode);
}

VteCursorShape
prompt_settings_get_cursor_shape (PromptSettings *self)
{
  g_return_val_if_fail (PROMPT_IS_SETTINGS (self), 0);

  return g_settings_get_enum (self->settings, PROMPT_SETTING_KEY_CURSOR_SHAPE);
}

void
prompt_settings_set_cursor_shape (PromptSettings *self,
                                  VteCursorShape  cursor_shape)
{
  g_return_if_fail (PROMPT_IS_SETTINGS (self));

  g_settings_set_enum (self->settings,
                       PROMPT_SETTING_KEY_CURSOR_SHAPE,
                       cursor_shape);
}

char *
prompt_settings_dup_font_name (PromptSettings *self)
{
  g_return_val_if_fail (PROMPT_IS_SETTINGS (self), NULL);

  return g_settings_get_string (self->settings, PROMPT_SETTING_KEY_FONT_NAME);
}

void
prompt_settings_set_font_name (PromptSettings *self,
                               const char     *font_name)
{
  g_return_if_fail (PROMPT_IS_SETTINGS (self));

  if (font_name == NULL)
    font_name = "";

  g_settings_set_string (self->settings, PROMPT_SETTING_KEY_FONT_NAME, font_name);
}

gboolean
prompt_settings_get_use_system_font (PromptSettings *self)
{
  g_return_val_if_fail (PROMPT_IS_SETTINGS (self), FALSE);

  return g_settings_get_boolean (self->settings, PROMPT_SETTING_KEY_USE_SYSTEM_FONT);
}

void
prompt_settings_set_use_system_font (PromptSettings *self,
                                     gboolean        use_system_font)
{
  g_return_if_fail (PROMPT_IS_SETTINGS (self));

  g_settings_set_boolean (self->settings, PROMPT_SETTING_KEY_USE_SYSTEM_FONT, use_system_font);
}

PangoFontDescription *
prompt_settings_dup_font_desc (PromptSettings *self)
{
  PromptApplication *app;
  g_autofree char *font_name = NULL;
  const char *system_font_name;

  g_return_val_if_fail (PROMPT_IS_SETTINGS (self), NULL);

  app = PROMPT_APPLICATION_DEFAULT;
  system_font_name = prompt_application_get_system_font_name (app);

  if (prompt_settings_get_use_system_font (self) ||
      !(font_name = prompt_settings_dup_font_name (self)) ||
      prompt_str_empty0 (font_name))
    return pango_font_description_from_string (system_font_name);

  return pango_font_description_from_string (font_name);
}

void
prompt_settings_set_font_desc (PromptSettings             *self,
                               const PangoFontDescription *font_desc)
{
  g_autofree char *font_name = NULL;

  g_return_if_fail (PROMPT_IS_SETTINGS (self));

  if (font_desc != NULL)
    font_name = pango_font_description_to_string (font_desc);

  prompt_settings_set_font_name (self, font_name);
}

PromptScrollbarPolicy
prompt_settings_get_scrollbar_policy (PromptSettings *self)
{
  g_return_val_if_fail (PROMPT_IS_SETTINGS (self), 0);

  return g_settings_get_enum (self->settings, PROMPT_SETTING_KEY_SCROLLBAR_POLICY);
}

void
prompt_settings_set_scrollbar_policy (PromptSettings        *self,
                                      PromptScrollbarPolicy  scrollbar_policy)
{
  g_return_if_fail (PROMPT_IS_SETTINGS (self));

  g_settings_set_enum (self->settings,
                       PROMPT_SETTING_KEY_SCROLLBAR_POLICY,
                       scrollbar_policy);
}

VteTextBlinkMode
prompt_settings_get_text_blink_mode (PromptSettings *self)
{
  g_return_val_if_fail (PROMPT_IS_SETTINGS (self), 0);

  return g_settings_get_enum (self->settings, PROMPT_SETTING_KEY_TEXT_BLINK_MODE);
}

void
prompt_settings_set_text_blink_mode (PromptSettings   *self,
                                     VteTextBlinkMode  text_blink_mode)
{
  g_return_if_fail (PROMPT_IS_SETTINGS (self));

  g_settings_set_enum (self->settings,
                       PROMPT_SETTING_KEY_TEXT_BLINK_MODE,
                       text_blink_mode);
}

gboolean
prompt_settings_get_restore_session (PromptSettings *self)
{
  g_return_val_if_fail (PROMPT_IS_SETTINGS (self), FALSE);

  return g_settings_get_boolean (self->settings, PROMPT_SETTING_KEY_RESTORE_SESSION);
}

void
prompt_settings_set_restore_session (PromptSettings *self,
                                     gboolean        restore_session)
{
  g_return_if_fail (PROMPT_IS_SETTINGS (self));

  g_settings_set_boolean (self->settings,
                          PROMPT_SETTING_KEY_RESTORE_SESSION,
                          restore_session);
}

gboolean
prompt_settings_get_restore_window_size (PromptSettings *self)
{
  g_return_val_if_fail (PROMPT_IS_SETTINGS (self), FALSE);

  return g_settings_get_boolean (self->settings, PROMPT_SETTING_KEY_RESTORE_WINDOW_SIZE);
}

void
prompt_settings_set_restore_window_size (PromptSettings *self,
                                         gboolean        restore_window_size)
{
  g_return_if_fail (PROMPT_IS_SETTINGS (self));

  g_settings_set_boolean (self->settings,
                          PROMPT_SETTING_KEY_RESTORE_WINDOW_SIZE,
                          restore_window_size);
}

void
prompt_settings_get_window_size (PromptSettings *self,
                                 guint          *columns,
                                 guint          *rows)
{
  g_return_if_fail (PROMPT_IS_SETTINGS (self));
  g_return_if_fail (columns != NULL);
  g_return_if_fail (rows != NULL);

  g_settings_get (self->settings, "window-size", "(uu)", columns, rows);
}

void
prompt_settings_set_window_size (PromptSettings *self,
                                 guint           columns,
                                 guint           rows)
{
  g_return_if_fail (PROMPT_IS_SETTINGS (self));

  g_settings_set (self->settings, "window-size", "(uu)", columns, rows);
}

AdwColorScheme
prompt_settings_get_interface_style (PromptSettings *self)
{
  g_return_val_if_fail (PROMPT_IS_SETTINGS (self), 0);

  return g_settings_get_enum (self->settings, PROMPT_SETTING_KEY_INTERFACE_STYLE);
}

void
prompt_settings_set_interface_style (PromptSettings *self,
                                     AdwColorScheme  color_scheme)
{
  g_return_if_fail (PROMPT_IS_SETTINGS (self));
  g_return_if_fail (color_scheme == ADW_COLOR_SCHEME_DEFAULT ||
                    color_scheme == ADW_COLOR_SCHEME_FORCE_LIGHT ||
                    color_scheme == ADW_COLOR_SCHEME_FORCE_DARK);

  g_settings_set_enum (self->settings, PROMPT_SETTING_KEY_INTERFACE_STYLE, color_scheme);
}

static void
prompt_settings_add_proxy_environment (PromptSettings *self,
                                       const char     *protocol,
                                       const char     *envvar,
                                       GStrvBuilder   *builder)
{
  g_autofree char *schema_id = NULL;
  g_autofree char *authentication_user = NULL;
  g_autofree char *authentication_password = NULL;
  g_autofree char *host = NULL;
  g_autoptr(GSettings) settings = NULL;
  g_autoptr(GUri) uri = NULL;
  int port = 0;

  g_assert (PROMPT_IS_SETTINGS (self));
  g_assert (protocol != NULL);
  g_assert (envvar != NULL);
  g_assert (builder != NULL);

  schema_id = g_strdup_printf ("org.gnome.system.proxy.%s", protocol);
  settings = g_settings_new (schema_id);

  host = g_settings_get_string (settings, "host");
  port = g_settings_get_int (settings, "port");
  if (strempty (host) || port <= 0)
    return;

  if (g_str_equal (protocol, "http") &&
      g_settings_get_boolean (settings, "use-authentication"))
    {
      authentication_user = g_settings_get_string (settings, "authentication-user");
      authentication_password = g_settings_get_string (settings, "authentication-password");
    }

  uri = g_uri_build_with_user (G_URI_FLAGS_NONE,
                               protocol,
                               strempty (authentication_user) ? NULL : authentication_user,
                               strempty (authentication_password) ? NULL : authentication_password,
                               NULL,
                               host,
                               port,
                               "",
                               NULL,
                               NULL);

  if (uri != NULL)
    {
      g_autofree char *uristr = g_uri_to_string (uri);
      g_autofree char *lower = g_strdup_printf ("%s=%s", envvar, uristr);
      g_autofree char *upper_key = g_ascii_strup (envvar, -1);
      g_autofree char *upper = g_strdup_printf ("%s=%s", upper_key, uristr);

      g_strv_builder_add (builder, lower);
      g_strv_builder_add (builder, upper);
    }
}

char **
prompt_settings_get_proxy_environment (PromptSettings *self)
{
  g_autoptr(GStrvBuilder) builder = NULL;
  g_autoptr(GSettings) settings = NULL;
  g_auto(GStrv) ignore_hosts = NULL;

  g_return_val_if_fail (PROMPT_IS_SETTINGS (self), NULL);

  settings = g_settings_new ("org.gnome.system.proxy");

  if (!g_str_equal (g_settings_get_string (settings, "mode"), "manual"))
    return NULL;

  builder = g_strv_builder_new ();

  prompt_settings_add_proxy_environment (self, "http", "http_proxy", builder);
  prompt_settings_add_proxy_environment (self, "https", "https_proxy", builder);
  prompt_settings_add_proxy_environment (self, "ftp", "ftp_proxy", builder);
  prompt_settings_add_proxy_environment (self, "socks", "all_proxy", builder);

  if ((ignore_hosts = g_settings_get_strv (settings, "ignore-hosts")) && ignore_hosts[0])
    {
      g_autofree char *value = g_strjoinv (",", ignore_hosts);
      g_autofree char *lower = g_strdup_printf ("no_proxy=%s", value);
      g_autofree char *upper = g_strdup_printf ("no_proxy=%s", value);

      g_strv_builder_add (builder, lower);
      g_strv_builder_add (builder, upper);
    }

  return g_strv_builder_end (builder);
}

gboolean
prompt_settings_get_toast_on_copy_clipboard (PromptSettings *self)
{
  g_return_val_if_fail (PROMPT_IS_SETTINGS (self), FALSE);

  return g_settings_get_boolean (self->settings, PROMPT_SETTING_KEY_TOAST_ON_COPY_CLIPBOARD);
}

void
prompt_settings_set_toast_on_copy_clipboard (PromptSettings *self,
                                             gboolean        toast_on_copy_clipboard)
{
  g_return_if_fail (PROMPT_IS_SETTINGS (self));

  g_settings_set_boolean (self->settings,
                          PROMPT_SETTING_KEY_TOAST_ON_COPY_CLIPBOARD,
                          toast_on_copy_clipboard);
}
