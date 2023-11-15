/*
 * prompt-profile.c
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

#include "prompt-application.h"
#include "prompt-enums.h"
#include "prompt-profile.h"
#include "prompt-util.h"

struct _PromptProfile
{
  GObject parent_instance;
  GSettings *settings;
  char *uuid;
};

enum {
  PROP_0,
  PROP_BACKSPACE_BINDING,
  PROP_BOLD_IS_BRIGHT,
  PROP_CJK_AMBIGUOUS_WIDTH,
  PROP_CUSTOM_COMMAND,
  PROP_DEFAULT_CONTAINER,
  PROP_DELETE_BINDING,
  PROP_EXIT_ACTION,
  PROP_LABEL,
  PROP_LIMIT_SCROLLBACK,
  PROP_LOGIN_SHELL,
  PROP_OPACITY,
  PROP_PALETTE,
  PROP_PALETTE_ID,
  PROP_PRESERVE_DIRECTORY,
  PROP_SCROLL_ON_KEYSTROKE,
  PROP_SCROLL_ON_OUTPUT,
  PROP_SCROLLBACK_LINES,
  PROP_USE_CUSTOM_COMMAND,
  PROP_UUID,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PromptProfile, prompt_profile, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
prompt_profile_changed_cb (PromptProfile *self,
                           const char    *key,
                           GSettings     *settings)
{
  g_assert (PROMPT_IS_PROFILE (self));
  g_assert (key != NULL);
  g_assert (G_IS_SETTINGS (settings));

  if (FALSE) {}
  else if (g_str_equal (key, PROMPT_PROFILE_KEY_LABEL))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LABEL]);
  else if (g_str_equal (key, PROMPT_PROFILE_KEY_DEFAULT_CONTAINER))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DEFAULT_CONTAINER]);
  else if (g_str_equal (key, PROMPT_PROFILE_KEY_EXIT_ACTION))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EXIT_ACTION]);
  else if (g_str_equal (key, PROMPT_PROFILE_KEY_PALETTE))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PALETTE]);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PALETTE_ID]);
    }
  else if (g_str_equal (key, PROMPT_PROFILE_KEY_OPACITY))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_OPACITY]);
  else if (g_str_equal (key, PROMPT_PROFILE_KEY_LIMIT_SCROLLBACK))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LIMIT_SCROLLBACK]);
  else if (g_str_equal (key, PROMPT_PROFILE_KEY_SCROLLBACK_LINES))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SCROLLBACK_LINES]);
  else if (g_str_equal (key, PROMPT_PROFILE_KEY_BACKSPACE_BINDING))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BACKSPACE_BINDING]);
  else if (g_str_equal (key, PROMPT_PROFILE_KEY_DELETE_BINDING))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DELETE_BINDING]);
  else if (g_str_equal (key, PROMPT_PROFILE_KEY_CJK_AMBIGUOUS_WIDTH))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CJK_AMBIGUOUS_WIDTH]);
  else if (g_str_equal (key, PROMPT_PROFILE_KEY_BOLD_IS_BRIGHT))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BOLD_IS_BRIGHT]);
  else if (g_str_equal (key, PROMPT_PROFILE_KEY_LOGIN_SHELL))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOGIN_SHELL]);
  else if (g_str_equal (key, PROMPT_PROFILE_KEY_CUSTOM_COMMAND))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CUSTOM_COMMAND]);
  else if (g_str_equal (key, PROMPT_PROFILE_KEY_USE_CUSTOM_COMMAND))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_USE_CUSTOM_COMMAND]);
}

static void
prompt_profile_constructed (GObject *object)
{
  PromptProfile *self = (PromptProfile *)object;
  g_autofree char *path = NULL;

  G_OBJECT_CLASS (prompt_profile_parent_class)->constructed (object);

  if (self->uuid == NULL)
    self->uuid = g_dbus_generate_guid ();

  path = g_strdup_printf (APP_SCHEMA_PATH"Profiles/%s/", self->uuid);
  self->settings = g_settings_new_with_path (APP_SCHEMA_PROFILE_ID, path);

  g_signal_connect_object (self->settings,
                           "changed",
                           G_CALLBACK (prompt_profile_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
prompt_profile_dispose (GObject *object)
{
  PromptProfile *self = (PromptProfile *)object;

  g_clear_object (&self->settings);
  g_clear_pointer (&self->uuid, g_free);

  G_OBJECT_CLASS (prompt_profile_parent_class)->dispose (object);
}

static void
prompt_profile_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  PromptProfile *self = PROMPT_PROFILE (object);

  switch (prop_id)
    {
    case PROP_CJK_AMBIGUOUS_WIDTH:
      g_value_set_enum (value, prompt_profile_get_cjk_ambiguous_width (self));
      break;

    case PROP_BACKSPACE_BINDING:
      g_value_set_enum (value, prompt_profile_get_backspace_binding (self));
      break;

    case PROP_BOLD_IS_BRIGHT:
      g_value_set_boolean (value, prompt_profile_get_bold_is_bright (self));
      break;

    case PROP_CUSTOM_COMMAND:
      g_value_take_string (value, prompt_profile_dup_custom_command (self));
      break;

    case PROP_DEFAULT_CONTAINER:
      g_value_take_string (value, prompt_profile_dup_default_container (self));
      break;

    case PROP_DELETE_BINDING:
      g_value_set_enum (value, prompt_profile_get_delete_binding (self));
      break;

    case PROP_EXIT_ACTION:
      g_value_set_enum (value, prompt_profile_get_exit_action (self));
      break;

    case PROP_LABEL:
      g_value_take_string (value, prompt_profile_dup_label (self));
      break;

    case PROP_LIMIT_SCROLLBACK:
      g_value_set_boolean (value, prompt_profile_get_limit_scrollback (self));
      break;

    case PROP_LOGIN_SHELL:
      g_value_set_boolean (value, prompt_profile_get_login_shell (self));
      break;

    case PROP_OPACITY:
      g_value_set_double (value, prompt_profile_get_opacity (self));
      break;

    case PROP_PALETTE:
      g_value_take_object (value, prompt_profile_dup_palette (self));
      break;

    case PROP_PALETTE_ID:
      {
        g_autoptr(PromptPalette) palette = prompt_profile_dup_palette (self);
        if (palette != NULL)
          g_value_set_string (value, prompt_palette_get_id (palette));
      }
      break;

    case PROP_PRESERVE_DIRECTORY:
      g_value_set_enum (value, prompt_profile_get_preserve_directory (self));
      break;

    case PROP_SCROLL_ON_KEYSTROKE:
      g_value_set_boolean (value, prompt_profile_get_scroll_on_keystroke (self));
      break;

    case PROP_SCROLL_ON_OUTPUT:
      g_value_set_boolean (value, prompt_profile_get_scroll_on_output (self));
      break;

    case PROP_SCROLLBACK_LINES:
      g_value_set_int (value, prompt_profile_get_scrollback_lines (self));
      break;

    case PROP_USE_CUSTOM_COMMAND:
      g_value_set_boolean (value, prompt_profile_get_use_custom_command (self));
      break;

    case PROP_UUID:
      g_value_set_string (value, prompt_profile_get_uuid (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_profile_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  PromptProfile *self = PROMPT_PROFILE (object);

  switch (prop_id)
    {
    case PROP_CJK_AMBIGUOUS_WIDTH:
      prompt_profile_set_cjk_ambiguous_width (self, g_value_get_enum (value));
      break;

    case PROP_BACKSPACE_BINDING:
      prompt_profile_set_backspace_binding (self, g_value_get_enum (value));
      break;

    case PROP_BOLD_IS_BRIGHT:
      prompt_profile_set_bold_is_bright (self, g_value_get_boolean (value));
      break;

    case PROP_CUSTOM_COMMAND:
      prompt_profile_set_custom_command (self, g_value_get_string (value));
      break;

    case PROP_DEFAULT_CONTAINER:
      prompt_profile_set_default_container (self, g_value_get_string (value));
      break;

    case PROP_DELETE_BINDING:
      prompt_profile_set_delete_binding (self, g_value_get_enum (value));
      break;

    case PROP_EXIT_ACTION:
      prompt_profile_set_exit_action (self, g_value_get_enum (value));
      break;

    case PROP_LABEL:
      prompt_profile_set_label (self, g_value_get_string (value));
      break;

    case PROP_LIMIT_SCROLLBACK:
      prompt_profile_set_limit_scrollback (self, g_value_get_boolean (value));
      break;

    case PROP_LOGIN_SHELL:
      prompt_profile_set_login_shell (self, g_value_get_boolean (value));
      break;

    case PROP_OPACITY:
      prompt_profile_set_opacity (self, g_value_get_double (value));
      break;

    case PROP_PALETTE:
      prompt_profile_set_palette (self, g_value_get_object (value));
      break;

    case PROP_PALETTE_ID:
      {
        const char *id = g_value_get_string (value);
        g_autoptr(PromptPalette) palette = prompt_palette_new_from_name (id);
        prompt_profile_set_palette (self, palette);
      }
      break;

    case PROP_PRESERVE_DIRECTORY:
      prompt_profile_set_preserve_directory (self, g_value_get_enum (value));
      break;

    case PROP_SCROLL_ON_KEYSTROKE:
      prompt_profile_set_scroll_on_keystroke (self, g_value_get_boolean (value));
      break;

    case PROP_SCROLL_ON_OUTPUT:
      prompt_profile_set_scroll_on_output (self, g_value_get_boolean (value));
      break;

    case PROP_SCROLLBACK_LINES:
      prompt_profile_set_scrollback_lines (self, g_value_get_int (value));
      break;

    case PROP_USE_CUSTOM_COMMAND:
      prompt_profile_set_use_custom_command (self, g_value_get_boolean (value));
      break;

    case PROP_UUID:
      self->uuid = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_profile_class_init (PromptProfileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = prompt_profile_constructed;
  object_class->dispose = prompt_profile_dispose;
  object_class->get_property = prompt_profile_get_property;
  object_class->set_property = prompt_profile_set_property;

  properties[PROP_CJK_AMBIGUOUS_WIDTH] =
    g_param_spec_enum ("cjk-ambiguous-width", NULL, NULL,
                       PROMPT_TYPE_CJK_AMBIGUOUS_WIDTH,
                       PROMPT_CJK_AMBIGUOUS_WIDTH_NARROW,
                       (G_PARAM_READWRITE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_CUSTOM_COMMAND] =
    g_param_spec_string ("custom-command", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_BACKSPACE_BINDING] =
    g_param_spec_enum ("backspace-binding", NULL, NULL,
                       VTE_TYPE_ERASE_BINDING,
                       VTE_ERASE_AUTO,
                       (G_PARAM_READWRITE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_BOLD_IS_BRIGHT] =
    g_param_spec_boolean ("bold-is-bright", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_DEFAULT_CONTAINER] =
    g_param_spec_string ("default-container", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_DELETE_BINDING] =
    g_param_spec_enum ("delete-binding", NULL, NULL,
                       VTE_TYPE_ERASE_BINDING,
                       VTE_ERASE_AUTO,
                       (G_PARAM_READWRITE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_EXIT_ACTION] =
    g_param_spec_enum ("exit-action", NULL, NULL,
                       PROMPT_TYPE_EXIT_ACTION,
                       PROMPT_EXIT_ACTION_CLOSE,
                       (G_PARAM_READWRITE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_LABEL] =
    g_param_spec_string ("label", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_LIMIT_SCROLLBACK] =
    g_param_spec_boolean ("limit-scrollback", NULL, NULL,
                         FALSE,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_LOGIN_SHELL] =
    g_param_spec_boolean ("login-shell", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_OPACITY] =
    g_param_spec_double ("opacity", NULL, NULL,
                         0, 1, 1,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_PALETTE] =
    g_param_spec_object ("palette", NULL, NULL,
                         PROMPT_TYPE_PALETTE,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_PALETTE_ID] =
    g_param_spec_string ("palette-id", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_PRESERVE_DIRECTORY] =
    g_param_spec_enum ("preserve-directory", NULL, NULL,
                       PROMPT_TYPE_PRESERVE_DIRECTORY,
                       PROMPT_PRESERVE_DIRECTORY_SAFE,
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

  properties[PROP_SCROLLBACK_LINES] =
    g_param_spec_int ("scrollback-lines", NULL, NULL,
                      0, G_MAXINT, 10000,
                      (G_PARAM_READWRITE |
                       G_PARAM_EXPLICIT_NOTIFY |
                       G_PARAM_STATIC_STRINGS));

  properties[PROP_USE_CUSTOM_COMMAND] =
    g_param_spec_boolean ("use-custom-command", NULL, NULL,
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
prompt_profile_init (PromptProfile *self)
{
}

PromptProfile *
prompt_profile_new (const char *uuid)
{
  return g_object_new (PROMPT_TYPE_PROFILE,
                       "uuid", uuid,
                       NULL);
}

/**
 * prompt_profile_get_uuid:
 * @self: a #PromptProfile
 *
 * Gets the UUID for the profile.
 *
 * Returns: (not nullable): the profile UUID
 */
const char *
prompt_profile_get_uuid (PromptProfile *self)
{
  g_return_val_if_fail (PROMPT_IS_PROFILE (self), NULL);

  return self->uuid;
}

char *
prompt_profile_dup_label (PromptProfile *self)
{
  g_autofree char *label = NULL;

  g_return_val_if_fail (PROMPT_IS_PROFILE (self), NULL);

  label = g_settings_get_string (self->settings, PROMPT_PROFILE_KEY_LABEL);

  if (label == NULL || label[0] == 0)
    g_set_str (&label, _("Untitled Profile"));

  return g_steal_pointer (&label);
}

void
prompt_profile_set_label (PromptProfile *self,
                          const char    *label)
{
  g_return_if_fail (PROMPT_IS_PROFILE (self));

  if (label == NULL)
    label = "";

  g_settings_set_string (self->settings, PROMPT_PROFILE_KEY_LABEL, label);
}

gboolean
prompt_profile_get_scroll_on_keystroke (PromptProfile *self)
{
  g_return_val_if_fail (PROMPT_IS_PROFILE (self), FALSE);

  return g_settings_get_boolean (self->settings, PROMPT_PROFILE_KEY_SCROLL_ON_KEYSTROKE);
}

void
prompt_profile_set_scroll_on_keystroke (PromptProfile *self,
                                        gboolean       scroll_on_keystroke)
{
  g_return_if_fail (PROMPT_IS_PROFILE (self));

  g_settings_set_boolean (self->settings,
                          PROMPT_PROFILE_KEY_SCROLL_ON_KEYSTROKE,
                          scroll_on_keystroke);
}

gboolean
prompt_profile_get_scroll_on_output (PromptProfile *self)
{
  g_return_val_if_fail (PROMPT_IS_PROFILE (self), FALSE);

  return g_settings_get_boolean (self->settings, PROMPT_PROFILE_KEY_SCROLL_ON_OUTPUT);
}

void
prompt_profile_set_scroll_on_output (PromptProfile *self,
                                     gboolean       scroll_on_output)
{
  g_return_if_fail (PROMPT_IS_PROFILE (self));

  g_settings_set_boolean (self->settings,
                          PROMPT_PROFILE_KEY_SCROLL_ON_OUTPUT,
                          scroll_on_output);
}

char *
prompt_profile_dup_default_container (PromptProfile *self)
{
  g_return_val_if_fail (PROMPT_IS_PROFILE (self), NULL);

  return g_settings_get_string (self->settings, PROMPT_PROFILE_KEY_DEFAULT_CONTAINER);
}

void
prompt_profile_set_default_container (PromptProfile *self,
                                      const char    *default_container)
{
  g_return_if_fail (PROMPT_IS_PROFILE (self));

  if (default_container == NULL)
    default_container = "";

  g_settings_set_string (self->settings,
                         PROMPT_PROFILE_KEY_DEFAULT_CONTAINER,
                         default_container);
}

PromptExitAction
prompt_profile_get_exit_action (PromptProfile *self)
{
  g_return_val_if_fail (PROMPT_IS_PROFILE (self), 0);

  return g_settings_get_enum (self->settings, PROMPT_PROFILE_KEY_EXIT_ACTION);
}

void
prompt_profile_set_exit_action (PromptProfile    *self,
                                PromptExitAction  exit_action)
{
  g_return_if_fail (PROMPT_IS_PROFILE (self));
  g_return_if_fail (exit_action <= PROMPT_EXIT_ACTION_CLOSE);

  g_settings_set_enum (self->settings,
                       PROMPT_PROFILE_KEY_EXIT_ACTION,
                       exit_action);
}

PromptPreserveDirectory
prompt_profile_get_preserve_directory (PromptProfile *self)
{
  g_return_val_if_fail (PROMPT_IS_PROFILE (self), 0);

  return g_settings_get_enum (self->settings, PROMPT_PROFILE_KEY_PRESERVE_DIRECTORY);
}

void
prompt_profile_set_preserve_directory (PromptProfile           *self,
                                       PromptPreserveDirectory  preserve_directory)
{
  g_return_if_fail (PROMPT_IS_PROFILE (self));
  g_return_if_fail (preserve_directory <= PROMPT_PRESERVE_DIRECTORY_ALWAYS);

  g_settings_set_enum (self->settings,
                       PROMPT_PROFILE_KEY_EXIT_ACTION,
                       preserve_directory);
}

PromptProfile *
prompt_profile_duplicate (PromptProfile *self)
{
  g_autoptr(PromptProfile) copy = NULL;
  g_autoptr(GSettingsSchema) schema = NULL;
  g_auto(GStrv) keys = NULL;

  g_return_val_if_fail (PROMPT_IS_PROFILE (self), NULL);

  g_object_get (self->settings,
                "settings-schema", &schema,
                NULL);

  copy = prompt_profile_new (NULL);
  keys = g_settings_schema_list_keys (schema);

  for (guint i = 0; keys[i]; i++)
    {
      g_autoptr(GVariant) user_value = g_settings_get_user_value (self->settings, keys[i]);

      if (user_value != NULL)
        g_settings_set_value (copy->settings, keys[i], user_value);
    }

  prompt_application_add_profile (PROMPT_APPLICATION_DEFAULT, copy);

  return g_steal_pointer (&copy);
}

PromptPalette *
prompt_profile_dup_palette (PromptProfile *self)
{
  g_autofree char *name = NULL;

  g_return_val_if_fail (PROMPT_IS_PROFILE (self), NULL);

  name = g_settings_get_string (self->settings, PROMPT_PROFILE_KEY_PALETTE);

  return prompt_palette_new_from_name (name);
}

void
prompt_profile_set_palette (PromptProfile *self,
                            PromptPalette *palette)
{
  const char *id = "gnome";

  g_return_if_fail (PROMPT_IS_PROFILE (self));
  g_return_if_fail (!palette || PROMPT_IS_PALETTE (palette));

  if (palette != NULL)
    id = prompt_palette_get_id (palette);

  g_settings_set_string (self->settings, PROMPT_PROFILE_KEY_PALETTE, id);
}

double
prompt_profile_get_opacity (PromptProfile *self)
{
  g_return_val_if_fail (PROMPT_IS_PROFILE (self), 1.);

  return g_settings_get_double (self->settings, PROMPT_PROFILE_KEY_OPACITY);
}

void
prompt_profile_set_opacity (PromptProfile *self,
                            double         opacity)
{
  g_return_if_fail (PROMPT_IS_PROFILE (self));

  opacity = CLAMP (opacity, 0, 1);

  g_settings_set_double (self->settings,
                         PROMPT_PROFILE_KEY_OPACITY,
                         opacity);
}

gboolean
prompt_profile_get_limit_scrollback (PromptProfile *self)
{
  g_return_val_if_fail (PROMPT_IS_PROFILE (self), FALSE);

  return g_settings_get_boolean (self->settings, PROMPT_PROFILE_KEY_LIMIT_SCROLLBACK);
}

void
prompt_profile_set_limit_scrollback (PromptProfile *self,
                                     gboolean       limit_scrollback)
{
  g_return_if_fail (PROMPT_IS_PROFILE (self));

  g_settings_set_boolean (self->settings,
                          PROMPT_PROFILE_KEY_LIMIT_SCROLLBACK,
                          limit_scrollback);
}

int
prompt_profile_get_scrollback_lines (PromptProfile *self)
{
  g_return_val_if_fail (PROMPT_IS_PROFILE (self), 0);

  return g_settings_get_int (self->settings, PROMPT_PROFILE_KEY_SCROLLBACK_LINES);
}

void
prompt_profile_set_scrollback_lines (PromptProfile *self,
                                     int            scrollback_lines)
{
  g_return_if_fail (PROMPT_IS_PROFILE (self));

  g_settings_set_int (self->settings,
                      PROMPT_PROFILE_KEY_SCROLLBACK_LINES,
                      scrollback_lines);
}

GSettings *
prompt_profile_dup_settings (PromptProfile *self)
{
  g_return_val_if_fail (PROMPT_IS_PROFILE (self), NULL);

  return g_object_ref (self->settings);
}

VteEraseBinding
prompt_profile_get_backspace_binding (PromptProfile *self)
{
  g_return_val_if_fail (PROMPT_IS_PROFILE (self), 0);

  return g_settings_get_enum (self->settings, PROMPT_PROFILE_KEY_BACKSPACE_BINDING);
}

void
prompt_profile_set_backspace_binding (PromptProfile   *self,
                                      VteEraseBinding  backspace_binding)
{
  g_return_if_fail (PROMPT_IS_PROFILE (self));

  g_settings_set_enum (self->settings,
                       PROMPT_PROFILE_KEY_BACKSPACE_BINDING,
                       backspace_binding);
}

VteEraseBinding
prompt_profile_get_delete_binding (PromptProfile *self)
{
  g_return_val_if_fail (PROMPT_IS_PROFILE (self), 0);

  return g_settings_get_enum (self->settings, PROMPT_PROFILE_KEY_DELETE_BINDING);
}

void
prompt_profile_set_delete_binding (PromptProfile   *self,
                                   VteEraseBinding  delete_binding)
{
  g_return_if_fail (PROMPT_IS_PROFILE (self));

  g_settings_set_enum (self->settings,
                       PROMPT_PROFILE_KEY_DELETE_BINDING,
                       delete_binding);
}

PromptCjkAmbiguousWidth
prompt_profile_get_cjk_ambiguous_width (PromptProfile *self)
{
  g_return_val_if_fail (PROMPT_IS_PROFILE (self), 1);

  return g_settings_get_enum (self->settings, PROMPT_PROFILE_KEY_CJK_AMBIGUOUS_WIDTH);
}

void
prompt_profile_set_cjk_ambiguous_width (PromptProfile           *self,
                                        PromptCjkAmbiguousWidth  cjk_ambiguous_width)
{
  g_return_if_fail (PROMPT_IS_PROFILE (self));

  g_settings_set_enum (self->settings,
                       PROMPT_PROFILE_KEY_CJK_AMBIGUOUS_WIDTH,
                       cjk_ambiguous_width);
}

gboolean
prompt_profile_get_bold_is_bright (PromptProfile *self)
{
  g_return_val_if_fail (PROMPT_IS_PROFILE (self), FALSE);

  return g_settings_get_boolean (self->settings, PROMPT_PROFILE_KEY_BOLD_IS_BRIGHT);
}

void
prompt_profile_set_bold_is_bright (PromptProfile *self,
                                   gboolean       bold_is_bright)
{
  g_return_if_fail (PROMPT_IS_PROFILE (self));

  g_settings_set_boolean (self->settings,
                          PROMPT_PROFILE_KEY_BOLD_IS_BRIGHT,
                          bold_is_bright);
}

gboolean
prompt_profile_get_login_shell (PromptProfile *self)
{
  g_return_val_if_fail (PROMPT_IS_PROFILE (self), FALSE);

  return g_settings_get_boolean (self->settings, PROMPT_PROFILE_KEY_LOGIN_SHELL);
}

void
prompt_profile_set_login_shell (PromptProfile *self,
                                gboolean       login_shell)
{
  g_return_if_fail (PROMPT_IS_PROFILE (self));

  g_settings_set_boolean (self->settings,
                          PROMPT_PROFILE_KEY_LOGIN_SHELL,
                          login_shell);
}

gboolean
prompt_profile_get_use_custom_command (PromptProfile *self)
{
  g_return_val_if_fail (PROMPT_IS_PROFILE (self), FALSE);

  return g_settings_get_boolean (self->settings, PROMPT_PROFILE_KEY_USE_CUSTOM_COMMAND);
}

void
prompt_profile_set_use_custom_command (PromptProfile *self,
                                       gboolean       use_custom_command)
{
  g_return_if_fail (PROMPT_IS_PROFILE (self));

  g_settings_set_boolean (self->settings,
                          PROMPT_PROFILE_KEY_USE_CUSTOM_COMMAND,
                          use_custom_command);
}

char *
prompt_profile_dup_custom_command (PromptProfile *self)
{
  g_return_val_if_fail (PROMPT_IS_PROFILE (self), NULL);

  return g_settings_get_string (self->settings, PROMPT_PROFILE_KEY_CUSTOM_COMMAND);
}

void
prompt_profile_set_custom_command (PromptProfile *self,
                                   const char    *custom_command)
{
  g_return_if_fail (PROMPT_IS_PROFILE (self));

  if (custom_command == NULL)
    custom_command = "";

  g_settings_set_string (self->settings, PROMPT_PROFILE_KEY_CUSTOM_COMMAND, custom_command);
}
