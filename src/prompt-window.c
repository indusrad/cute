/* prompt-window.c
 *
 * Copyright 2023 Christian Hergert
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
#include "prompt-close-dialog.h"
#include "prompt-find-bar.h"
#include "prompt-parking-lot.h"
#include "prompt-settings.h"
#include "prompt-tab-monitor.h"
#include "prompt-theme-selector.h"
#include "prompt-title-dialog.h"
#include "prompt-util.h"
#include "prompt-window.h"
#include "prompt-window-dressing.h"

struct _PromptWindow
{
  AdwApplicationWindow   parent_instance;

  PromptShortcuts       *shortcuts;
  PromptParkingLot      *parking_lot;

  GtkButton             *new_terminal_button;
  GtkMenuButton         *new_terminal_menu_button;
  GtkSeparator          *new_terminal_separator;
  PromptFindBar         *find_bar;
  GtkRevealer           *find_bar_revealer;
  AdwHeaderBar          *header_bar;
  GMenu                 *primary_menu;
  GtkMenuButton         *primary_menu_button;
  AdwTabBar             *tab_bar;
  GMenu                 *tab_menu;
  AdwTabOverview        *tab_overview;
  AdwTabView            *tab_view;
  GtkWidget             *zoom_label;

  GBindingGroup         *active_tab_bindings;
  GSignalGroup          *active_tab_signals;
  PromptWindowDressing  *dressing;
  GtkBox                *visual_bell;

  guint                  visual_bell_source;
  guint                  focus_active_tab_source;

  guint                  tab_overview_animating : 1;
};

G_DEFINE_FINAL_TYPE (PromptWindow, prompt_window, ADW_TYPE_APPLICATION_WINDOW)

enum {
  PROP_0,
  PROP_ACTIVE_TAB,
  PROP_SHORTCUTS,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
prompt_window_save_size (PromptWindow *self)
{
  PromptTab *active_tab;

  g_assert (PROMPT_IS_WINDOW (self));

  if ((active_tab = prompt_window_get_active_tab (self)))
    {
      PromptSettings *settings = prompt_application_get_settings (PROMPT_APPLICATION_DEFAULT);
      PromptTerminal *terminal = prompt_tab_get_terminal (active_tab);
      guint columns = vte_terminal_get_column_count (VTE_TERMINAL (terminal));
      guint rows = vte_terminal_get_row_count (VTE_TERMINAL (terminal));

      prompt_settings_set_window_size (settings, columns, rows);
    }
}

static void
prompt_window_close_page_dialog_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  g_autoptr(PromptTab) tab = user_data;
  g_autoptr(GError) error = NULL;
  PromptWindow *self;
  AdwTabPage *page;

  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (PROMPT_IS_TAB (tab));

  self = PROMPT_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (tab), PROMPT_TYPE_WINDOW));
  page = adw_tab_view_get_page (self->tab_view, GTK_WIDGET (tab));

  if (!_prompt_close_dialog_run_finish (result, &error))
    {
      adw_tab_view_close_page_finish (self->tab_view, page, FALSE);
      return;
    }

  prompt_parking_lot_push (self->parking_lot, tab);
  adw_tab_view_close_page_finish (self->tab_view, page, TRUE);
}

static gboolean
prompt_window_close_page_cb (PromptWindow *self,
                             AdwTabPage   *tab_page,
                             AdwTabView   *tab_view)
{
  g_autoptr(GPtrArray) tabs = NULL;
  PromptTab *tab;

  g_assert (PROMPT_IS_WINDOW (self));
  g_assert (ADW_IS_TAB_PAGE (tab_page));
  g_assert (ADW_IS_TAB_VIEW (tab_view));

  prompt_window_save_size (self);

  tab = PROMPT_TAB (adw_tab_page_get_child (tab_page));

  if (!prompt_tab_is_running (tab, NULL))
    {
      prompt_parking_lot_push (self->parking_lot, tab);
      return GDK_EVENT_PROPAGATE;
    }

  tabs = g_ptr_array_new_with_free_func (g_object_unref);
  g_ptr_array_add (tabs, g_object_ref (tab));

  _prompt_close_dialog_run_async (GTK_WINDOW (self),
                                   tabs,
                                   NULL,
                                   prompt_window_close_page_dialog_cb,
                                   g_object_ref (tab));

  return GDK_EVENT_STOP;
}

static gboolean
prompt_window_focus_active_tab_cb (gpointer data)
{
  PromptWindow *self = data;
  PromptTab *active_tab;

  g_assert (PROMPT_IS_WINDOW (self));

  self->focus_active_tab_source = 0;
  self->tab_overview_animating = FALSE;

  if ((active_tab = prompt_window_get_active_tab (self)))
    {
      gtk_widget_grab_focus (GTK_WIDGET (active_tab));
      gtk_widget_queue_resize (GTK_WIDGET (active_tab));
    }

  return G_SOURCE_REMOVE;
}

static void
prompt_window_tab_overview_notify_open_cb (PromptWindow   *self,
                                           GParamSpec     *pspec,
                                           AdwTabOverview *tab_overview)
{
  g_assert (PROMPT_IS_WINDOW (self));
  g_assert (ADW_IS_TAB_OVERVIEW (tab_overview));

  /* For some reason when we get here the selected page is not
   * getting focused. So work around libadwaita by deferring the
   * focus to an idle so that we can ensure we're working with
   * the appropriate focus tab.
   *
   * See https://gitlab.gnome.org/GNOME/libadwaita/-/issues/670
   */

  g_clear_handle_id (&self->focus_active_tab_source, g_source_remove);

  if (!adw_tab_overview_get_open (tab_overview))
    {
      PromptTab *active_tab;
      GtkSettings *settings = gtk_settings_get_default ();
      gboolean gtk_enable_animations = TRUE;
      guint delay_msec = 425; /* Sync with libadwaita */

      g_object_get (settings,
                    "gtk-enable-animations", &gtk_enable_animations,
                    NULL);

      if (!gtk_enable_animations)
        delay_msec = 10;

      self->focus_active_tab_source = g_timeout_add_full (G_PRIORITY_LOW,
                                                          delay_msec,
                                                          prompt_window_focus_active_tab_cb,
                                                          self, NULL);

      if ((active_tab = prompt_window_get_active_tab (self)))
        gtk_widget_grab_focus (GTK_WIDGET (active_tab));
    }

  self->tab_overview_animating = TRUE;
}

static void
prompt_window_setup_menu_cb (PromptWindow *self,
                             AdwTabPage   *page,
                             AdwTabView   *view)
{
  g_assert (PROMPT_IS_WINDOW (self));
  g_assert (!page || ADW_IS_TAB_PAGE (page));
  g_assert (ADW_IS_TAB_VIEW (view));

  if (page != NULL)
    adw_tab_view_set_selected_page (view, page);
}

static AdwTabView *
prompt_window_create_window_cb (PromptWindow *self,
                                AdwTabView   *tab_view)
{
  PromptWindow *other;

  g_assert (PROMPT_IS_WINDOW (self));

  other = g_object_new (PROMPT_TYPE_WINDOW, NULL);
  gtk_window_present (GTK_WINDOW (other));
  return other->tab_view;
}

static void
prompt_window_page_attached_cb (PromptWindow *self,
                                AdwTabPage   *page,
                                int           position,
                                AdwTabView   *tab_view)
{
  GtkWidget *child;

  g_assert (PROMPT_IS_WINDOW (self));
  g_assert (ADW_IS_TAB_PAGE (page));
  g_assert (ADW_IS_TAB_VIEW (tab_view));

  child = adw_tab_page_get_child (page);

  g_object_bind_property (child, "title", page, "title", G_BINDING_SYNC_CREATE);
  g_object_bind_property (child, "icon", page, "icon", G_BINDING_SYNC_CREATE);

  gtk_widget_set_visible (GTK_WIDGET (self->tab_bar),
                          adw_tab_view_get_n_pages (tab_view) > 1);
}

static void
prompt_window_page_detached_cb (PromptWindow *self,
                                AdwTabPage   *page,
                                int           position,
                                AdwTabView   *tab_view)
{
  guint n_pages;

  g_assert (PROMPT_IS_WINDOW (self));
  g_assert (ADW_IS_TAB_PAGE (page));
  g_assert (ADW_IS_TAB_VIEW (tab_view));

  n_pages = adw_tab_view_get_n_pages (tab_view);

  if (n_pages == 0)
    gtk_window_destroy (GTK_WINDOW (self));
  else
    gtk_widget_set_visible (GTK_WIDGET (self->tab_bar), n_pages > 1);
}

static void
prompt_window_notify_selected_page_cb (PromptWindow *self,
                                       GParamSpec   *pspec,
                                       AdwTabView   *tab_view)
{
  g_autoptr(GPropertyAction) read_only = NULL;
  PromptTerminal *terminal = NULL;
  AdwTabPage *page;
  PromptTab *tab = NULL;
  gboolean has_page = FALSE;

  g_assert (PROMPT_IS_WINDOW (self));
  g_assert (ADW_IS_TAB_VIEW (tab_view));

  if ((page = adw_tab_view_get_selected_page (self->tab_view)))
    {
      PromptProfile *profile;

      tab = PROMPT_TAB (adw_tab_page_get_child (page));
      profile = prompt_tab_get_profile (tab);

      has_page = TRUE;

      terminal = prompt_tab_get_terminal (tab);

      g_signal_group_set_target (self->active_tab_signals, tab);

      g_object_bind_property (profile, "palette",
                              self->dressing, "palette",
                              G_BINDING_SYNC_CREATE);
      g_object_bind_property (profile, "opacity",
                              self->dressing, "opacity",
                              G_BINDING_SYNC_CREATE);

      read_only = g_property_action_new ("tab.read-only", tab, "read-only");

      adw_tab_page_set_needs_attention (page, FALSE);

      gtk_widget_grab_focus (GTK_WIDGET (tab));
    }

  if (terminal == NULL)
    gtk_revealer_set_reveal_child (self->find_bar_revealer, FALSE);

  prompt_find_bar_set_terminal (self->find_bar, terminal);

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "win.zoom-in", has_page);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "win.zoom-out", has_page);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "win.zoom-one", has_page);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "win.search", has_page);

  g_action_map_remove_action (G_ACTION_MAP (self), "tab.read-only");
  if (read_only != NULL)
    g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (read_only));

  g_binding_group_set_source (self->active_tab_bindings, tab);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVE_TAB]);
}

static void
prompt_window_apply_current_settings (PromptWindow *self,
                                      PromptTab    *tab)
{
  PromptApplication *app;
  PromptTab *active_tab;
  PromptProfile *profile;

  g_assert (PROMPT_IS_WINDOW (self));
  g_assert (PROMPT_IS_TAB (tab));

  app = PROMPT_APPLICATION_DEFAULT;
  profile = prompt_tab_get_profile (tab);

  if ((active_tab = prompt_window_get_active_tab (self)))
    {
      PromptTerminal *terminal = prompt_tab_get_terminal (active_tab);
      const char *current_directory_uri = prompt_tab_get_current_directory_uri (active_tab);
      const char *current_container_name = vte_terminal_get_current_container_name (VTE_TERMINAL (terminal));
      const char *current_container_runtime = vte_terminal_get_current_container_runtime (VTE_TERMINAL (terminal));
      PromptZoomLevel zoom = prompt_tab_get_zoom (active_tab);
      g_autoptr(PromptIpcContainer) current_container = NULL;

      if (prompt_profile_get_preserve_container (profile))
        {
          if ((current_container = prompt_application_find_container_by_name (app,
                                                                              current_container_runtime,
                                                                              current_container_name)))
            prompt_tab_set_container (tab, current_container);
        }

      if (current_directory_uri != NULL)
        prompt_tab_set_previous_working_directory_uri (tab, current_directory_uri);

      prompt_tab_set_zoom (tab, zoom);
    }
}

static PromptProfile *
prompt_window_dup_profile_for_param (PromptWindow *self,
                                     const char   *profile_uuid)
{
  g_autoptr(PromptProfile) profile = NULL;
  PromptApplication *app;
  PromptProfile *active_profile;

  g_assert (PROMPT_IS_WINDOW (self));
  g_assert (profile_uuid != NULL);

  app = PROMPT_APPLICATION_DEFAULT;
  active_profile = prompt_window_get_active_profile (self);

  if (profile_uuid[0] == 0 && active_profile != NULL)
    profile = g_object_ref (active_profile);
  else if (profile_uuid[0] == 0 || g_str_equal (profile_uuid, "default"))
    profile = prompt_application_dup_default_profile (app);
  else
    profile = prompt_application_dup_profile (app, profile_uuid);

  return g_steal_pointer (&profile);
}

static AdwTabPage *
prompt_window_tab_overview_create_tab_cb (PromptWindow   *self,
                                          AdwTabOverview *tab_overview)
{
  g_autoptr(PromptProfile) profile = NULL;
  PromptTab *tab;

  g_assert (PROMPT_IS_WINDOW (self));
  g_assert (ADW_IS_TAB_OVERVIEW (tab_overview));

  profile = prompt_window_dup_profile_for_param (self, "default");
  tab = prompt_tab_new (profile);

  prompt_window_add_tab (self, tab);
  prompt_window_set_active_tab (self, tab);

  return adw_tab_view_get_selected_page (self->tab_view);
}

static void
prompt_window_new_tab_action (GtkWidget  *widget,
                              const char *action_name,
                              GVariant   *param)
{
  PromptWindow *self = (PromptWindow *)widget;
  g_autoptr(PromptProfile) profile = NULL;
  const char *profile_uuid = "";
  const char *container_id = "";
  PromptTab *tab;

  g_assert (PROMPT_IS_WINDOW (self));
  g_assert (param != NULL);
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE ("(ss)")));

  g_variant_get (param, "(&s&s)", &profile_uuid, &container_id);
  profile = prompt_window_dup_profile_for_param (self, profile_uuid);

  tab = prompt_tab_new (profile);
  prompt_window_apply_current_settings (self, tab);

  if (!prompt_str_empty0 (container_id))
    {
      g_autoptr(PromptIpcContainer) container = NULL;

      if ((container = prompt_application_lookup_container (PROMPT_APPLICATION_DEFAULT, container_id)))
        prompt_tab_set_container (tab, container);
    }

  prompt_window_add_tab (self, tab);
  prompt_window_set_active_tab (self, tab);
}

static void
prompt_window_new_window_action (GtkWidget  *widget,
                                 const char *action_name,
                                 GVariant   *param)
{
  PromptWindow *self = (PromptWindow *)widget;
  g_autoptr(PromptProfile) profile = NULL;
  PromptWindow *window;
  const char *profile_uuid = "";
  const char *container_id = "";
  PromptTab *tab;

  g_assert (PROMPT_IS_WINDOW (self));
  g_assert (param != NULL);
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE ("(ss)")));

  g_variant_get (param, "(&s&s)", &profile_uuid, &container_id);
  profile = prompt_window_dup_profile_for_param (self, profile_uuid);

  tab = prompt_tab_new (profile);
  prompt_window_apply_current_settings (self, tab);

  if (!prompt_str_empty0 (container_id))
    {
      g_autoptr(PromptIpcContainer) container = NULL;

      if ((container = prompt_application_lookup_container (PROMPT_APPLICATION_DEFAULT, container_id)))
        prompt_tab_set_container (tab, container);
    }

  window = g_object_new (PROMPT_TYPE_WINDOW, NULL);
  prompt_window_add_tab (window, tab);

  gtk_window_present (GTK_WINDOW (window));
}

static void
prompt_window_new_terminal_action (GtkWidget  *widget,
                                   const char *action_name,
                                   GVariant   *param)
{
  g_assert (PROMPT_IS_WINDOW (widget));
  g_assert (param != NULL);
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE ("(ss)")));

  if (prompt_application_control_is_pressed (PROMPT_APPLICATION_DEFAULT))
    prompt_window_new_window_action (widget, NULL, param);
  else
    prompt_window_new_tab_action (widget, NULL, param);
}

static void
prompt_window_tab_overview_action (GtkWidget  *widget,
                                   const char *action_name,
                                   GVariant   *param)
{
  PromptWindow *self = (PromptWindow *)widget;

  g_assert (PROMPT_IS_WINDOW (self));

  adw_tab_overview_set_open (self->tab_overview, TRUE);
}

static void
prompt_window_zoom_in_action (GtkWidget  *widget,
                              const char *action_name,
                              GVariant   *param)
{
  PromptWindow *self = (PromptWindow *)widget;
  PromptTab *active_tab;

  g_assert (PROMPT_IS_WINDOW (self));

  if ((active_tab = prompt_window_get_active_tab (self)))
    {
      prompt_tab_zoom_in (active_tab);
      gtk_window_set_default_size (GTK_WINDOW (self), -1, -1);
    }
}

static void
prompt_window_zoom_out_action (GtkWidget  *widget,
                               const char *action_name,
                               GVariant   *param)
{
  PromptWindow *self = (PromptWindow *)widget;
  PromptTab *active_tab;

  g_assert (PROMPT_IS_WINDOW (self));

  if ((active_tab = prompt_window_get_active_tab (self)))
    {
      prompt_tab_zoom_out (active_tab);
      gtk_window_set_default_size (GTK_WINDOW (self), -1, -1);
    }
}

static void
prompt_window_zoom_one_action (GtkWidget  *widget,
                               const char *action_name,
                               GVariant   *param)
{
  PromptWindow *self = (PromptWindow *)widget;
  PromptTab *active_tab;

  g_assert (PROMPT_IS_WINDOW (self));

  if ((active_tab = prompt_window_get_active_tab (self)))
    {
      prompt_tab_set_zoom (active_tab, PROMPT_ZOOM_LEVEL_DEFAULT);
      gtk_window_set_default_size (GTK_WINDOW (self), -1, -1);
    }
}

static void
prompt_window_active_tab_bell_cb (PromptWindow *self,
                                  PromptTab    *tab)
{
  g_assert (PROMPT_IS_WINDOW (self));
  g_assert (PROMPT_IS_TAB (tab));

  prompt_window_visual_bell (self);
}

static void
prompt_window_close_action (GtkWidget  *widget,
                            const char *action_name,
                            GVariant   *param)
{
  PromptWindow *self = (PromptWindow *)widget;
  PromptTab *tab;
  AdwTabPage *tab_page;

  g_assert (PROMPT_IS_WINDOW (self));

  if (!(tab = prompt_window_get_active_tab (self)))
    return;

  tab_page = adw_tab_view_get_page (self->tab_view, GTK_WIDGET (tab));
  adw_tab_view_close_page (self->tab_view, tab_page);
}

static void
prompt_window_close_others_action (GtkWidget  *widget,
                                   const char *action_name,
                                   GVariant   *param)
{
  PromptWindow *self = (PromptWindow *)widget;
  PromptTab *tab;
  AdwTabPage *tab_page;

  g_assert (PROMPT_IS_WINDOW (self));

  if (!(tab = prompt_window_get_active_tab (self)))
    return;

  tab_page = adw_tab_view_get_page (self->tab_view, GTK_WIDGET (tab));
  adw_tab_view_close_other_pages (self->tab_view, tab_page);
}

static void
prompt_window_detach_action (GtkWidget  *widget,
                             const char *action_name,
                             GVariant   *param)
{
  PromptWindow *self = (PromptWindow *)widget;
  PromptWindow *new_window;
  PromptTab *tab;
  AdwTabPage *tab_page;

  g_assert (PROMPT_IS_WINDOW (self));

  if (!(tab = prompt_window_get_active_tab (self)))
    return;

  tab_page = adw_tab_view_get_page (self->tab_view, GTK_WIDGET (tab));

  new_window = g_object_new (PROMPT_TYPE_WINDOW, NULL);
  adw_tab_view_transfer_page (self->tab_view, tab_page, new_window->tab_view, 0);

  gtk_window_present (GTK_WINDOW (new_window));
}

static void
prompt_window_page_previous_action (GtkWidget  *widget,
                                    const char *action_name,
                                    GVariant   *param)
{
  adw_tab_view_select_previous_page (PROMPT_WINDOW (widget)->tab_view);
}

static void
prompt_window_page_next_action (GtkWidget  *widget,
                                const char *action_name,
                                GVariant   *param)
{
  adw_tab_view_select_next_page (PROMPT_WINDOW (widget)->tab_view);
}

static void
prompt_window_tab_focus_action (GtkWidget  *widget,
                                const char *action_name,
                                GVariant   *param)
{
  PromptWindow *self = (PromptWindow *)widget;
  int position;

  g_assert (PROMPT_IS_WINDOW (self));
  g_assert (param != NULL);
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_INT32));

  position = g_variant_get_int32 (param);

  if (position > 0 && position <= adw_tab_view_get_n_pages (self->tab_view))
    {
      AdwTabPage *page = adw_tab_view_get_nth_page (self->tab_view, position - 1);

      adw_tab_view_set_selected_page (self->tab_view, page);
    }
}

static void
prompt_window_tab_reset_action (GtkWidget  *widget,
                                const char *action_name,
                                GVariant   *param)
{
  PromptWindow *self = (PromptWindow *)widget;
  PromptTerminal *terminal;
  PromptTab *tab;
  gboolean clear;

  g_assert (PROMPT_IS_WINDOW (self));
  g_assert (param != NULL);
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_BOOLEAN));

  clear = g_variant_get_boolean (param);

  if (!(tab = prompt_window_get_active_tab (self)))
    return;

  terminal = prompt_tab_get_terminal (tab);
  vte_terminal_reset (VTE_TERMINAL (terminal), TRUE, clear);
}

static void
prompt_window_move_left_action (GtkWidget  *widget,
                                const char *action_name,
                                GVariant   *param)
{
  PromptWindow *self = (PromptWindow *)widget;
  PromptTab *tab;
  AdwTabPage *tab_page;

  g_assert (PROMPT_IS_WINDOW (self));

  if (!(tab = prompt_window_get_active_tab (self)))
    return;

  tab_page = adw_tab_view_get_page (self->tab_view, GTK_WIDGET (tab));
  adw_tab_view_reorder_backward (self->tab_view, tab_page);
  prompt_tab_raise (tab);
}

static void
prompt_window_move_right_action (GtkWidget  *widget,
                                 const char *action_name,
                                 GVariant   *param)
{
  PromptWindow *self = (PromptWindow *)widget;
  PromptTab *tab;
  AdwTabPage *tab_page;

  g_assert (PROMPT_IS_WINDOW (self));

  if (!(tab = prompt_window_get_active_tab (self)))
    return;

  tab_page = adw_tab_view_get_page (self->tab_view, GTK_WIDGET (tab));
  adw_tab_view_reorder_forward (self->tab_view, tab_page);
  prompt_tab_raise (tab);
}

static void
prompt_window_fullscreen_action (GtkWidget  *widget,
                                 const char *action_name,
                                 GVariant   *param)
{
  gtk_window_fullscreen (GTK_WINDOW (widget));
}

static void
prompt_window_unfullscreen_action (GtkWidget  *widget,
                                   const char *action_name,
                                   GVariant   *param)
{
  gtk_window_unfullscreen (GTK_WINDOW (widget));
}

static void
prompt_window_toggle_fullscreen (GtkWidget  *widget,
                                 const char *action_name,
                                 GVariant   *param)
{
  if (gtk_window_is_fullscreen (GTK_WINDOW (widget)))
    gtk_window_unfullscreen (GTK_WINDOW (widget));
  else
    gtk_window_fullscreen (GTK_WINDOW (widget));
}

static void
prompt_window_toplevel_state_changed_cb (PromptWindow *self,
                                         GParamSpec   *pspec,
                                         GdkToplevel  *toplevel)
{
  GdkToplevelState state;
  gboolean is_fullscreen;

  g_assert (PROMPT_IS_WINDOW (self));
  g_assert (GDK_IS_TOPLEVEL (toplevel));

  state = gdk_toplevel_get_state (toplevel);

  is_fullscreen = !!(state & GDK_TOPLEVEL_STATE_FULLSCREEN);

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "win.fullscreen", !is_fullscreen);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "win.unfullscreen", is_fullscreen);
}

static void
prompt_window_realize (GtkWidget *widget)
{
  PromptWindow *self = (PromptWindow *)widget;
  GdkToplevel *toplevel;

  g_assert (PROMPT_IS_WINDOW (self));

  GTK_WIDGET_CLASS (prompt_window_parent_class)->realize (widget);

  toplevel = GDK_TOPLEVEL (gtk_native_get_surface (GTK_NATIVE (self)));

  g_signal_connect_object (toplevel,
                           "notify::state",
                           G_CALLBACK (prompt_window_toplevel_state_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
prompt_window_shortcuts_notify_cb (PromptWindow    *self,
                                   GParamSpec      *pspec,
                                   PromptShortcuts *shortcuts)
{
  g_assert (PROMPT_IS_WINDOW (self));
  g_assert (PROMPT_IS_SHORTCUTS (shortcuts));

  prompt_shortcuts_update_menu (shortcuts, self->primary_menu);
  prompt_shortcuts_update_menu (shortcuts, self->tab_menu);
}

static void
prompt_window_set_title_action (GtkWidget  *widget,
                                const char *action_name,
                                GVariant   *param)
{
  PromptWindow *self = (PromptWindow *)widget;
  AdwMessageDialog *dialog;
  PromptTab *active_tab;

  g_assert (PROMPT_IS_WINDOW (self));

  if (!(active_tab = prompt_window_get_active_tab (self)))
    return;

  dialog = g_object_new (PROMPT_TYPE_TITLE_DIALOG,
                         "modal", TRUE,
                         "resizable", FALSE,
                         "tab", active_tab,
                         "title", _("Set Title"),
                         "transient-for", self,
                         NULL);

  gtk_window_present (GTK_WINDOW (dialog));
}

static void
prompt_window_search_action (GtkWidget  *widget,
                             const char *action_name,
                             GVariant   *param)
{
  PromptWindow *self = (PromptWindow *)widget;

  g_assert (PROMPT_IS_WINDOW (self));

  gtk_revealer_set_reveal_child (self->find_bar_revealer, TRUE);
  gtk_widget_grab_focus (GTK_WIDGET (self->find_bar));
}

static void
prompt_window_undo_close_tab_action (GtkWidget  *widget,
                                     const char *action_name,
                                     GVariant   *param)
{
  PromptWindow *self = (PromptWindow *)widget;
  g_autoptr(PromptTab) tab = NULL;

  g_assert (PROMPT_IS_WINDOW (self));

  if ((tab = prompt_parking_lot_pop (self->parking_lot)))
    {
      if (!prompt_tab_is_running (tab, NULL))
        prompt_tab_show_banner (tab);
      prompt_window_add_tab (self, tab);
      prompt_window_set_active_tab (self, tab);
      gtk_widget_grab_focus (GTK_WIDGET (tab));
    }
}

static void
prompt_window_close_request_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  g_autoptr(PromptWindow) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (PROMPT_IS_WINDOW (self));

  if (!_prompt_close_dialog_run_finish (result, &error))
    return;

  gtk_window_destroy (GTK_WINDOW (self));
}

static gboolean
prompt_window_close_request (GtkWindow *window)
{
  PromptWindow *self = (PromptWindow *)window;
  g_autoptr(GPtrArray) tabs = NULL;
  guint n_pages;

  g_assert (PROMPT_IS_WINDOW (self));

  prompt_window_save_size (self);

  tabs = g_ptr_array_new_with_free_func (g_object_unref);
  n_pages = adw_tab_view_get_n_pages (self->tab_view);

  for (guint i = 0; i < n_pages; i++)
    {
      AdwTabPage *page = adw_tab_view_get_nth_page (self->tab_view, i);
      PromptTab *tab = PROMPT_TAB (adw_tab_page_get_child (page));

      if (prompt_tab_is_running (tab, NULL))
        g_ptr_array_add (tabs, g_object_ref (tab));
    }

  if (tabs->len == 0)
    return GDK_EVENT_PROPAGATE;

  _prompt_close_dialog_run_async (GTK_WINDOW (self),
                                   tabs,
                                   NULL,
                                   prompt_window_close_request_cb,
                                   g_object_ref (self));

  return GDK_EVENT_STOP;
}

static void
prompt_window_notify_process_leader_kind_cb (PromptWindow *self,
                                             GParamSpec   *pspec,
                                             PromptTab    *tab)
{
  PromptProcessLeaderKind kind;

  g_assert (PROMPT_IS_WINDOW (self));
  g_assert (PROMPT_IS_TAB (tab));

  g_object_get (tab,
                "process-leader-kind", &kind,
                NULL);

  gtk_widget_remove_css_class (GTK_WIDGET (self), "container");
  gtk_widget_remove_css_class (GTK_WIDGET (self), "remote");
  gtk_widget_remove_css_class (GTK_WIDGET (self), "superuser");

  if (kind == PROMPT_PROCESS_LEADER_KIND_SUPERUSER)
    gtk_widget_add_css_class (GTK_WIDGET (self), "superuser");
  else if (kind == PROMPT_PROCESS_LEADER_KIND_REMOTE)
    gtk_widget_add_css_class (GTK_WIDGET (self), "remote");
  else if (kind == PROMPT_PROCESS_LEADER_KIND_CONTAINER)
    gtk_widget_add_css_class (GTK_WIDGET (self), "container");
}

static void
prompt_window_active_tab_bind_cb (PromptWindow *self,
                                  PromptTab    *tab,
                                  GSignalGroup *signals)
{
  g_assert (PROMPT_IS_WINDOW (self));
  g_assert (PROMPT_IS_TAB (tab));
  g_assert (G_IS_SIGNAL_GROUP (signals));

  prompt_window_notify_process_leader_kind_cb (self, NULL, tab);
}

static void
prompt_window_add_zoom_controls (PromptWindow *self)
{
  GtkPopover *popover;
  GtkWidget *zoom_box;
  GtkWidget *zoom_out;
  GtkWidget *zoom_in;

  g_assert (PROMPT_IS_WINDOW (self));

  popover = gtk_menu_button_get_popover (self->primary_menu_button);

  /* Add zoom controls */
  zoom_box = g_object_new (GTK_TYPE_BOX,
                           "spacing", 12,
                           "margin-start", 18,
                           "margin-end", 18,
                           NULL);
  zoom_in = g_object_new (GTK_TYPE_BUTTON,
                          "action-name", "win.zoom-in",
                          "tooltip-text", _("Zoom In"),
                          "child", g_object_new (GTK_TYPE_IMAGE,
                                                 "icon-name", "zoom-in-symbolic",
                                                 "pixel-size", 16,
                                                 NULL),
                          NULL);
  gtk_widget_add_css_class (zoom_in, "circular");
  gtk_widget_add_css_class (zoom_in, "flat");
  gtk_widget_set_tooltip_text (zoom_in, _("Zoom In"));
  gtk_accessible_update_property (GTK_ACCESSIBLE (zoom_in),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL,
                                  _("Zoom in"), -1);
  zoom_out = g_object_new (GTK_TYPE_BUTTON,
                           "action-name", "win.zoom-out",
                           "tooltip-text", _("Zoom Out"),
                           "child", g_object_new (GTK_TYPE_IMAGE,
                                                  "icon-name", "zoom-out-symbolic",
                                                  "pixel-size", 16,
                                                  NULL),
                           NULL);
  gtk_widget_add_css_class (zoom_out, "circular");
  gtk_widget_add_css_class (zoom_out, "flat");
  gtk_widget_set_tooltip_text (zoom_out, _("Zoom Out"));
  gtk_accessible_update_property (GTK_ACCESSIBLE (zoom_out),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL,
                                  _("Zoom out"), -1);
  self->zoom_label = g_object_new (GTK_TYPE_BUTTON,
                                   "css-classes", (const char * const[]) {"flat", "pill", NULL},
                                   "action-name", "win.zoom-one",
                                   "hexpand", TRUE,
                                   "tooltip-text", _("Reset Zoom"),
                                   "label", "100%",
                                   NULL);
  g_binding_group_bind (self->active_tab_bindings, "zoom-label", self->zoom_label, "label", 0);
  gtk_box_append (GTK_BOX (zoom_box), zoom_out);
  gtk_box_append (GTK_BOX (zoom_box), self->zoom_label);
  gtk_box_append (GTK_BOX (zoom_box), zoom_in);
  gtk_popover_menu_add_child (GTK_POPOVER_MENU (popover), zoom_box, "zoom");
}

static void
prompt_window_add_theme_controls (PromptWindow *self)
{
  g_autoptr(GPropertyAction) interface_style = NULL;
  PromptSettings *settings;
  GtkPopover *popover;
  GtkWidget *selector;

  g_assert (PROMPT_IS_WINDOW (self));

  settings = prompt_application_get_settings (PROMPT_APPLICATION_DEFAULT);
  interface_style = g_property_action_new ("interface-style", settings, "interface-style");
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (interface_style));

  popover = gtk_menu_button_get_popover (self->primary_menu_button);
  selector = g_object_new (PROMPT_TYPE_THEME_SELECTOR,
                           "action-name", "win.interface-style",
                           NULL);
  gtk_popover_menu_add_child (GTK_POPOVER_MENU (popover), selector, "interface-style");
}

static void
prompt_window_menu_items_changed_cb (PromptWindow *self,
                                     guint         position,
                                     guint         removed,
                                     guint         added,
                                     GMenuModel   *model)
{
  gboolean visible = FALSE;
  guint n_items;

  g_assert (PROMPT_IS_WINDOW (self));
  g_assert (!model || G_IS_MENU_MODEL (model));

  model = gtk_menu_button_get_menu_model (self->new_terminal_menu_button);
  n_items = g_menu_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GMenuModel) child = g_menu_model_get_item_link (model, i, G_MENU_LINK_SECTION);

      if (child && g_menu_model_get_n_items (child))
        {
          visible = TRUE;
          break;
        }
    }

  gtk_widget_set_visible (GTK_WIDGET (self->new_terminal_separator), visible);
  gtk_widget_set_visible (GTK_WIDGET (self->new_terminal_menu_button), visible);
}

static void
prompt_window_constructed (GObject *object)
{
  PromptWindow *self = (PromptWindow *)object;
  PromptApplication *app = PROMPT_APPLICATION_DEFAULT;
  g_autoptr(GMenuModel) profile_menu = NULL;
  g_autoptr(GMenuModel) container_menu = NULL;
  g_autoptr(GMenu) menu = NULL;

  G_OBJECT_CLASS (prompt_window_parent_class)->constructed (object);

  self->dressing = prompt_window_dressing_new (self);

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "win.unfullscreen", FALSE);

  prompt_window_add_theme_controls (self);
  prompt_window_add_zoom_controls (self);

  menu = g_menu_new ();

  profile_menu = prompt_application_dup_profile_menu (app);
  g_menu_append_section (menu, _("Profiles"), profile_menu);
  g_signal_connect_object (profile_menu,
                           "items-changed",
                           G_CALLBACK (prompt_window_menu_items_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  container_menu = prompt_application_dup_container_menu (app);
  g_menu_append_section (menu, _("Containers"), container_menu);
  g_signal_connect_object (container_menu,
                           "items-changed",
                           G_CALLBACK (prompt_window_menu_items_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_menu_button_set_menu_model (self->new_terminal_menu_button,
                                  G_MENU_MODEL (menu));

  prompt_window_menu_items_changed_cb (self, 0, 0, 0, NULL);
}

static void
prompt_window_dispose (GObject *object)
{
  PromptWindow *self = (PromptWindow *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), PROMPT_TYPE_WINDOW);

  g_signal_group_set_target (self->active_tab_signals, NULL);
  g_binding_group_set_source (self->active_tab_bindings, NULL);
  g_clear_handle_id (&self->focus_active_tab_source, g_source_remove);
  g_clear_object (&self->parking_lot);

  G_OBJECT_CLASS (prompt_window_parent_class)->dispose (object);
}

static void
prompt_window_finalize (GObject *object)
{
  PromptWindow *self = (PromptWindow *)object;

  g_clear_object (&self->active_tab_bindings);
  g_clear_object (&self->active_tab_signals);

  G_OBJECT_CLASS (prompt_window_parent_class)->finalize (object);
}

static void
prompt_window_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  PromptWindow *self = PROMPT_WINDOW (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_TAB:
      g_value_set_object (value, prompt_window_get_active_tab (self));
      break;

    case PROP_SHORTCUTS:
      g_value_set_object (value, self->shortcuts);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_window_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  PromptWindow *self = PROMPT_WINDOW (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_TAB:
      prompt_window_set_active_tab (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_window_class_init (PromptWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkWindowClass *window_class = GTK_WINDOW_CLASS (klass);

  object_class->constructed = prompt_window_constructed;
  object_class->dispose = prompt_window_dispose;
  object_class->finalize = prompt_window_finalize;
  object_class->get_property = prompt_window_get_property;
  object_class->set_property = prompt_window_set_property;

  widget_class->realize = prompt_window_realize;

  window_class->close_request = prompt_window_close_request;

  properties[PROP_ACTIVE_TAB] =
    g_param_spec_object ("active-tab", NULL, NULL,
                         PROMPT_TYPE_TAB,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_SHORTCUTS] =
    g_param_spec_object ("shortcuts", NULL, NULL,
                         PROMPT_TYPE_SHORTCUTS,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Prompt/prompt-window.ui");

  gtk_widget_class_bind_template_child (widget_class, PromptWindow, find_bar);
  gtk_widget_class_bind_template_child (widget_class, PromptWindow, find_bar_revealer);
  gtk_widget_class_bind_template_child (widget_class, PromptWindow, header_bar);
  gtk_widget_class_bind_template_child (widget_class, PromptWindow, primary_menu);
  gtk_widget_class_bind_template_child (widget_class, PromptWindow, primary_menu_button);
  gtk_widget_class_bind_template_child (widget_class, PromptWindow, new_terminal_button);
  gtk_widget_class_bind_template_child (widget_class, PromptWindow, new_terminal_menu_button);
  gtk_widget_class_bind_template_child (widget_class, PromptWindow, new_terminal_separator);
  gtk_widget_class_bind_template_child (widget_class, PromptWindow, tab_bar);
  gtk_widget_class_bind_template_child (widget_class, PromptWindow, tab_menu);
  gtk_widget_class_bind_template_child (widget_class, PromptWindow, tab_overview);
  gtk_widget_class_bind_template_child (widget_class, PromptWindow, tab_view);
  gtk_widget_class_bind_template_child (widget_class, PromptWindow, visual_bell);

  gtk_widget_class_bind_template_callback (widget_class, prompt_window_page_attached_cb);
  gtk_widget_class_bind_template_callback (widget_class, prompt_window_page_detached_cb);
  gtk_widget_class_bind_template_callback (widget_class, prompt_window_notify_selected_page_cb);
  gtk_widget_class_bind_template_callback (widget_class, prompt_window_create_window_cb);
  gtk_widget_class_bind_template_callback (widget_class, prompt_window_close_page_cb);
  gtk_widget_class_bind_template_callback (widget_class, prompt_window_setup_menu_cb);
  gtk_widget_class_bind_template_callback (widget_class, prompt_window_tab_overview_notify_open_cb);
  gtk_widget_class_bind_template_callback (widget_class, prompt_window_tab_overview_create_tab_cb);

  gtk_widget_class_install_action (widget_class, "win.new-tab", "(ss)", prompt_window_new_tab_action);
  gtk_widget_class_install_action (widget_class, "win.new-window", "(ss)", prompt_window_new_window_action);
  gtk_widget_class_install_action (widget_class, "win.new-terminal", "(ss)", prompt_window_new_terminal_action);
  gtk_widget_class_install_action (widget_class, "win.fullscreen", NULL, prompt_window_fullscreen_action);
  gtk_widget_class_install_action (widget_class, "win.unfullscreen", NULL, prompt_window_unfullscreen_action);
  gtk_widget_class_install_action (widget_class, "win.toggle-fullscreen", NULL, prompt_window_toggle_fullscreen);
  gtk_widget_class_install_action (widget_class, "win.tab-overview", NULL, prompt_window_tab_overview_action);
  gtk_widget_class_install_action (widget_class, "win.zoom-in", NULL, prompt_window_zoom_in_action);
  gtk_widget_class_install_action (widget_class, "win.zoom-out", NULL, prompt_window_zoom_out_action);
  gtk_widget_class_install_action (widget_class, "win.zoom-one", NULL, prompt_window_zoom_one_action);
  gtk_widget_class_install_action (widget_class, "page.move-left", NULL, prompt_window_move_left_action);
  gtk_widget_class_install_action (widget_class, "page.move-right", NULL, prompt_window_move_right_action);
  gtk_widget_class_install_action (widget_class, "page.close", NULL, prompt_window_close_action);
  gtk_widget_class_install_action (widget_class, "page.close-others", NULL, prompt_window_close_others_action);
  gtk_widget_class_install_action (widget_class, "page.detach", NULL, prompt_window_detach_action);
  gtk_widget_class_install_action (widget_class, "tab.reset", "b", prompt_window_tab_reset_action);
  gtk_widget_class_install_action (widget_class, "tab.focus", "i", prompt_window_tab_focus_action);
  gtk_widget_class_install_action (widget_class, "page.next", NULL, prompt_window_page_next_action);
  gtk_widget_class_install_action (widget_class, "page.previous", NULL, prompt_window_page_previous_action);
  gtk_widget_class_install_action (widget_class, "win.set-title", NULL, prompt_window_set_title_action);
  gtk_widget_class_install_action (widget_class, "win.search", NULL, prompt_window_search_action);
  gtk_widget_class_install_action (widget_class, "win.undo-close-tab", NULL, prompt_window_undo_close_tab_action);

  g_type_ensure (PROMPT_TYPE_FIND_BAR);
}

static void
prompt_window_init (PromptWindow *self)
{
  self->active_tab_bindings = g_binding_group_new ();

  self->active_tab_signals = g_signal_group_new (PROMPT_TYPE_TAB);
  g_signal_connect_object (self->active_tab_signals,
                           "bind",
                           G_CALLBACK (prompt_window_active_tab_bind_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->active_tab_signals,
                                 "bell",
                                 G_CALLBACK (prompt_window_active_tab_bell_cb),
                                 self,
                                 G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->active_tab_signals,
                                 "notify::process-leader-kind",
                                 G_CALLBACK (prompt_window_notify_process_leader_kind_cb),
                                 self,
                                 G_CONNECT_SWAPPED);

  self->parking_lot = prompt_parking_lot_new ();

  self->shortcuts = g_object_ref (prompt_application_get_shortcuts (PROMPT_APPLICATION_DEFAULT));

  gtk_widget_init_template (GTK_WIDGET (self));

#if DEVELOPMENT_BUILD
  gtk_widget_add_css_class (GTK_WIDGET (self), "devel");
#endif

  g_signal_connect_object (self->shortcuts,
                           "notify",
                           G_CALLBACK (prompt_window_shortcuts_notify_cb),
                           self,
                           G_CONNECT_SWAPPED);
  prompt_window_shortcuts_notify_cb (self, NULL, self->shortcuts);

  adw_tab_view_set_shortcuts (self->tab_view, 0);
}

PromptWindow *
prompt_window_new (void)
{
  return prompt_window_new_for_profile (NULL);
}

PromptWindow *
prompt_window_new_for_profile (PromptProfile *profile)
{
  g_autoptr(PromptProfile) default_profile = NULL;
  PromptSettings *settings;
  PromptTerminal *terminal;
  PromptWindow *self;
  PromptTab *tab;
  guint columns;
  guint rows;

  g_return_val_if_fail (!profile || PROMPT_IS_PROFILE (profile), NULL);

  settings = prompt_application_get_settings (PROMPT_APPLICATION_DEFAULT);

  if (profile == NULL)
    {
      default_profile = prompt_application_dup_default_profile (PROMPT_APPLICATION_DEFAULT);
      profile = default_profile;
    }

  g_assert (PROMPT_IS_PROFILE (profile));

  self = g_object_new (PROMPT_TYPE_WINDOW,
                       "application", PROMPT_APPLICATION_DEFAULT,
                       NULL);

  tab = prompt_tab_new (profile);
  terminal = prompt_tab_get_terminal (tab);
  prompt_settings_get_window_size (settings, &columns, &rows);
  vte_terminal_set_size (VTE_TERMINAL (terminal), columns, rows);
  prompt_window_append_tab (self, tab);

  gtk_window_set_default_size (GTK_WINDOW (self), -1, -1);

  return self;
}

void
prompt_window_append_tab (PromptWindow *self,
                          PromptTab    *tab)
{
  g_return_if_fail (PROMPT_IS_WINDOW (self));
  g_return_if_fail (PROMPT_IS_TAB (tab));

  adw_tab_view_append (self->tab_view, GTK_WIDGET (tab));

  gtk_widget_grab_focus (GTK_WIDGET (tab));
}

void
prompt_window_add_tab (PromptWindow *self,
                       PromptTab    *tab)
{
  PromptNewTabPosition new_tab_position;
  PromptApplication *app;
  PromptSettings *settings;
  AdwTabPage *page;
  int position = 0;

  g_return_if_fail (PROMPT_IS_WINDOW (self));
  g_return_if_fail (PROMPT_IS_TAB (tab));

  app = PROMPT_APPLICATION_DEFAULT;
  settings = prompt_application_get_settings (app);
  new_tab_position = prompt_settings_get_new_tab_position (settings);

  if ((page = adw_tab_view_get_selected_page (self->tab_view)))
    {
      position = adw_tab_view_get_page_position (self->tab_view, page);

      switch (new_tab_position)
        {
        case PROMPT_NEW_TAB_POSITION_NEXT:
          position++;
          break;

        case PROMPT_NEW_TAB_POSITION_LAST:
          position = adw_tab_view_get_n_pages (self->tab_view);
          break;

        default:
          g_assert_not_reached ();
        }
    }

  adw_tab_view_insert (self->tab_view, GTK_WIDGET (tab), position);

  gtk_widget_grab_focus (GTK_WIDGET (tab));
}

PromptTab *
prompt_window_get_active_tab (PromptWindow *self)
{
  AdwTabPage *page;

  g_return_val_if_fail (PROMPT_IS_WINDOW (self), NULL);

  if (self->tab_view == NULL)
    return NULL;

  if (!(page = adw_tab_view_get_selected_page (self->tab_view)))
    return NULL;

  return PROMPT_TAB (adw_tab_page_get_child (page));
}

void
prompt_window_set_active_tab (PromptWindow *self,
                              PromptTab    *tab)
{
  AdwTabPage *page;

  g_return_if_fail (PROMPT_IS_WINDOW (self));
  g_return_if_fail (!tab || PROMPT_IS_TAB (tab));

  if (self->tab_view == NULL)
    return;

  if (tab == NULL)
    return;

  if (!(page = adw_tab_view_get_page (self->tab_view, GTK_WIDGET (tab))))
    return;

  adw_tab_view_set_selected_page (self->tab_view, page);
}

static gboolean
prompt_window_remove_visual_bell (gpointer data)
{
  PromptWindow *self = data;

  g_assert (PROMPT_IS_WINDOW (self));

  self->visual_bell_source = 0;

  gtk_widget_remove_css_class (GTK_WIDGET (self->visual_bell), "visual-bell");

  return G_SOURCE_REMOVE;
}

void
prompt_window_visual_bell (PromptWindow *self)
{
  PromptSettings *settings;

  g_return_if_fail (PROMPT_IS_WINDOW (self));

  settings = prompt_application_get_settings (PROMPT_APPLICATION_DEFAULT);
  if (!prompt_settings_get_visual_bell (settings))
    return;

  gtk_widget_add_css_class (GTK_WIDGET (self->visual_bell), "visual-bell");

  g_clear_handle_id (&self->visual_bell_source, g_source_remove);

  self->visual_bell_source = g_timeout_add_full (G_PRIORITY_HIGH_IDLE,
                                                 /* Sync duration with style.css */
                                                 500,
                                                 prompt_window_remove_visual_bell,
                                                 g_object_ref (self),
                                                 g_object_unref);
}

/**
 * prompt_window_get_active_profile:
 * @self: a #PromptWindow
 *
 * Returns: (transfer none) (nullable): the profile of the active tab
 *   or %NULL if no tab is active.
 */
PromptProfile *
prompt_window_get_active_profile (PromptWindow *self)
{
  PromptTab *active_tab;

  g_return_val_if_fail (PROMPT_IS_WINDOW (self), NULL);

  if ((active_tab = prompt_window_get_active_tab (self)))
    return prompt_tab_get_profile (active_tab);

  return NULL;
}

gboolean
prompt_window_focus_tab_by_uuid (PromptWindow *self,
                                 const char   *uuid)
{
  g_autoptr(GtkSelectionModel) model = NULL;
  guint n_items;

  g_return_val_if_fail (PROMPT_IS_WINDOW (self), FALSE);
  g_return_val_if_fail (uuid != NULL, FALSE);

  model = adw_tab_view_get_pages (self->tab_view);
  n_items = g_list_model_get_n_items (G_LIST_MODEL (model));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(AdwTabPage) page = g_list_model_get_item (G_LIST_MODEL (model), i);
      PromptTab *tab = PROMPT_TAB (adw_tab_page_get_child (page));

      if (0 == g_strcmp0 (uuid, prompt_tab_get_uuid (tab)))
        {
          prompt_window_set_active_tab (self, tab);
          return TRUE;
        }
    }

  return FALSE;
}

gboolean
prompt_window_is_animating (PromptWindow *self)
{
  g_return_val_if_fail (PROMPT_IS_WINDOW (self), FALSE);

  return self->tab_overview_animating;
}
