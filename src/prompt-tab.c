/*
 * prompt-tab.c
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

#include "prompt-application.h"
#include "prompt-container.h"
#include "prompt-enums.h"
#include "prompt-process.h"
#include "prompt-tab.h"
#include "prompt-tab-monitor.h"
#include "prompt-terminal.h"
#include "prompt-util.h"
#include "prompt-window.h"

typedef enum _PromptTabState
{
  PROMPT_TAB_STATE_INITIAL,
  PROMPT_TAB_STATE_SPAWNING,
  PROMPT_TAB_STATE_RUNNING,
  PROMPT_TAB_STATE_EXITED,
  PROMPT_TAB_STATE_FAILED,
} PromptTabState;

struct _PromptTab
{
  GtkWidget          parent_instance;

  char              *previous_working_directory_uri;
  PromptProfile     *profile;
  PromptProcess     *process;
  char              *title_prefix;
  PromptTabMonitor  *monitor;

  AdwBanner         *banner;
  GtkScrolledWindow *scrolled_window;
  PromptTerminal    *terminal;

  PromptZoomLevel    zoom;
  PromptTabState     state;

  guint              forced_exit : 1;
};

enum {
  PROP_0,
  PROP_ICON,
  PROP_PROCESS_LEADER_KIND,
  PROP_PROFILE,
  PROP_READ_ONLY,
  PROP_TITLE,
  PROP_TITLE_PREFIX,
  PROP_SUBTITLE,
  PROP_ZOOM,
  PROP_ZOOM_LABEL,
  N_PROPS
};

enum {
  BELL,
  N_SIGNALS
};

static void prompt_tab_respawn (PromptTab *self);

G_DEFINE_FINAL_TYPE (PromptTab, prompt_tab, GTK_TYPE_WIDGET)

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];
static double zoom_font_scales[] = {
  0,
  1.0 / (1.2 * 1.2 * 1.2 * 1.2 * 1.2 * 1.2 * 1.2),
  1.0 / (1.2 * 1.2 * 1.2 * 1.2 * 1.2 * 1.2),
  1.0 / (1.2 * 1.2 * 1.2 * 1.2 * 1.2),
  1.0 / (1.2 * 1.2 * 1.2 * 1.2),
  1.0 / (1.2 * 1.2 * 1.2),
  1.0 / (1.2 * 1.2),
  1.0 / (1.2),
  1.0,
  1.0 * 1.2,
  1.0 * 1.2 * 1.2,
  1.0 * 1.2 * 1.2 * 1.2,
  1.0 * 1.2 * 1.2 * 1.2 * 1.2,
  1.0 * 1.2 * 1.2 * 1.2 * 1.2 * 1.2,
  1.0 * 1.2 * 1.2 * 1.2 * 1.2 * 1.2 * 1.2,
  1.0 * 1.2 * 1.2 * 1.2 * 1.2 * 1.2 * 1.2 * 1,2,
};

static gboolean
prompt_tab_is_active (PromptTab *self)
{
  GtkWidget *window;

  g_assert (PROMPT_IS_TAB (self));

  if ((window = gtk_widget_get_ancestor (GTK_WIDGET (self), PROMPT_TYPE_WINDOW)))
    return prompt_window_get_active_tab (PROMPT_WINDOW (window)) == self;

  return FALSE;
}

static void
prompt_tab_update_scrollback_lines (PromptTab *self)
{
  long scrollback_lines = -1;

  g_assert (PROMPT_IS_TAB (self));

  if (prompt_profile_get_limit_scrollback (self->profile))
    scrollback_lines = prompt_profile_get_scrollback_lines (self->profile);

  vte_terminal_set_scrollback_lines (VTE_TERMINAL (self->terminal), scrollback_lines);
}

static gboolean
prompt_tab_grab_focus (GtkWidget *widget)
{
  PromptTab *self = (PromptTab *)widget;

  g_assert (PROMPT_IS_TAB (self));

  return gtk_widget_grab_focus (GTK_WIDGET (self->terminal));
}

static void
prompt_tab_wait_check_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  PromptProcess *process = (PromptProcess *)object;
  g_autoptr(PromptTab) self = user_data;
  g_autoptr(GError) error = NULL;
  PromptExitAction exit_action;
  AdwTabPage *page = NULL;
  GtkWidget *tab_view;
  gboolean success;

  g_assert (PROMPT_IS_PROCESS (process));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (PROMPT_IS_TAB (self));
  g_assert (self->state == PROMPT_TAB_STATE_RUNNING);

  g_clear_object (&self->process);

  success = prompt_process_wait_check_finish (process, result, &error);

  if (success)
    self->state = PROMPT_TAB_STATE_EXITED;
  else
    self->state = PROMPT_TAB_STATE_FAILED;

  if (self->forced_exit)
    return;

  if (prompt_process_get_if_signaled (process))
    {
      g_autofree char *title = NULL;

      title = g_strdup_printf (_("Process Exited from Signal %d"),
                               prompt_process_get_term_sig (process));

      adw_banner_set_title (self->banner, title);
      adw_banner_set_revealed (self->banner, TRUE);
      adw_banner_set_button_label (self->banner, _("_Restart"));
      gtk_actionable_set_action_name (GTK_ACTIONABLE (self->banner), "tab.respawn");
      return;
    }

  exit_action = prompt_profile_get_exit_action (self->profile);
  tab_view = gtk_widget_get_ancestor (GTK_WIDGET (self), ADW_TYPE_TAB_VIEW);

  if (ADW_IS_TAB_VIEW (tab_view))
    page = adw_tab_view_get_page (ADW_TAB_VIEW (tab_view), GTK_WIDGET (self));

  /* Always prepare the banner even if we don't show it because we may
   * display it again if the tab is removed from the parking lot and
   * restored into the window.
   */
  adw_banner_set_title (self->banner, _("Process Exited"));
  adw_banner_set_button_label (self->banner, _("_Restart"));
  gtk_actionable_set_action_name (GTK_ACTIONABLE (self->banner), "tab.respawn");

  switch (exit_action)
    {
    case PROMPT_EXIT_ACTION_RESTART:
      prompt_tab_respawn (self);
      break;

    case PROMPT_EXIT_ACTION_CLOSE:
      if (ADW_IS_TAB_VIEW (tab_view) && ADW_IS_TAB_PAGE (page))
        adw_tab_view_close_page (ADW_TAB_VIEW (tab_view), page);
      break;

    case PROMPT_EXIT_ACTION_NONE:
      adw_banner_set_revealed (self->banner, TRUE);
      break;

    default:
      g_assert_not_reached ();
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);
}

static void
prompt_tab_spawn_cb (GObject      *object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  PromptContainer *container = (PromptContainer *)object;
  g_autoptr(PromptProcess) process = NULL;
  g_autoptr(PromptTab) self = user_data;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;
  VtePty *pty;

  g_assert (PROMPT_IS_CONTAINER (container));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (PROMPT_IS_TAB (self));
  g_assert (self->state == PROMPT_TAB_STATE_SPAWNING);

  if (!(subprocess = prompt_container_spawn_finish (container, result, &error)))
    {
      const char *profile_uuid = prompt_profile_get_uuid (self->profile);

      self->state = PROMPT_TAB_STATE_FAILED;

      vte_terminal_feed (VTE_TERMINAL (self->terminal), error->message, -1);
      vte_terminal_feed (VTE_TERMINAL (self->terminal), "\r\n", -1);

      adw_banner_set_title (self->banner, _("Failed to launch terminal"));
      adw_banner_set_revealed (self->banner, TRUE);
      adw_banner_set_button_label (self->banner, _("Edit Profile"));
      gtk_actionable_set_action_target (GTK_ACTIONABLE (self->banner), "s", profile_uuid);
      gtk_actionable_set_action_name (GTK_ACTIONABLE (self->banner), "app.edit-profile");

      return;
    }

  self->state = PROMPT_TAB_STATE_RUNNING;

  pty = vte_terminal_get_pty (VTE_TERMINAL (self->terminal));
  process = prompt_process_new (subprocess, pty);

  g_set_object (&self->process, process);

  prompt_process_wait_check_async (process,
                                    NULL,
                                    prompt_tab_wait_check_cb,
                                    g_object_ref (self));
}

static void
prompt_tab_respawn (PromptTab *self)
{
  g_autofree char *default_container = NULL;
  g_autoptr(PromptContainer) container = NULL;
  g_autoptr(VtePty) new_pty = NULL;
  PromptApplication *app;
  const char *profile_uuid;
  VtePty *pty;

  g_assert (PROMPT_IS_TAB (self));
  g_assert (self->state == PROMPT_TAB_STATE_INITIAL ||
            self->state == PROMPT_TAB_STATE_EXITED ||
            self->state == PROMPT_TAB_STATE_FAILED);

  adw_banner_set_revealed (self->banner, FALSE);

  app = PROMPT_APPLICATION_DEFAULT;
  default_container = prompt_profile_dup_default_container (self->profile);
  container = prompt_application_lookup_container (app, default_container);
  profile_uuid = prompt_profile_get_uuid (self->profile);

  if (container == NULL)
    {
      g_autofree char *title = NULL;

      self->state = PROMPT_TAB_STATE_FAILED;

      title = g_strdup_printf (_("Cannot locate container “%s”"), default_container);
      adw_banner_set_title (self->banner, title);
      adw_banner_set_revealed (self->banner, TRUE);
      adw_banner_set_button_label (self->banner, _("Edit Profile"));
      gtk_actionable_set_action_target (GTK_ACTIONABLE (self->banner), "s", profile_uuid);
      gtk_actionable_set_action_name (GTK_ACTIONABLE (self->banner), "app.edit-profile");

      return;
    }

  self->state = PROMPT_TAB_STATE_SPAWNING;

  pty = vte_terminal_get_pty (VTE_TERMINAL (self->terminal));

  if (pty == NULL)
    {
      g_autoptr(GError) error = NULL;

      new_pty = vte_pty_new_sync (0, NULL, &error);

      if (new_pty == NULL)
        {
          self->state = PROMPT_TAB_STATE_FAILED;

          adw_banner_set_title (self->banner, _("Failed to create pseudo terminal device"));
          adw_banner_set_revealed (self->banner, TRUE);
          adw_banner_set_button_label (self->banner, NULL);
          gtk_actionable_set_action_name (GTK_ACTIONABLE (self->banner), NULL);

          return;
        }

      if (!vte_pty_set_utf8 (new_pty, TRUE, &error))
        g_debug ("Failed to set UTF-8 mode for PTY: %s", error->message);

      vte_terminal_set_pty (VTE_TERMINAL (self->terminal), new_pty);

      pty = new_pty;
    }

  prompt_container_spawn_async (container,
                                 pty,
                                 self->profile,
                                 self->previous_working_directory_uri,
                                 NULL,
                                 prompt_tab_spawn_cb,
                                 g_object_ref (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);
}

static void
prompt_tab_respawn_action (GtkWidget  *widget,
                           const char *action_name,
                           GVariant   *params)
{
  PromptTab *self = (PromptTab *)widget;

  g_assert (PROMPT_IS_TAB (self));

  if (self->state == PROMPT_TAB_STATE_FAILED ||
      self->state == PROMPT_TAB_STATE_EXITED)
    prompt_tab_respawn (self);
}

static void
prompt_tab_map (GtkWidget *widget)
{
  PromptTab *self = (PromptTab *)widget;

  g_assert (PROMPT_IS_TAB (widget));

  GTK_WIDGET_CLASS (prompt_tab_parent_class)->map (widget);

  if (self->state == PROMPT_TAB_STATE_INITIAL)
    prompt_tab_respawn (self);
}

static void
prompt_tab_notify_window_title_cb (PromptTab      *self,
                                   GParamSpec     *pspec,
                                   PromptTerminal *terminal)
{
  g_assert (PROMPT_IS_TAB (self));
  g_assert (PROMPT_IS_TERMINAL (terminal));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);
}

static void
prompt_tab_notify_window_subtitle_cb (PromptTab      *self,
                                      PromptTerminal *terminal)
{
  g_assert (PROMPT_IS_TAB (self));
  g_assert (PROMPT_IS_TERMINAL (terminal));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUBTITLE]);
}

static void
prompt_tab_increase_font_size_cb (PromptTab      *self,
                                  PromptTerminal *terminal)
{
  g_assert (PROMPT_IS_TAB (self));
  g_assert (PROMPT_IS_TERMINAL (terminal));

  prompt_tab_zoom_in (self);
}

static void
prompt_tab_decrease_font_size_cb (PromptTab      *self,
                                  PromptTerminal *terminal)
{
  g_assert (PROMPT_IS_TAB (self));
  g_assert (PROMPT_IS_TERMINAL (terminal));

  prompt_tab_zoom_out (self);
}

static void
prompt_tab_bell_cb (PromptTab      *self,
                    PromptTerminal *terminal)
{
  g_assert (PROMPT_IS_TAB (self));
  g_assert (PROMPT_IS_TERMINAL (terminal));

  g_signal_emit (self, signals[BELL], 0);
}

static gboolean
scrollbar_policy_to_overlay_scrolling (GBinding     *binding,
                                       const GValue *from_value,
                                       GValue       *to_value,
                                       gpointer      user_data)
{
  PromptScrollbarPolicy policy = g_value_get_enum (from_value);

  switch (policy)
    {
    case PROMPT_SCROLLBAR_POLICY_NEVER:
    case PROMPT_SCROLLBAR_POLICY_ALWAYS:
      g_value_set_boolean (to_value, FALSE);
      break;

    case PROMPT_SCROLLBAR_POLICY_SYSTEM:
      g_value_set_boolean (to_value, TRUE);
      break;

    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static gboolean
scrollbar_policy_to_vscroll_policy (GBinding     *binding,
                                    const GValue *from_value,
                                    GValue       *to_value,
                                    gpointer      user_data)
{
  PromptScrollbarPolicy policy = g_value_get_enum (from_value);

  switch (policy)
    {
    case PROMPT_SCROLLBAR_POLICY_NEVER:
      g_value_set_enum (to_value, GTK_POLICY_NEVER);
      break;

    case PROMPT_SCROLLBAR_POLICY_SYSTEM:
      g_value_set_enum (to_value, GTK_POLICY_AUTOMATIC);
      break;

    case PROMPT_SCROLLBAR_POLICY_ALWAYS:
      g_value_set_enum (to_value, GTK_POLICY_ALWAYS);
      break;

    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static void
prompt_tab_notify_process_leader_kind_cb (PromptTab        *self,
                                          GParamSpec       *pspec,
                                          PromptTabMonitor *monitor)
{
  PromptProcessLeaderKind kind;

  g_assert (PROMPT_IS_TAB (self));
  g_assert (PROMPT_IS_TAB_MONITOR (monitor));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PROCESS_LEADER_KIND]);

  /* If the process leader is superuser and our tab is not currently
   * the active tab, then let the user know by setting the attention
   * bit on the AdwTabPage.
   */
  kind = prompt_tab_monitor_get_process_leader_kind (monitor);
  if (kind == PROMPT_PROCESS_LEADER_KIND_SUPERUSER &&
      !prompt_tab_is_active (self))
    {
      GtkWidget *tab_view = gtk_widget_get_ancestor (GTK_WIDGET (self), ADW_TYPE_TAB_VIEW);
      AdwTabPage *page = adw_tab_view_get_page (ADW_TAB_VIEW (tab_view), GTK_WIDGET (self));

      adw_tab_page_set_needs_attention (page, TRUE);
    }
}

static GIcon *
prompt_tab_dup_icon (PromptTab *self)
{
  PromptProcessLeaderKind kind;

  g_assert (PROMPT_IS_TAB (self));

  kind = prompt_tab_monitor_get_process_leader_kind (self->monitor);

  switch (kind)
    {
    default:
    case PROMPT_PROCESS_LEADER_KIND_REMOTE:
      return g_themed_icon_new ("process-remote-symbolic");

    case PROMPT_PROCESS_LEADER_KIND_UNKNOWN:
      return NULL;

    case PROMPT_PROCESS_LEADER_KIND_CONTAINER:
      return g_themed_icon_new ("process-container-symbolic");

    case PROMPT_PROCESS_LEADER_KIND_SUPERUSER:
      return g_themed_icon_new ("process-superuser-symbolic");
    }
}

static void
prompt_tab_constructed (GObject *object)
{
  PromptTab *self = (PromptTab *)object;
  PromptSettings *settings;

  G_OBJECT_CLASS (prompt_tab_parent_class)->constructed (object);

  settings = prompt_application_get_settings (PROMPT_APPLICATION_DEFAULT);
  g_object_bind_property (settings, "audible-bell",
                          self->terminal, "audible-bell",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (settings, "cursor-shape",
                          self->terminal, "cursor-shape",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (settings, "cursor-blink-mode",
                          self->terminal, "cursor-blink-mode",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (settings, "font-desc",
                          self->terminal, "font-desc",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (settings, "text-blink-mode",
                          self->terminal, "text-blink-mode",
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property_full (settings, "scrollbar-policy",
                               self->scrolled_window, "vscrollbar-policy",
                               G_BINDING_SYNC_CREATE,
                               scrollbar_policy_to_vscroll_policy,
                               NULL, NULL, NULL);
  g_object_bind_property_full (settings, "scrollbar-policy",
                               self->scrolled_window, "overlay-scrolling",
                               G_BINDING_SYNC_CREATE,
                               scrollbar_policy_to_overlay_scrolling,
                               NULL, NULL, NULL);

  g_signal_connect_object (G_OBJECT (self->profile),
                           "notify::limit-scrollback",
                           G_CALLBACK (prompt_tab_update_scrollback_lines),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (G_OBJECT (self->profile),
                           "notify::scrollback-lines",
                           G_CALLBACK (prompt_tab_update_scrollback_lines),
                           self,
                           G_CONNECT_SWAPPED);
  prompt_tab_update_scrollback_lines (self);

  self->monitor = prompt_tab_monitor_new (self);

  g_signal_connect_object (self->monitor,
                           "notify::process-leader-kind",
                           G_CALLBACK (prompt_tab_notify_process_leader_kind_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
prompt_tab_dispose (GObject *object)
{
  PromptTab *self = (PromptTab *)object;
  GtkWidget *child;

  if (self->process != NULL)
    prompt_process_force_exit (self->process);

  gtk_widget_dispose_template (GTK_WIDGET (self), PROMPT_TYPE_TAB);

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self))))
    gtk_widget_unparent (child);

  g_clear_object (&self->profile);
  g_clear_object (&self->process);
  g_clear_object (&self->monitor);

  g_clear_pointer (&self->previous_working_directory_uri, g_free);
  g_clear_pointer (&self->title_prefix, g_free);

  G_OBJECT_CLASS (prompt_tab_parent_class)->dispose (object);
}

static void
prompt_tab_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  PromptTab *self = PROMPT_TAB (object);

  switch (prop_id)
    {
    case PROP_ICON:
      g_value_take_object (value, prompt_tab_dup_icon (self));
      break;

    case PROP_PROCESS_LEADER_KIND:
      g_value_set_enum (value,
                        prompt_tab_monitor_get_process_leader_kind (self->monitor));
      break;

    case PROP_PROFILE:
      g_value_set_object (value, prompt_tab_get_profile (self));
      break;

    case PROP_READ_ONLY:
      g_value_set_boolean (value, !vte_terminal_get_input_enabled (VTE_TERMINAL (self->terminal)));
      break;

    case PROP_SUBTITLE:
      g_value_take_string (value, prompt_tab_dup_subtitle (self));
      break;

    case PROP_TITLE:
      g_value_take_string (value, prompt_tab_dup_title (self));
      break;

    case PROP_TITLE_PREFIX:
      g_value_set_string (value, prompt_tab_get_title_prefix (self));
      break;

    case PROP_ZOOM:
      g_value_set_enum (value, prompt_tab_get_zoom (self));
      break;

    case PROP_ZOOM_LABEL:
      g_value_take_string (value, prompt_tab_dup_zoom_label (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_tab_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  PromptTab *self = PROMPT_TAB (object);

  switch (prop_id)
    {
    case PROP_PROFILE:
      self->profile = g_value_dup_object (value);
      break;

    case PROP_READ_ONLY:
      vte_terminal_set_input_enabled (VTE_TERMINAL (self->terminal), !g_value_get_boolean (value));
      break;

    case PROP_TITLE_PREFIX:
      prompt_tab_set_title_prefix (self, g_value_get_string (value));
      break;

    case PROP_ZOOM:
      prompt_tab_set_zoom (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_tab_class_init (PromptTabClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = prompt_tab_constructed;
  object_class->dispose = prompt_tab_dispose;
  object_class->get_property = prompt_tab_get_property;
  object_class->set_property = prompt_tab_set_property;

  widget_class->grab_focus = prompt_tab_grab_focus;
  widget_class->map = prompt_tab_map;

  properties[PROP_ICON] =
    g_param_spec_object ("icon", NULL, NULL,
                         G_TYPE_ICON,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_PROCESS_LEADER_KIND] =
    g_param_spec_enum ("process-leader-kind", NULL, NULL,
                       PROMPT_TYPE_PROCESS_LEADER_KIND,
                       PROMPT_PROCESS_LEADER_KIND_UNKNOWN,
                       (G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_PROFILE] =
    g_param_spec_object ("profile", NULL, NULL,
                         PROMPT_TYPE_PROFILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_READ_ONLY] =
    g_param_spec_boolean ("read-only", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
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

  properties[PROP_ZOOM] =
    g_param_spec_enum ("zoom", NULL, NULL,
                       PROMPT_TYPE_ZOOM_LEVEL,
                       PROMPT_ZOOM_LEVEL_DEFAULT,
                       (G_PARAM_READWRITE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_ZOOM_LABEL] =
    g_param_spec_string ("zoom-label", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals[BELL] =
    g_signal_new_class_handler ("bell",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                NULL,
                                NULL, NULL,
                                NULL,
                                G_TYPE_NONE, 0);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Prompt/prompt-tab.ui");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);

  gtk_widget_class_bind_template_child (widget_class, PromptTab, banner);
  gtk_widget_class_bind_template_child (widget_class, PromptTab, terminal);
  gtk_widget_class_bind_template_child (widget_class, PromptTab, scrolled_window);

  gtk_widget_class_bind_template_callback (widget_class, prompt_tab_notify_window_title_cb);
  gtk_widget_class_bind_template_callback (widget_class, prompt_tab_notify_window_subtitle_cb);
  gtk_widget_class_bind_template_callback (widget_class, prompt_tab_increase_font_size_cb);
  gtk_widget_class_bind_template_callback (widget_class, prompt_tab_decrease_font_size_cb);
  gtk_widget_class_bind_template_callback (widget_class, prompt_tab_bell_cb);

  gtk_widget_class_install_action (widget_class, "tab.respawn", NULL, prompt_tab_respawn_action);

  g_type_ensure (PROMPT_TYPE_TERMINAL);
}

static void
prompt_tab_init (PromptTab *self)
{
  self->state = PROMPT_TAB_STATE_INITIAL;
  self->zoom = PROMPT_ZOOM_LEVEL_DEFAULT;

  gtk_widget_init_template (GTK_WIDGET (self));
}

PromptTab *
prompt_tab_new (PromptProfile *profile)
{
  g_return_val_if_fail (PROMPT_IS_PROFILE (profile), NULL);

  return g_object_new (PROMPT_TYPE_TAB,
                       "profile", profile,
                       NULL);
}

/**
 * prompt_tab_get_profile:
 * @self: a #PromptTab
 *
 * Gets the profile used by the tab.
 *
 * Returns: (transfer none) (not nullable): a #PromptProfile
 */
PromptProfile *
prompt_tab_get_profile (PromptTab *self)
{
  g_return_val_if_fail (PROMPT_IS_TAB (self), NULL);

  return self->profile;
}

const char *
prompt_tab_get_title_prefix (PromptTab *self)
{
  g_return_val_if_fail (PROMPT_IS_TAB (self), NULL);

  return self->title_prefix ? self->title_prefix : "";
}

void
prompt_tab_set_title_prefix (PromptTab *self,
                              const char *title_prefix)
{
  g_return_if_fail (PROMPT_IS_TAB (self));

  if (title_prefix != NULL && title_prefix[0] == 0)
    title_prefix = NULL;

  if (g_set_str (&self->title_prefix, title_prefix))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE_PREFIX]);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);
    }
}

char *
prompt_tab_dup_title (PromptTab *self)
{
  const char *window_title;
  GString *gstr;

  g_return_val_if_fail (PROMPT_IS_TAB (self), NULL);

  gstr = g_string_new (self->title_prefix);

  if ((window_title = vte_terminal_get_window_title (VTE_TERMINAL (self->terminal))) && window_title[0])
    g_string_append (gstr, window_title);
  else
    g_string_append (gstr, _("Terminal"));

  if (self->state == PROMPT_TAB_STATE_EXITED)
    g_string_append_printf (gstr, " (%s)", _("Exited"));
  else if (self->state == PROMPT_TAB_STATE_FAILED)
    g_string_append_printf (gstr, " (%s)", _("Failed"));

  return g_string_free (gstr, FALSE);
}

static char *
prompt_tab_collapse_uri (const char *uri)
{
  g_autoptr(GFile) file = NULL;

  if (uri == NULL)
    return NULL;

  if (!(file = g_file_new_for_uri (uri)))
    return NULL;

  if (g_file_is_native (file))
    return prompt_path_collapse (g_file_peek_path (file));

  return strdup (uri);
}

char *
prompt_tab_dup_subtitle (PromptTab *self)
{
  const char *current_directory_uri;
  const char *current_file_uri;

  g_return_val_if_fail (PROMPT_IS_TAB (self), NULL);

  current_file_uri = vte_terminal_get_current_file_uri (VTE_TERMINAL (self->terminal));
  if (current_file_uri != NULL && current_file_uri[0] != 0)
    return prompt_tab_collapse_uri (current_file_uri);

  current_directory_uri = vte_terminal_get_current_directory_uri (VTE_TERMINAL (self->terminal));
  if (current_directory_uri != NULL && current_directory_uri[0] != 0)
    return prompt_tab_collapse_uri (current_directory_uri);

  return NULL;
}

const char *
prompt_tab_get_current_directory_uri (PromptTab *self)
{
  g_return_val_if_fail (PROMPT_IS_TAB (self), NULL);

  return vte_terminal_get_current_directory_uri (VTE_TERMINAL (self->terminal));
}

void
prompt_tab_set_previous_working_directory_uri (PromptTab  *self,
                                               const char *previous_working_directory_uri)
{
  g_return_if_fail (PROMPT_IS_TAB (self));

  g_set_str (&self->previous_working_directory_uri, previous_working_directory_uri);
}

static void
prompt_tab_apply_zoom (PromptTab *self)
{
  g_assert (PROMPT_IS_TAB (self));

  vte_terminal_set_font_scale (VTE_TERMINAL (self->terminal),
                               zoom_font_scales[self->zoom]);
}

PromptZoomLevel
prompt_tab_get_zoom (PromptTab *self)
{
  g_return_val_if_fail (PROMPT_IS_TAB (self), 0);

  return self->zoom;
}

void
prompt_tab_set_zoom (PromptTab       *self,
                     PromptZoomLevel  zoom)
{
  g_return_if_fail (PROMPT_IS_TAB (self));
  g_return_if_fail (zoom >= PROMPT_ZOOM_LEVEL_MINUS_7 &&
                    zoom <= PROMPT_ZOOM_LEVEL_PLUS_7);

  if (zoom != self->zoom)
    {
      self->zoom = zoom;
      prompt_tab_apply_zoom (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ZOOM]);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ZOOM_LABEL]);
    }
}

void
prompt_tab_zoom_in (PromptTab *self)
{
  g_return_if_fail (PROMPT_IS_TAB (self));

  if (self->zoom < PROMPT_ZOOM_LEVEL_PLUS_7)
    prompt_tab_set_zoom (self, self->zoom + 1);
}

void
prompt_tab_zoom_out (PromptTab *self)
{
  g_return_if_fail (PROMPT_IS_TAB (self));

  if (self->zoom > PROMPT_ZOOM_LEVEL_MINUS_7)
    prompt_tab_set_zoom (self, self->zoom - 1);
}

PromptTerminal *
prompt_tab_get_terminal (PromptTab *self)
{
  g_return_val_if_fail (PROMPT_IS_TAB (self), NULL);

  return self->terminal;
}

void
prompt_tab_raise (PromptTab *self)
{
  AdwTabView *tab_view;
  AdwTabPage *tab_page;

  g_return_if_fail (PROMPT_IS_TAB (self));

  if ((tab_view = ADW_TAB_VIEW (gtk_widget_get_ancestor (GTK_WIDGET (self), ADW_TYPE_TAB_VIEW))) &&
      (tab_page = adw_tab_view_get_page (tab_view, GTK_WIDGET (self))))
    adw_tab_view_set_selected_page (tab_view, tab_page);
}

gboolean
prompt_tab_is_running (PromptTab *self)
{
  g_return_val_if_fail (PROMPT_IS_TAB (self), 0);

  if (self->process == NULL)
    return FALSE;

  return prompt_process_has_leader (self->process);
}

void
prompt_tab_force_quit (PromptTab *self)
{
  g_return_if_fail (PROMPT_IS_TAB (self));

  self->forced_exit = TRUE;

  if (self->process != NULL)
    prompt_process_force_exit (self->process);
}

PromptProcess *
prompt_tab_get_process (PromptTab *self)
{
  g_return_val_if_fail (PROMPT_IS_TAB (self), NULL);

  return self->process;
}

char *
prompt_tab_dup_zoom_label (PromptTab *self)
{
  g_return_val_if_fail (PROMPT_IS_TAB (self), 0);

  if (self->zoom == PROMPT_ZOOM_LEVEL_DEFAULT)
    return g_strdup ("100%");

  return g_strdup_printf ("%.0lf%%", zoom_font_scales[self->zoom] * 100.0);
}

void
prompt_tab_show_banner (PromptTab *self)
{
  g_return_if_fail (PROMPT_IS_TAB (self));

  adw_banner_set_revealed (self->banner, TRUE);
}
