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
#include "capsule-enums.h"
#include "capsule-profile.h"
#include "capsule-util.h"

#define CAPSULE_PROFILE_KEY_DEFAULT_CONTAINER   "default-container"
#define CAPSULE_PROFILE_KEY_EXIT_ACTION         "exit-action"
#define CAPSULE_PROFILE_KEY_LABEL               "label"
#define CAPSULE_PROFILE_KEY_OPACITY             "opacity"
#define CAPSULE_PROFILE_KEY_PALETTE             "palette"
#define CAPSULE_PROFILE_KEY_PRESERVE_DIRECTORY  "preserve-directory"
#define CAPSULE_PROFILE_KEY_SCROLL_ON_KEYSTROKE "scroll-on-keystroke"
#define CAPSULE_PROFILE_KEY_SCROLL_ON_OUTPUT    "scroll-on-output"

struct _CapsuleProfile
{
  GObject parent_instance;
  GSettings *settings;
  char *uuid;
};

enum {
  PROP_0,
  PROP_DEFAULT_CONTAINER,
  PROP_EXIT_ACTION,
  PROP_LABEL,
  PROP_OPACITY,
  PROP_PALETTE,
  PROP_PRESERVE_DIRECTORY,
  PROP_SCROLL_ON_KEYSTROKE,
  PROP_SCROLL_ON_OUTPUT,
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
  else if (g_str_equal (key, CAPSULE_PROFILE_KEY_LABEL))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LABEL]);
  else if (g_str_equal (key, CAPSULE_PROFILE_KEY_DEFAULT_CONTAINER))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DEFAULT_CONTAINER]);
  else if (g_str_equal (key, CAPSULE_PROFILE_KEY_EXIT_ACTION))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EXIT_ACTION]);
  else if (g_str_equal (key, CAPSULE_PROFILE_KEY_PALETTE))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PALETTE]);
  else if (g_str_equal (key, CAPSULE_PROFILE_KEY_OPACITY))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_OPACITY]);
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
    case PROP_DEFAULT_CONTAINER:
      g_value_take_string (value, capsule_profile_dup_default_container (self));
      break;

    case PROP_EXIT_ACTION:
      g_value_set_enum (value, capsule_profile_get_exit_action (self));
      break;

    case PROP_LABEL:
      g_value_take_string (value, capsule_profile_dup_label (self));
      break;

    case PROP_OPACITY:
      g_value_set_double (value, capsule_profile_get_opacity (self));
      break;

    case PROP_PALETTE:
      g_value_take_object (value, capsule_profile_dup_palette (self));
      break;

    case PROP_PRESERVE_DIRECTORY:
      g_value_set_enum (value, capsule_profile_get_preserve_directory (self));
      break;

    case PROP_SCROLL_ON_KEYSTROKE:
      g_value_set_boolean (value, capsule_profile_get_scroll_on_keystroke (self));
      break;

    case PROP_SCROLL_ON_OUTPUT:
      g_value_set_boolean (value, capsule_profile_get_scroll_on_output (self));
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
    case PROP_DEFAULT_CONTAINER:
      capsule_profile_set_default_container (self, g_value_get_string (value));
      break;

    case PROP_EXIT_ACTION:
      capsule_profile_set_exit_action (self, g_value_get_enum (value));
      break;

    case PROP_LABEL:
      capsule_profile_set_label (self, g_value_get_string (value));
      break;

    case PROP_OPACITY:
      capsule_profile_set_opacity (self, g_value_get_double (value));
      break;

    case PROP_PALETTE:
      capsule_profile_set_palette (self, g_value_get_object (value));
      break;

    case PROP_PRESERVE_DIRECTORY:
      capsule_profile_set_preserve_directory (self, g_value_get_enum (value));
      break;

    case PROP_SCROLL_ON_KEYSTROKE:
      capsule_profile_set_scroll_on_keystroke (self, g_value_get_boolean (value));
      break;

    case PROP_SCROLL_ON_OUTPUT:
      capsule_profile_set_scroll_on_output (self, g_value_get_boolean (value));
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

  properties[PROP_DEFAULT_CONTAINER] =
    g_param_spec_string ("default-container", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_EXIT_ACTION] =
    g_param_spec_enum ("exit-action", NULL, NULL,
                       CAPSULE_TYPE_EXIT_ACTION,
                       CAPSULE_EXIT_ACTION_CLOSE,
                       (G_PARAM_READWRITE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_LABEL] =
    g_param_spec_string ("label", NULL, NULL,
                         NULL,
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
                         CAPSULE_TYPE_PALETTE,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_PRESERVE_DIRECTORY] =
    g_param_spec_enum ("preserve-directory", NULL, NULL,
                       CAPSULE_TYPE_PRESERVE_DIRECTORY,
                       CAPSULE_PRESERVE_DIRECTORY_SAFE,
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

char *
capsule_profile_dup_default_container (CapsuleProfile *self)
{
  g_return_val_if_fail (CAPSULE_IS_PROFILE (self), NULL);

  return g_settings_get_string (self->settings, CAPSULE_PROFILE_KEY_DEFAULT_CONTAINER);
}

void
capsule_profile_set_default_container (CapsuleProfile *self,
                                       const char *default_container)
{
  g_return_if_fail (CAPSULE_IS_PROFILE (self));

  if (default_container == NULL)
    default_container = "";

  g_settings_set_string (self->settings,
                         CAPSULE_PROFILE_KEY_DEFAULT_CONTAINER,
                         default_container);
}

CapsuleExitAction
capsule_profile_get_exit_action (CapsuleProfile *self)
{
  g_return_val_if_fail (CAPSULE_IS_PROFILE (self), 0);

  return g_settings_get_enum (self->settings, CAPSULE_PROFILE_KEY_EXIT_ACTION);
}

void
capsule_profile_set_exit_action (CapsuleProfile    *self,
                                 CapsuleExitAction  exit_action)
{
  g_return_if_fail (CAPSULE_IS_PROFILE (self));
  g_return_if_fail (exit_action <= CAPSULE_EXIT_ACTION_CLOSE);

  g_settings_set_enum (self->settings,
                       CAPSULE_PROFILE_KEY_EXIT_ACTION,
                       exit_action);
}

CapsulePreserveDirectory
capsule_profile_get_preserve_directory (CapsuleProfile *self)
{
  g_return_val_if_fail (CAPSULE_IS_PROFILE (self), 0);

  return g_settings_get_enum (self->settings, CAPSULE_PROFILE_KEY_PRESERVE_DIRECTORY);
}

void
capsule_profile_set_preserve_directory (CapsuleProfile           *self,
                                        CapsulePreserveDirectory  preserve_directory)
{
  g_return_if_fail (CAPSULE_IS_PROFILE (self));
  g_return_if_fail (preserve_directory <= CAPSULE_PRESERVE_DIRECTORY_ALWAYS);

  g_settings_set_enum (self->settings,
                       CAPSULE_PROFILE_KEY_EXIT_ACTION,
                       preserve_directory);
}


void
capsule_profile_apply (CapsuleProfile    *self,
                       CapsuleRunContext *run_context,
                       VtePty            *pty,
                       const char        *current_directory_uri,
                       const char        *default_shell)
{
  CapsulePreserveDirectory preserve_directory;
  g_autoptr(GFile) last_directory = NULL;
  const char *cwd = NULL;
  const char *arg0 = NULL;

  g_assert (CAPSULE_IS_PROFILE (self));
  g_assert (CAPSULE_IS_RUN_CONTEXT (run_context));
  g_assert (VTE_IS_PTY (pty));

  capsule_run_context_set_pty (run_context, pty);
  capsule_run_context_setenv (run_context, "CAPSULE_PROFILE", self->uuid);
  capsule_run_context_setenv (run_context, "CAPSULE_VERSION", PACKAGE_VERSION);

  if (default_shell == NULL)
    default_shell = "/bin/sh";

  /* TODO: figure out shell/args */
  arg0 = default_shell;

  capsule_run_context_append_argv (run_context, arg0);

  if (current_directory_uri != NULL)
    last_directory = g_file_new_for_uri (current_directory_uri);

  preserve_directory = capsule_profile_get_preserve_directory (self);

  switch (preserve_directory)
    {
    case CAPSULE_PRESERVE_DIRECTORY_NEVER:
      break;

    case CAPSULE_PRESERVE_DIRECTORY_SAFE:
      /* TODO: We might want to check with the container that this
       * is a shell (as opposed to one available on the host).
       */
      if (!capsule_is_shell (arg0))
        break;
      G_GNUC_FALLTHROUGH;

    case CAPSULE_PRESERVE_DIRECTORY_ALWAYS:
      if (last_directory != NULL && g_file_is_native (last_directory))
        cwd = g_file_peek_path (last_directory);
      break;

    default:
      g_assert_not_reached ();
    }

  if (cwd != NULL)
    capsule_run_context_set_cwd (run_context, cwd);
}

CapsuleProfile *
capsule_profile_duplicate (CapsuleProfile *self)
{
  g_autoptr(CapsuleProfile) copy = NULL;
  g_autoptr(GSettingsSchema) schema = NULL;
  g_auto(GStrv) keys = NULL;

  g_return_val_if_fail (CAPSULE_IS_PROFILE (self), NULL);

  g_object_get (self->settings,
                "settings-schema", &schema,
                NULL);

  copy = capsule_profile_new (NULL);
  keys = g_settings_schema_list_keys (schema);

  for (guint i = 0; keys[i]; i++)
    {
      g_autoptr(GVariant) user_value = g_settings_get_user_value (self->settings, keys[i]);

      if (user_value != NULL)
        g_settings_set_value (copy->settings, keys[i], user_value);
    }

  capsule_application_add_profile (CAPSULE_APPLICATION_DEFAULT, copy);

  return g_steal_pointer (&copy);
}

CapsulePalette *
capsule_profile_dup_palette (CapsuleProfile *self)
{
  g_autofree char *name = NULL;

  g_return_val_if_fail (CAPSULE_IS_PROFILE (self), NULL);

  name = g_settings_get_string (self->settings, CAPSULE_PROFILE_KEY_PALETTE);

  return capsule_palette_new_from_name (name);
}

void
capsule_profile_set_palette (CapsuleProfile *self,
                             CapsulePalette *palette)
{
  const char *id = "gnome";

  g_return_if_fail (CAPSULE_IS_PROFILE (self));
  g_return_if_fail (!palette || CAPSULE_IS_PALETTE (palette));

  if (palette != NULL)
    id = capsule_palette_get_id (palette);

  g_settings_set_string (self->settings, CAPSULE_PROFILE_KEY_PALETTE, id);
}

double
capsule_profile_get_opacity (CapsuleProfile *self)
{
  g_return_val_if_fail (CAPSULE_IS_PROFILE (self), 1.);

  return g_settings_get_double (self->settings, CAPSULE_PROFILE_KEY_OPACITY);
}

void
capsule_profile_set_opacity (CapsuleProfile *self,
                             double          opacity)
{
  g_return_if_fail (CAPSULE_IS_PROFILE (self));

  opacity = CLAMP (opacity, 0, 1);

  g_settings_set_double (self->settings,
                         CAPSULE_PROFILE_KEY_OPACITY,
                         opacity);
}
