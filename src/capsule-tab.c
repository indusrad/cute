/*
 * capsule-tab.c
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

#include "capsule-application.h"
#include "capsule-container.h"
#include "capsule-tab.h"
#include "capsule-terminal.h"

typedef enum _CapsuleTabState
{
  CAPSULE_TAB_STATE_INITIAL,
  CAPSULE_TAB_STATE_SPAWNING,
  CAPSULE_TAB_STATE_RUNNING,
  CAPSULE_TAB_STATE_EXITED,
  CAPSULE_TAB_STATE_FAILED,
} CapsuleTabState;

struct _CapsuleTab
{
  GtkWidget          parent_instance;

  CapsuleProfile    *profile;
  GSubprocess       *subprocess;
  char              *title_prefix;

  AdwBanner         *banner;
  GtkScrolledWindow *scrolled_window;
  CapsuleTerminal   *terminal;

  CapsuleTabState    state;
};

enum {
  PROP_0,
  PROP_PROFILE,
  PROP_TITLE,
  PROP_TITLE_PREFIX,
  PROP_SUBTITLE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (CapsuleTab, capsule_tab, GTK_TYPE_WIDGET)

static GParamSpec *properties[N_PROPS];

static void
capsule_tab_wait_check_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  GSubprocess *subprocess = (GSubprocess *)object;
  g_autoptr(CapsuleTab) self = user_data;
  g_autoptr(GError) error = NULL;
  gboolean success;

  g_assert (G_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (CAPSULE_IS_TAB (self));
  g_assert (self->state == CAPSULE_TAB_STATE_RUNNING);

  success = g_subprocess_wait_check_finish (subprocess, result, &error);

  if (success)
    self->state = CAPSULE_TAB_STATE_EXITED;
  else
    self->state = CAPSULE_TAB_STATE_FAILED;

  /* TODO: respawn state */
}

static void
capsule_tab_spawn_cb (GObject      *object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  CapsuleContainer *container = (CapsuleContainer *)object;
  g_autoptr(CapsuleTab) self = user_data;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (CAPSULE_IS_CONTAINER (container));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (CAPSULE_IS_TAB (self));
  g_assert (self->state == CAPSULE_TAB_STATE_SPAWNING);

  if (!(subprocess = capsule_container_spawn_finish (container, result, &error)))
    {
      const char *profile_uuid = capsule_profile_get_uuid (self->profile);

      self->state = CAPSULE_TAB_STATE_FAILED;

      vte_terminal_feed (VTE_TERMINAL (self->terminal), error->message, -1);
      vte_terminal_feed (VTE_TERMINAL (self->terminal), "\r\n", -1);

      adw_banner_set_title (self->banner, _("Failed to launch terminal"));
      adw_banner_set_revealed (self->banner, TRUE);
      adw_banner_set_button_label (self->banner, _("Edit Profile"));
      gtk_actionable_set_action_target (GTK_ACTIONABLE (self->banner), "s", profile_uuid);
      gtk_actionable_set_action_name (GTK_ACTIONABLE (self->banner), "app.edit-profile");

      return;
    }

  self->state = CAPSULE_TAB_STATE_RUNNING;

  g_subprocess_wait_check_async (subprocess,
                                 NULL,
                                 capsule_tab_wait_check_cb,
                                 g_object_ref (self));
}

static void
capsule_tab_respawn (CapsuleTab *self)
{
  g_autofree char *default_container = NULL;
  g_autoptr(CapsuleContainer) container = NULL;
  g_autoptr(VtePty) new_pty = NULL;
  CapsuleApplication *app;
  const char *profile_uuid;
  VtePty *pty;

  g_assert (CAPSULE_IS_TAB (self));
  g_assert (self->state == CAPSULE_TAB_STATE_INITIAL ||
            self->state == CAPSULE_TAB_STATE_EXITED ||
            self->state == CAPSULE_TAB_STATE_FAILED);

  adw_banner_set_revealed (self->banner, FALSE);

  app = CAPSULE_APPLICATION_DEFAULT;
  default_container = capsule_profile_dup_default_container (self->profile);
  container = capsule_application_lookup_container (app, default_container);
  profile_uuid = capsule_profile_get_uuid (self->profile);

  if (container == NULL)
    {
      g_autofree char *title = NULL;

      self->state = CAPSULE_TAB_STATE_FAILED;

      title = g_strdup_printf (_("Cannot locate container “%s”"), default_container);
      adw_banner_set_title (self->banner, title);
      adw_banner_set_revealed (self->banner, TRUE);
      adw_banner_set_button_label (self->banner, _("Edit Profile"));
      gtk_actionable_set_action_target (GTK_ACTIONABLE (self->banner), "s", profile_uuid);
      gtk_actionable_set_action_name (GTK_ACTIONABLE (self->banner), "app.edit-profile");

      return;
    }

  self->state = CAPSULE_TAB_STATE_SPAWNING;

  pty = vte_terminal_get_pty (VTE_TERMINAL (self->terminal));

  if (pty == NULL)
    {
      g_autoptr(GError) error = NULL;

      new_pty = vte_pty_new_sync (0, NULL, &error);

      if (new_pty == NULL)
        {
          self->state = CAPSULE_TAB_STATE_FAILED;

          adw_banner_set_title (self->banner, _("Failed to create pseudo terminal device"));
          adw_banner_set_revealed (self->banner, TRUE);
          adw_banner_set_button_label (self->banner, NULL);
          gtk_actionable_set_action_name (GTK_ACTIONABLE (self->banner), NULL);

          return;
        }

      vte_terminal_set_pty (VTE_TERMINAL (self->terminal), new_pty);

      pty = new_pty;
    }

  capsule_container_spawn_async (container,
                                 pty,
                                 self->profile,
                                 NULL,
                                 capsule_tab_spawn_cb,
                                 g_object_ref (self));
}

static void
capsule_tab_map (GtkWidget *widget)
{
  CapsuleTab *self = (CapsuleTab *)widget;

  g_assert (CAPSULE_IS_TAB (widget));

  GTK_WIDGET_CLASS (capsule_tab_parent_class)->map (widget);

  if (self->state == CAPSULE_TAB_STATE_INITIAL)
    capsule_tab_respawn (self);
}

static void
capsule_tab_notify_window_title_cb (CapsuleTab      *self,
                                    GParamSpec      *pspec,
                                    CapsuleTerminal *terminal)
{
  g_assert (CAPSULE_IS_TAB (self));
  g_assert (CAPSULE_IS_TERMINAL (terminal));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);
}

static void
capsule_tab_constructed (GObject *object)
{
  /* TODO: Setup terminal */

  G_OBJECT_CLASS (capsule_tab_parent_class)->constructed (object);
}

static void
capsule_tab_dispose (GObject *object)
{
  CapsuleTab *self = (CapsuleTab *)object;
  GtkWidget *child;

  gtk_widget_dispose_template (GTK_WIDGET (self), CAPSULE_TYPE_TAB);

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self))))
    gtk_widget_unparent (child);

  g_clear_object (&self->profile);

  G_OBJECT_CLASS (capsule_tab_parent_class)->dispose (object);
}

static void
capsule_tab_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  CapsuleTab *self = CAPSULE_TAB (object);

  switch (prop_id)
    {
    case PROP_PROFILE:
      g_value_set_object (value, capsule_tab_get_profile (self));
      break;

    case PROP_SUBTITLE:
      g_value_take_string (value, capsule_tab_dup_subtitle (self));
      break;

    case PROP_TITLE:
      g_value_take_string (value, capsule_tab_dup_title (self));
      break;

    case PROP_TITLE_PREFIX:
      g_value_set_string (value, capsule_tab_get_title_prefix (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
capsule_tab_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  CapsuleTab *self = CAPSULE_TAB (object);

  switch (prop_id)
    {
    case PROP_PROFILE:
      self->profile = g_value_dup_object (value);
      break;

    case PROP_TITLE_PREFIX:
      capsule_tab_set_title_prefix (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
capsule_tab_class_init (CapsuleTabClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = capsule_tab_constructed;
  object_class->dispose = capsule_tab_dispose;
  object_class->get_property = capsule_tab_get_property;
  object_class->set_property = capsule_tab_set_property;

  widget_class->map = capsule_tab_map;

  properties[PROP_PROFILE] =
    g_param_spec_object ("profile", NULL, NULL,
                         CAPSULE_TYPE_PROFILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_SUBTITLE] =
    g_param_spec_string ("subtitle", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_TITLE_PREFIX] =
    g_param_spec_string ("title-prefix", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Capsule/capsule-tab.ui");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);

  gtk_widget_class_bind_template_child (widget_class, CapsuleTab, banner);
  gtk_widget_class_bind_template_child (widget_class, CapsuleTab, terminal);
  gtk_widget_class_bind_template_child (widget_class, CapsuleTab, scrolled_window);

  gtk_widget_class_bind_template_callback (widget_class, capsule_tab_notify_window_title_cb);

  g_type_ensure (CAPSULE_TYPE_TERMINAL);
}

static void
capsule_tab_init (CapsuleTab *self)
{
  self->state = CAPSULE_TAB_STATE_INITIAL;

  gtk_widget_init_template (GTK_WIDGET (self));
}

CapsuleTab *
capsule_tab_new (CapsuleProfile *profile)
{
  g_return_val_if_fail (CAPSULE_IS_PROFILE (profile), NULL);

  return g_object_new (CAPSULE_TYPE_TAB,
                       "profile", profile,
                       NULL);
}

/**
 * capsule_tab_get_profile:
 * @self: a #CapsuleTab
 *
 * Gets the profile used by the tab.
 *
 * Returns: (transfer none) (not nullable): a #CapsuleProfile
 */
CapsuleProfile *
capsule_tab_get_profile (CapsuleTab *self)
{
  g_return_val_if_fail (CAPSULE_IS_TAB (self), NULL);

  return self->profile;
}

const char *
capsule_tab_get_title_prefix (CapsuleTab *self)
{
  g_return_val_if_fail (CAPSULE_IS_TAB (self), NULL);

  return self->title_prefix ? self->title_prefix : "";
}

void
capsule_tab_set_title_prefix (CapsuleTab *self,
                              const char *title_prefix)
{
  g_return_if_fail (CAPSULE_IS_TAB (self));

  if (title_prefix != NULL && title_prefix[0] == 0)
    title_prefix = NULL;

  if (g_set_str (&self->title_prefix, title_prefix))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE_PREFIX]);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);
    }
}

char *
capsule_tab_dup_title (CapsuleTab *self)
{
  const char *window_title;

  g_return_val_if_fail (CAPSULE_IS_TAB (self), NULL);

  window_title = vte_terminal_get_window_title (VTE_TERMINAL (self->terminal));
  if (window_title != NULL && window_title[0] == 0)
    window_title = NULL;

  if (window_title && self->title_prefix)
    return g_strconcat (self->title_prefix, window_title, NULL);

  if (window_title)
    return g_strdup (window_title);

  if (self->title_prefix)
    return g_strdup (self->title_prefix);

  return g_strdup (_("Terminal"));
}

char *
capsule_tab_dup_subtitle (CapsuleTab *self)
{
  const char *current_directory_uri;
  const char *current_file_uri;

  g_return_val_if_fail (CAPSULE_IS_TAB (self), NULL);

  current_file_uri = vte_terminal_get_current_file_uri (VTE_TERMINAL (self->terminal));
  if (current_file_uri != NULL && current_file_uri[0] != 0)
    return g_strdup (current_file_uri);

  current_directory_uri = vte_terminal_get_current_directory_uri (VTE_TERMINAL (self->terminal));
  if (current_directory_uri != NULL && current_directory_uri[0] != 0)
    return g_strdup (current_directory_uri);

  return NULL;
}
