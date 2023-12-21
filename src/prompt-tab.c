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

#include <libportal/portal.h>
#include <libportal-gtk4/portal-gtk4.h>

#include "prompt-agent-ipc.h"
#include "prompt-application.h"
#include "prompt-enums.h"
#include "prompt-inspector.h"
#include "prompt-tab.h"
#include "prompt-tab-monitor.h"
#include "prompt-tab-notify.h"
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
  GtkWidget                parent_instance;

  char                    *initial_working_directory_uri;
  char                    *previous_working_directory_uri;
  PromptProfile           *profile;
  PromptIpcProcess        *process;
  char                    *title_prefix;
  PromptTabMonitor        *monitor;
  char                    *uuid;
  PromptIpcContainer      *container_at_creation;
  char                   **command;
  char                    *initial_title;
  GdkTexture              *cached_texture;
  AdwBanner               *banner;
  GtkScrolledWindow       *scrolled_window;
  PromptTerminal          *terminal;
  char                    *command_line;
  char                    *program_name;
  PromptTabNotify          notify;

  PromptTabState           state;
  GPid                     pid;

  PromptZoomLevel          zoom : 5;
  PromptProcessLeaderKind  leader_kind : 3;
  guint                    has_foreground_process : 1;
  guint                    forced_exit : 1;
};

enum {
  PROP_0,
  PROP_COMMAND_LINE,
  PROP_ICON,
  PROP_PROCESS_LEADER_KIND,
  PROP_PROFILE,
  PROP_READ_ONLY,
  PROP_SUBTITLE,
  PROP_TITLE,
  PROP_TITLE_PREFIX,
  PROP_UUID,
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
static XdpPortal *portal;
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
on_scroll_scrolled_cb (GtkEventControllerScroll *scroll,
                       double                    dx,
                       double                    dy,
                       PromptTab                *self)
{
  GdkModifierType mods;

  g_assert (GTK_IS_EVENT_CONTROLLER_SCROLL (scroll));
  g_assert (PROMPT_IS_TAB (self));

  mods = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (scroll));

  if ((mods & GDK_CONTROL_MASK) != 0)
    {
      if (dy < 0)
        prompt_tab_zoom_in (self);
      else if (dy > 0)
        prompt_tab_zoom_out (self);

      return TRUE;
    }

  return FALSE;
}

static void
on_scroll_begin_cb (GtkEventControllerScroll *scroll,
                    PromptTab                *self)
{
  GdkModifierType state;

  g_assert (GTK_IS_EVENT_CONTROLLER_SCROLL (scroll));
  g_assert (PROMPT_IS_TAB (self));

  state = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (scroll));

  if ((state & GDK_CONTROL_MASK) != 0)
    gtk_event_controller_scroll_set_flags (scroll,
                                           GTK_EVENT_CONTROLLER_SCROLL_VERTICAL |
                                           GTK_EVENT_CONTROLLER_SCROLL_DISCRETE);
}

static void
on_scroll_end_cb (GtkEventControllerScroll *scroll,
                  PromptTab                *self)
{
  g_assert (GTK_IS_EVENT_CONTROLLER_SCROLL (scroll));
  g_assert (PROMPT_IS_TAB (self));

  gtk_event_controller_scroll_set_flags (scroll, GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
}

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
prompt_tab_wait_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  PromptApplication *app = (PromptApplication *)object;
  g_autoptr(PromptTab) self = user_data;
  g_autoptr(GError) error = NULL;
  PromptExitAction exit_action;
  AdwTabPage *page = NULL;
  GtkWidget *tab_view;
  int exit_code;

  g_assert (PROMPT_IS_APPLICATION (app));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (PROMPT_IS_TAB (self));
  g_assert (self->state == PROMPT_TAB_STATE_RUNNING);

  g_clear_object (&self->process);

  exit_code = prompt_application_wait_finish (app, result, &error);

  if (error == NULL && WIFEXITED (exit_code) && WEXITSTATUS (exit_code) == 0)
    self->state = PROMPT_TAB_STATE_EXITED;
  else
    self->state = PROMPT_TAB_STATE_FAILED;

  if (self->forced_exit)
    return;

  if (WIFSIGNALED (exit_code))
    {
      g_autofree char *title = NULL;

      title = g_strdup_printf (_("Process Exited from Signal %d"), WTERMSIG (exit_code));

      adw_banner_set_title (self->banner, title);
      adw_banner_set_button_label (self->banner, _("_Restart"));
      gtk_actionable_set_action_name (GTK_ACTIONABLE (self->banner), "tab.respawn");
      gtk_widget_set_visible (GTK_WIDGET (self->banner), TRUE);
      return;
    }

  exit_action = prompt_profile_get_exit_action (self->profile);
  tab_view = gtk_widget_get_ancestor (GTK_WIDGET (self), ADW_TYPE_TAB_VIEW);

  /* If this was started with something like prompt_window_new_for_command()
   * then we just want to exit the application (so allow tab to close).
   */
  if (self->command != NULL)
    exit_action = PROMPT_EXIT_ACTION_CLOSE;

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
        {
          if (adw_tab_page_get_pinned (page))
            adw_tab_view_set_page_pinned (ADW_TAB_VIEW (tab_view), page, FALSE);
          adw_tab_view_close_page (ADW_TAB_VIEW (tab_view), page);
        }
      break;

    case PROMPT_EXIT_ACTION_NONE:
      gtk_widget_set_visible (GTK_WIDGET (self->banner), TRUE);
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
  PromptApplication *app = (PromptApplication *)object;
  g_autoptr(PromptIpcProcess) process = NULL;
  g_autoptr(PromptTab) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (PROMPT_IS_TAB (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (PROMPT_IS_TAB (self));
  g_assert (self->state == PROMPT_TAB_STATE_SPAWNING);

  if (!(process = prompt_application_spawn_finish (app, result, &error)))
    {
      const char *profile_uuid = prompt_profile_get_uuid (self->profile);

      self->state = PROMPT_TAB_STATE_FAILED;

      vte_terminal_feed (VTE_TERMINAL (self->terminal), error->message, -1);
      vte_terminal_feed (VTE_TERMINAL (self->terminal), "\r\n", -1);

      adw_banner_set_title (self->banner, _("Failed to launch terminal"));
      adw_banner_set_button_label (self->banner, _("Edit Profile"));
      gtk_actionable_set_action_target (GTK_ACTIONABLE (self->banner), "s", profile_uuid);
      gtk_actionable_set_action_name (GTK_ACTIONABLE (self->banner), "app.edit-profile");
      gtk_widget_set_visible (GTK_WIDGET (self->banner), TRUE);

      return;
    }

  self->state = PROMPT_TAB_STATE_RUNNING;

  g_set_object (&self->process, process);

  prompt_application_wait_async (app,
                                 process,
                                 NULL,
                                 prompt_tab_wait_cb,
                                 g_object_ref (self));
}

static void
prompt_tab_respawn (PromptTab *self)
{
  g_autofree char *default_container = NULL;
  g_autoptr(PromptIpcContainer) container = NULL;
  g_autoptr(VtePty) new_pty = NULL;
  PromptApplication *app;
  const char *profile_uuid;
  const char *cwd;
  VtePty *pty;

  g_assert (PROMPT_IS_TAB (self));
  g_assert (self->state == PROMPT_TAB_STATE_INITIAL ||
            self->state == PROMPT_TAB_STATE_EXITED ||
            self->state == PROMPT_TAB_STATE_FAILED);

  gtk_widget_set_visible (GTK_WIDGET (self->banner), FALSE);

  app = PROMPT_APPLICATION_DEFAULT;
  profile_uuid = prompt_profile_get_uuid (self->profile);
  default_container = prompt_profile_dup_default_container (self->profile);

  if (self->container_at_creation != NULL)
    container = g_object_ref (self->container_at_creation);
  else
    container = prompt_application_lookup_container (app, default_container);

  if (container == NULL)
    {
      g_autofree char *title = NULL;

      self->state = PROMPT_TAB_STATE_FAILED;

      title = g_strdup_printf (_("Cannot locate container “%s”"), default_container);
      adw_banner_set_title (self->banner, title);
      adw_banner_set_button_label (self->banner, _("Edit Profile"));
      gtk_actionable_set_action_target (GTK_ACTIONABLE (self->banner), "s", profile_uuid);
      gtk_actionable_set_action_name (GTK_ACTIONABLE (self->banner), "app.edit-profile");
      gtk_widget_set_visible (GTK_WIDGET (self->banner), TRUE);

      return;
    }

  self->state = PROMPT_TAB_STATE_SPAWNING;

  pty = vte_terminal_get_pty (VTE_TERMINAL (self->terminal));

  if (pty == NULL)
    {
      g_autoptr(GError) error = NULL;

      new_pty = prompt_application_create_pty (PROMPT_APPLICATION_DEFAULT, &error);

      if (new_pty == NULL)
        {
          self->state = PROMPT_TAB_STATE_FAILED;

          adw_banner_set_title (self->banner, _("Failed to create pseudo terminal device"));
          adw_banner_set_button_label (self->banner, NULL);
          gtk_actionable_set_action_name (GTK_ACTIONABLE (self->banner), NULL);
          gtk_widget_set_visible (GTK_WIDGET (self->banner), TRUE);

          return;
        }

      vte_terminal_set_pty (VTE_TERMINAL (self->terminal), new_pty);

      pty = new_pty;
    }

  cwd = self->previous_working_directory_uri;
  if (self->initial_working_directory_uri)
    cwd = self->initial_working_directory_uri;

  prompt_application_spawn_async (PROMPT_APPLICATION_DEFAULT,
                                  container,
                                  self->profile,
                                  cwd,
                                  pty,
                                  (const char * const *)self->command,
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
prompt_tab_inspect_action (GtkWidget  *widget,
                           const char *action_name,
                           GVariant   *params)
{
  PromptTab *self = (PromptTab *)widget;
  PromptInspector *inspector;
  GtkRoot *root;

  g_assert (PROMPT_IS_TAB (self));

  inspector = prompt_inspector_new (self);
  root = gtk_widget_get_root (GTK_WIDGET (self));

  gtk_window_set_transient_for (GTK_WINDOW (inspector), GTK_WINDOW (root));
  gtk_window_set_modal (GTK_WINDOW (inspector), FALSE);
  gtk_window_present (GTK_WINDOW (inspector));
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
prompt_tab_notify_contains_focus_cb (PromptTab               *self,
                                     GParamSpec              *pspec,
                                     GtkEventControllerFocus *focus)
{
  g_assert (PROMPT_IS_TAB (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_FOCUS (focus));

  if (gtk_event_controller_focus_contains_focus (focus))
    {
      prompt_tab_set_needs_attention (self, FALSE);
      g_application_withdraw_notification (G_APPLICATION (PROMPT_APPLICATION_DEFAULT),
                                           self->uuid);
    }
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

static PromptIpcContainer *
prompt_tab_discover_container (PromptTab *self)
{
  VteTerminal *terminal = VTE_TERMINAL (self->terminal);
  const char *current_container_name = vte_terminal_get_current_container_name (terminal);
  const char *current_container_runtime = vte_terminal_get_current_container_runtime (terminal);

  return prompt_application_find_container_by_name (PROMPT_APPLICATION_DEFAULT,
                                                    current_container_runtime,
                                                    current_container_name);
}

static GIcon *
prompt_tab_dup_icon (PromptTab *self)
{
  PromptProcessLeaderKind kind;

  g_assert (PROMPT_IS_TAB (self));

  kind = self->leader_kind;

  switch (kind)
    {
    default:
    case PROMPT_PROCESS_LEADER_KIND_REMOTE:
      return g_themed_icon_new ("process-remote-symbolic");

    case PROMPT_PROCESS_LEADER_KIND_SUPERUSER:
      return g_themed_icon_new ("process-superuser-symbolic");

    case PROMPT_PROCESS_LEADER_KIND_CONTAINER:
    case PROMPT_PROCESS_LEADER_KIND_UNKNOWN:
      {
        g_autoptr(PromptIpcContainer) container = NULL;
        const char *icon_name;

        if (!(container = prompt_tab_discover_container (self)))
          g_set_object (&container, self->container_at_creation);

        if (container != NULL &&
            (icon_name = prompt_ipc_container_get_icon_name (container)) &&
            icon_name[0] != 0)
          return g_themed_icon_new (icon_name);
      }
      return NULL;
    }
}

static void
prompt_tab_notify_palette_cb (PromptTab      *self,
                              GParamSpec     *pspec,
                              PromptTerminal *terminal)
{
  GtkWidget *view;
  AdwTabPage *page;

  g_assert (PROMPT_IS_TAB (self));
  g_assert (PROMPT_IS_TERMINAL (terminal));

  g_clear_object (&self->cached_texture);

  if ((view = gtk_widget_get_ancestor (GTK_WIDGET (self), ADW_TYPE_TAB_VIEW)) &&
      (page = adw_tab_view_get_page (ADW_TAB_VIEW (view), GTK_WIDGET (self))))
    adw_tab_page_invalidate_thumbnail (page);
}

static void
prompt_tab_update_scrollbar_policy (PromptTab *self)
{
  PromptSettings *settings;
  PromptScrollbarPolicy policy;

  g_assert (PROMPT_IS_TAB (self));

  settings = prompt_application_get_settings (PROMPT_APPLICATION_DEFAULT);
  policy = prompt_settings_get_scrollbar_policy (settings);

  switch (policy)
    {
    case PROMPT_SCROLLBAR_POLICY_NEVER:
      gtk_scrolled_window_set_overlay_scrolling (self->scrolled_window, FALSE);
      gtk_scrolled_window_set_policy (self->scrolled_window, GTK_POLICY_NEVER, GTK_POLICY_EXTERNAL);
      break;

    case PROMPT_SCROLLBAR_POLICY_ALWAYS:
      gtk_scrolled_window_set_overlay_scrolling (self->scrolled_window, FALSE);
      gtk_scrolled_window_set_policy (self->scrolled_window, GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
      break;

    case PROMPT_SCROLLBAR_POLICY_SYSTEM:
      if (prompt_application_get_overlay_scrollbars (PROMPT_APPLICATION_DEFAULT))
        {
          gtk_scrolled_window_set_overlay_scrolling (self->scrolled_window, TRUE);
          gtk_scrolled_window_set_policy (self->scrolled_window, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        }
      else
        {
          gtk_scrolled_window_set_overlay_scrolling (self->scrolled_window, FALSE);
          gtk_scrolled_window_set_policy (self->scrolled_window, GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
        }

      break;

    default:
      g_assert_not_reached ();
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

  g_signal_connect_object (PROMPT_APPLICATION_DEFAULT,
                           "notify::overlay-scrollbars",
                           G_CALLBACK (prompt_tab_update_scrollbar_policy),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (settings,
                           "notify::scrollbar-policy",
                           G_CALLBACK (prompt_tab_update_scrollbar_policy),
                           self,
                           G_CONNECT_SWAPPED);
  prompt_tab_update_scrollbar_policy (self);

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
}

static void
prompt_tab_snapshot (GtkWidget   *widget,
                     GtkSnapshot *snapshot)
{
  PromptTab *self = (PromptTab *)widget;
  PromptWindow *window;
  GdkRGBA bg;
  gboolean animating;
  int width;
  int height;

  g_assert (PROMPT_IS_TAB (self));
  g_assert (GTK_IS_SNAPSHOT (snapshot));

  window = PROMPT_WINDOW (gtk_widget_get_root (widget));
  animating = prompt_window_is_animating (window);
  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  vte_terminal_get_color_background_for_draw (VTE_TERMINAL (self->terminal), &bg);

  if (animating &&
      prompt_window_get_active_tab (window) == self)
    {

      if (self->cached_texture == NULL)
        {
          GtkSnapshot *sub_snapshot = gtk_snapshot_new ();
          int scale_factor = gtk_widget_get_scale_factor (widget);
          g_autoptr(GskRenderNode) node = NULL;
          graphene_matrix_t matrix;
          GskRenderer *renderer;

          gtk_snapshot_scale (sub_snapshot, scale_factor, scale_factor);
          gtk_snapshot_append_color (sub_snapshot,
                                     &bg,
                                     &GRAPHENE_RECT_INIT (0, 0, width, height));

          if (gtk_widget_compute_transform (GTK_WIDGET (self->terminal),
                                            GTK_WIDGET (self),
                                            &matrix))
            {
              gtk_snapshot_transform_matrix (sub_snapshot, &matrix);
              GTK_WIDGET_GET_CLASS (self->terminal)->snapshot (GTK_WIDGET (self->terminal), sub_snapshot);
            }

          node = gtk_snapshot_free_to_node (sub_snapshot);
          renderer = gtk_native_get_renderer (GTK_NATIVE (window));

          self->cached_texture = gsk_renderer_render_texture (renderer,
                                                              node,
                                                              &GRAPHENE_RECT_INIT (0,
                                                                                   0,
                                                                                   width * scale_factor,
                                                                                   height * scale_factor));
        }

      gtk_snapshot_append_texture (snapshot,
                                   self->cached_texture,
                                   &GRAPHENE_RECT_INIT (0, 0, width, height));
    }
  else
    {
      g_clear_object (&self->cached_texture);

      if (animating)
        gtk_snapshot_append_color (snapshot,
                                   &bg,
                                   &GRAPHENE_RECT_INIT (0, 0, width, height));

      GTK_WIDGET_CLASS (prompt_tab_parent_class)->snapshot (widget, snapshot);
    }
}

static void
prompt_tab_size_allocate (GtkWidget *widget,
                          int        width,
                          int        height,
                          int        baseline)
{
  PromptTab *self = (PromptTab *)widget;

  g_assert (PROMPT_IS_TAB (self));

  GTK_WIDGET_CLASS (prompt_tab_parent_class)->size_allocate (widget, width, height, baseline);

  g_clear_object (&self->cached_texture);
}

static void
prompt_tab_invalidate_icon (PromptTab *self)
{
  g_assert (PROMPT_IS_TAB (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON]);
}

static gboolean
prompt_tab_match_clicked_cb (PromptTab       *self,
                             double           x,
                             double           y,
                             int              button,
                             GdkModifierType  state,
                             const char      *match,
                             PromptTerminal  *terminal)
{
  g_assert (PROMPT_IS_TAB (self));
  g_assert (match != NULL);
  g_assert (PROMPT_IS_TERMINAL (terminal));

  if (!prompt_str_empty0 (match))
    {
      prompt_tab_open_uri (self, match);
      return TRUE;
    }

  return FALSE;
}

static void
prompt_tab_dispose (GObject *object)
{
  PromptTab *self = (PromptTab *)object;
  GtkWidget *child;

  prompt_tab_notify_destroy (&self->notify);

  if (self->process != NULL)
    prompt_ipc_process_call_send_signal (self->process, SIGKILL, NULL, NULL, NULL);

  gtk_widget_dispose_template (GTK_WIDGET (self), PROMPT_TYPE_TAB);

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self))))
    gtk_widget_unparent (child);

  g_clear_object (&self->cached_texture);
  g_clear_object (&self->profile);
  g_clear_object (&self->process);
  g_clear_object (&self->monitor);
  g_clear_object (&self->container_at_creation);

  g_clear_pointer (&self->initial_working_directory_uri, g_free);
  g_clear_pointer (&self->previous_working_directory_uri, g_free);
  g_clear_pointer (&self->title_prefix, g_free);
  g_clear_pointer (&self->initial_title, g_free);
  g_clear_pointer (&self->command, g_strfreev);
  g_clear_pointer (&self->command_line, g_free);
  g_clear_pointer (&self->program_name, g_free);

  G_OBJECT_CLASS (prompt_tab_parent_class)->dispose (object);
}

static void
prompt_tab_finalize (GObject *object)
{
  PromptTab *self = (PromptTab *)object;

  g_clear_pointer (&self->uuid, g_free);

  G_OBJECT_CLASS (prompt_tab_parent_class)->finalize (object);
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
    case PROP_COMMAND_LINE:
      g_value_set_string (value, self->command_line);
      break;

    case PROP_ICON:
      g_value_take_object (value, prompt_tab_dup_icon (self));
      break;

    case PROP_PROCESS_LEADER_KIND:
      g_value_set_enum (value, self->leader_kind);
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

    case PROP_UUID:
      g_value_set_string (value, prompt_tab_get_uuid (self));
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
  object_class->finalize = prompt_tab_finalize;
  object_class->get_property = prompt_tab_get_property;
  object_class->set_property = prompt_tab_set_property;

  widget_class->grab_focus = prompt_tab_grab_focus;
  widget_class->map = prompt_tab_map;
  widget_class->snapshot = prompt_tab_snapshot;
  widget_class->size_allocate = prompt_tab_size_allocate;

  properties[PROP_COMMAND_LINE] =
    g_param_spec_string ("command-line", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

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

  properties[PROP_UUID] =
    g_param_spec_string ("uuid", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
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
  gtk_widget_class_set_css_name (widget_class, "prompttab");

  gtk_widget_class_bind_template_child (widget_class, PromptTab, banner);
  gtk_widget_class_bind_template_child (widget_class, PromptTab, terminal);
  gtk_widget_class_bind_template_child (widget_class, PromptTab, scrolled_window);

  gtk_widget_class_bind_template_callback (widget_class, prompt_tab_notify_contains_focus_cb);
  gtk_widget_class_bind_template_callback (widget_class, prompt_tab_notify_window_title_cb);
  gtk_widget_class_bind_template_callback (widget_class, prompt_tab_notify_window_subtitle_cb);
  gtk_widget_class_bind_template_callback (widget_class, prompt_tab_increase_font_size_cb);
  gtk_widget_class_bind_template_callback (widget_class, prompt_tab_decrease_font_size_cb);
  gtk_widget_class_bind_template_callback (widget_class, prompt_tab_notify_palette_cb);
  gtk_widget_class_bind_template_callback (widget_class, prompt_tab_bell_cb);
  gtk_widget_class_bind_template_callback (widget_class, prompt_tab_invalidate_icon);
  gtk_widget_class_bind_template_callback (widget_class, prompt_tab_match_clicked_cb);

  gtk_widget_class_install_action (widget_class, "tab.respawn", NULL, prompt_tab_respawn_action);
  gtk_widget_class_install_action (widget_class, "tab.inspect", NULL, prompt_tab_inspect_action);

  g_type_ensure (PROMPT_TYPE_TERMINAL);
}

static void
prompt_tab_init (PromptTab *self)
{
  GtkEventController *controller;

  self->state = PROMPT_TAB_STATE_INITIAL;
  self->zoom = PROMPT_ZOOM_LEVEL_DEFAULT;
  self->uuid = g_uuid_string_random ();

  gtk_widget_init_template (GTK_WIDGET (self));

  prompt_tab_notify_init (&self->notify, self);

  controller = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
  gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
  g_signal_connect (controller,
                    "scroll",
                    G_CALLBACK (on_scroll_scrolled_cb),
                    self);
  g_signal_connect (controller,
                    "scroll-begin",
                    G_CALLBACK (on_scroll_begin_cb),
                    self);
  g_signal_connect (controller,
                    "scroll-end",
                    G_CALLBACK (on_scroll_end_cb),
                    self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);
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

  if (prompt_str_empty0 (title_prefix))
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
  else if (self->command != NULL && self->command[0] != NULL)
    g_string_append (gstr, self->command[0]);
  else if (self->initial_title != NULL)
    g_string_append (gstr, self->initial_title);
  else
    g_string_append (gstr, _("Terminal"));

  if (self->state == PROMPT_TAB_STATE_EXITED)
    g_string_append_printf (gstr, " (%s)", _("Exited"));
  else if (self->state == PROMPT_TAB_STATE_FAILED)
    g_string_append_printf (gstr, " (%s)", _("Failed"));
  else if (self->has_foreground_process &&
           !prompt_str_empty0 (self->command_line) &&
           !prompt_str_empty0 (self->program_name) &&
           !prompt_is_shell (self->program_name))
    g_string_append_printf (gstr, " — %s", self->command_line);

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
prompt_tab_set_initial_working_directory_uri (PromptTab  *self,
                                              const char *initial_working_directory_uri)
{
  g_return_if_fail (PROMPT_IS_TAB (self));

  g_set_str (&self->initial_working_directory_uri, initial_working_directory_uri);
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

/**
 * prompt_tab_is_running:
 * @self: a #PromptTab
 * @cmdline: (out) (nullable): a location for the command line
 *
 * Returns: %TRUE if there is a command running
 */
gboolean
prompt_tab_is_running (PromptTab  *self,
                       char      **cmdline)
{
  g_return_val_if_fail (PROMPT_IS_TAB (self), FALSE);

  prompt_tab_poll_agent (self);

  if (cmdline != NULL)
    *cmdline = g_strdup (self->command_line);

  if (self->has_foreground_process && self->program_name != NULL)
    return !prompt_is_shell (self->program_name);

  return FALSE;
}

void
prompt_tab_force_quit (PromptTab *self)
{
  g_return_if_fail (PROMPT_IS_TAB (self));

  self->forced_exit = TRUE;

  if (self->process != NULL)
    prompt_ipc_process_call_send_signal (self->process, SIGKILL, NULL, NULL, NULL);
}

PromptIpcProcess *
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

  gtk_widget_set_visible (GTK_WIDGET (self->banner), TRUE);
}

void
prompt_tab_set_needs_attention (PromptTab *self,
                                gboolean   needs_attention)
{
  GtkWidget *tab_view;
  AdwTabPage *page;

  g_return_if_fail (PROMPT_IS_TAB (self));

  if ((tab_view = gtk_widget_get_ancestor (GTK_WIDGET (self), ADW_TYPE_TAB_VIEW)) &&
      (page = adw_tab_view_get_page (ADW_TAB_VIEW (tab_view), GTK_WIDGET (self))))
    adw_tab_page_set_needs_attention (page, needs_attention);
}

const char *
prompt_tab_get_uuid (PromptTab *self)
{
  g_return_val_if_fail (PROMPT_IS_TAB (self), NULL);

  return self->uuid;
}

PromptIpcContainer *
prompt_tab_dup_container (PromptTab *self)
{
  g_autoptr(PromptIpcContainer) container = NULL;
  const char *runtime;
  const char *name;

  g_return_val_if_fail (PROMPT_IS_TAB (self), NULL);

  if ((runtime = vte_terminal_get_current_container_runtime (VTE_TERMINAL (self->terminal))) &&
      (name = vte_terminal_get_current_container_name (VTE_TERMINAL (self->terminal))))
    container = prompt_application_find_container_by_name (PROMPT_APPLICATION_DEFAULT, runtime, name);

  if (container == NULL)
    g_set_object (&container, self->container_at_creation);

  return g_steal_pointer (&container);
}

void
prompt_tab_set_container (PromptTab          *self,
                          PromptIpcContainer *container)
{
  g_return_if_fail (PROMPT_IS_TAB (self));
  g_return_if_fail (!container || PROMPT_IPC_IS_CONTAINER (container));

  g_set_object (&self->container_at_creation, container);
}

gboolean
prompt_tab_poll_agent (PromptTab *self)
{
  gboolean has_foreground_process;
  g_autoptr(GUnixFDList) fd_list = NULL;
  g_autofree char *the_cmdline = NULL;
  g_autofree char *the_leader_kind = NULL;
  PromptProcessLeaderKind leader_kind;
  gboolean changed = FALSE;
  VtePty *pty;
  GPid the_pid;
  int handle;
  int pty_fd;

  g_assert (PROMPT_IS_TAB (self));

  if (self->process == NULL)
    {
      if (g_set_str (&self->command_line, NULL))
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COMMAND_LINE]);

      if (self->leader_kind != PROMPT_PROCESS_LEADER_KIND_UNKNOWN)
        {
          self->leader_kind = PROMPT_PROCESS_LEADER_KIND_UNKNOWN;
          g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PROCESS_LEADER_KIND]);
        }

      return FALSE;
    }

  pty = vte_terminal_get_pty (VTE_TERMINAL (self->terminal));
  pty_fd = vte_pty_get_fd (pty);
  fd_list = g_unix_fd_list_new ();
  handle = g_unix_fd_list_append (fd_list, pty_fd, NULL);

  if (!prompt_ipc_process_call_has_foreground_process_sync (self->process,
                                                            g_variant_new_handle (handle),
                                                            fd_list,
                                                            &has_foreground_process,
                                                            &the_pid,
                                                            &the_cmdline,
                                                            &the_leader_kind,
                                                            NULL, NULL, NULL))
    has_foreground_process = FALSE;

  if (self->pid != the_pid)
    {
      changed = TRUE;
      self->pid = the_pid;
    }

  if (self->has_foreground_process != has_foreground_process)
    {
      changed = TRUE;
      self->has_foreground_process = has_foreground_process;
    }

  if (g_strcmp0 (the_leader_kind, "superuser") == 0)
    leader_kind = PROMPT_PROCESS_LEADER_KIND_SUPERUSER;
  else if (g_strcmp0 (the_leader_kind, "container") == 0)
    leader_kind = PROMPT_PROCESS_LEADER_KIND_CONTAINER;
  else if (g_strcmp0 (the_leader_kind, "remote") == 0)
    leader_kind = PROMPT_PROCESS_LEADER_KIND_REMOTE;
  else
    leader_kind = PROMPT_PROCESS_LEADER_KIND_UNKNOWN;

  if (self->leader_kind != leader_kind)
    {
      changed = TRUE;
      self->leader_kind = leader_kind;

      if (!prompt_tab_is_active (self))
        prompt_tab_set_needs_attention (self, TRUE);

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PROCESS_LEADER_KIND]);
    }

  if (g_set_str (&self->command_line, the_cmdline))
    {
      g_autofree char *program_name = NULL;
      const char *space;

      changed = TRUE;

      if (the_cmdline != NULL && (space = strchr (the_cmdline, ' ')))
        program_name = g_strndup (the_cmdline, space - the_cmdline);

      g_set_str (&self->program_name, program_name);

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COMMAND_LINE]);
    }

  if (changed)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);

  return changed;
}

gboolean
prompt_tab_has_foreground_process (PromptTab  *self,
                                   GPid       *pid,
                                   char      **cmdline)
{
  g_return_val_if_fail (PROMPT_IS_TAB (self), FALSE);

  prompt_tab_poll_agent (self);

  if (pid != NULL)
    *pid = self->pid;

  if (cmdline != NULL)
    *cmdline = g_strdup (self->command_line);

  return self->has_foreground_process;
}

void
prompt_tab_set_command (PromptTab          *self,
                        const char * const *command)
{
  char **copy;

  g_return_if_fail (PROMPT_IS_TAB (self));

  if (command != NULL && command[0] == NULL)
    command = NULL;

  copy = g_strdupv ((char **)command);
  g_strfreev (self->command);
  self->command = copy;
}

void
prompt_tab_set_initial_title (PromptTab  *self,
                              const char *initial_title)
{
  g_return_if_fail (PROMPT_IS_TAB (self));

  g_set_str (&self->initial_title, initial_title);
}

const char *
prompt_tab_get_command_line (PromptTab *self)
{
  g_return_val_if_fail (PROMPT_IS_TAB (self), NULL);

  return self->command_line;
}

static void
prompt_tab_toast (PromptTab  *self,
                  int         timeout,
                  const char *title)
{
  GtkWidget *overlay = gtk_widget_get_ancestor (GTK_WIDGET (self), ADW_TYPE_TOAST_OVERLAY);
  AdwToast *toast;

  if (overlay == NULL)
    return;

  toast = g_object_new (ADW_TYPE_TOAST,
                        "title", title,
                        "timeout", timeout,
                        NULL);
  adw_toast_overlay_add_toast (ADW_TOAST_OVERLAY (overlay), toast);
}

static void
prompt_tab_open_uri_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  g_autoptr(PromptTab) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (XDP_IS_PORTAL (object));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (PROMPT_IS_TAB (self));

  if (!xdp_portal_open_uri_finish (XDP_PORTAL (object), result, &error))
    prompt_tab_toast (self, 3, _("Failed to open link"));
}

void
prompt_tab_open_uri (PromptTab  *self,
                     const char *uri)
{
  g_autofree char *translated = NULL;
  GtkWindow *window;
  XdpParent *parent;

  g_return_if_fail (PROMPT_IS_TAB (self));
  g_return_if_fail (uri != NULL);

  window = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));

  if (g_str_has_prefix (uri, "file://"))
    {
      g_autoptr(PromptIpcContainer) container = prompt_tab_dup_container (self);
      g_autoptr(GUri) guri = NULL;

      if (container == NULL)
        {
          g_autofree char *default_container = prompt_profile_dup_default_container (self->profile);
          container = prompt_application_lookup_container (PROMPT_APPLICATION_DEFAULT, default_container);
        }

      if (container != NULL)
        {
          if (prompt_ipc_container_call_translate_uri_sync (container, uri, &translated, NULL, NULL))
            uri = translated;
        }

      if (prompt_get_process_kind () == PROMPT_PROCESS_KIND_FLATPAK &&
          (guri = g_uri_parse (uri, 0, NULL)) &&
          !g_str_has_prefix (g_uri_get_path (guri), g_get_home_dir ()))
        {
          const char *path = g_uri_get_path (guri);
          g_autofree char *new_path = g_build_filename ("/var/run/host", path, NULL);
          g_autoptr(GUri) rewritten = NULL;

          rewritten = g_uri_build (0,
                                   "file",
                                   g_uri_get_userinfo (guri),
                                   g_uri_get_host (guri),
                                   g_uri_get_port (guri),
                                   new_path,
                                   g_uri_get_query (guri),
                                   g_uri_get_fragment (guri));

          g_clear_pointer (&translated, g_free);
          uri = translated = g_uri_to_string (rewritten);
        }
    }

  if (portal == NULL)
    portal = xdp_portal_new ();

  parent = xdp_parent_new_gtk (window);
  xdp_portal_open_uri (portal,
                       parent,
                       uri,
                       XDP_OPEN_URI_FLAG_NONE,
                       NULL,
                       prompt_tab_open_uri_cb,
                       g_object_ref (self));
  xdp_parent_free (parent);
}
