/* capsule-window.c
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

#include "capsule-application.h"
#include "capsule-close-dialog.h"
#include "capsule-find-bar.h"
#include "capsule-settings.h"
#include "capsule-title-dialog.h"
#include "capsule-window.h"
#include "capsule-window-dressing.h"

struct _CapsuleWindow
{
  AdwApplicationWindow   parent_instance;

  CapsuleCloseDialog    *close_dialog;

  CapsuleShortcuts      *shortcuts;

  CapsuleFindBar        *find_bar;
  GtkRevealer           *find_bar_revealer;
  AdwHeaderBar          *header_bar;
  GMenu                 *primary_menu;
  AdwTabBar             *tab_bar;
  GMenu                 *tab_menu;
  AdwTabOverview        *tab_overview;
  AdwTabView            *tab_view;

  GSignalGroup          *active_tab_signals;
  CapsuleWindowDressing *dressing;
  GtkBox                *visual_bell;

  guint                  visual_bell_source;
  guint                  focus_active_tab_source;
};

G_DEFINE_FINAL_TYPE (CapsuleWindow, capsule_window, ADW_TYPE_APPLICATION_WINDOW)

enum {
  PROP_0,
  PROP_ACTIVE_TAB,
  PROP_SHORTCUTS,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
capsule_window_save_size (CapsuleWindow *self)
{
  CapsuleTab *active_tab;

  g_assert (CAPSULE_IS_WINDOW (self));

  if ((active_tab = capsule_window_get_active_tab (self)))
    {
      CapsuleSettings *settings = capsule_application_get_settings (CAPSULE_APPLICATION_DEFAULT);
      CapsuleTerminal *terminal = capsule_tab_get_terminal (active_tab);
      guint columns = vte_terminal_get_column_count (VTE_TERMINAL (terminal));
      guint rows = vte_terminal_get_row_count (VTE_TERMINAL (terminal));

      capsule_settings_set_window_size (settings, columns, rows);
    }
}

static gboolean
capsule_window_close_page_cb (CapsuleWindow *self,
                              AdwTabPage    *tab_page,
                              AdwTabView    *tab_view)
{
  CapsuleTab *tab;

  g_assert (CAPSULE_IS_WINDOW (self));
  g_assert (ADW_IS_TAB_PAGE (tab_page));
  g_assert (ADW_IS_TAB_VIEW (tab_view));

  capsule_window_save_size (self);

  tab = CAPSULE_TAB (adw_tab_page_get_child (tab_page));
  if (!capsule_tab_is_running (tab))
    return GDK_EVENT_PROPAGATE;

  /* TODO: Setup dialog to confirm close */

  return GDK_EVENT_STOP;
}

static gboolean
capsule_window_focus_active_tab_cb (gpointer data)
{
  CapsuleWindow *self = data;
  CapsuleTab *active_tab;

  g_assert (CAPSULE_IS_WINDOW (self));

  self->focus_active_tab_source = 0;

  if ((active_tab = capsule_window_get_active_tab (self)))
    gtk_widget_grab_focus (GTK_WIDGET (active_tab));

  return G_SOURCE_REMOVE;
}

static void
capsule_window_tab_overview_notify_open_cb (CapsuleWindow  *self,
                                            GParamSpec     *pspec,
                                            AdwTabOverview *tab_overview)
{
  g_assert (CAPSULE_IS_WINDOW (self));
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
      GtkSettings *settings = gtk_settings_get_default ();
      gboolean gtk_enable_animations = TRUE;
      guint delay_msec = 410; /* Sync with libadwaita */

      g_object_get (settings,
                    "gtk-enable-animations", &gtk_enable_animations,
                    NULL);

      if (!gtk_enable_animations)
        delay_msec = 10;

      self->focus_active_tab_source = g_timeout_add_full (G_PRIORITY_LOW,
                                                          delay_msec,
                                                          capsule_window_focus_active_tab_cb,
                                                          self, NULL);
    }
}

static void
capsule_window_setup_menu_cb (CapsuleWindow *self,
                              AdwTabPage    *page,
                              AdwTabView    *view)
{
  g_assert (CAPSULE_IS_WINDOW (self));
  g_assert (!page || ADW_IS_TAB_PAGE (page));
  g_assert (ADW_IS_TAB_VIEW (view));

  if (page != NULL)
    adw_tab_view_set_selected_page (view, page);
}

static AdwTabView *
capsule_window_create_window_cb (CapsuleWindow *self,
                                 AdwTabView    *tab_view)
{
  CapsuleWindow *other;

  g_assert (CAPSULE_IS_WINDOW (self));

  other = g_object_new (CAPSULE_TYPE_WINDOW, NULL);
  gtk_window_present (GTK_WINDOW (other));
  return other->tab_view;
}

static void
capsule_window_page_attached_cb (CapsuleWindow *self,
                                 AdwTabPage    *page,
                                 int            position,
                                 AdwTabView    *tab_view)
{
  GtkWidget *child;

  g_assert (CAPSULE_IS_WINDOW (self));
  g_assert (ADW_IS_TAB_PAGE (page));
  g_assert (ADW_IS_TAB_VIEW (tab_view));

  child = adw_tab_page_get_child (page);

  g_object_bind_property (child, "title", page, "title", G_BINDING_SYNC_CREATE);
}

static void
capsule_window_page_detached_cb (CapsuleWindow *self,
                                 AdwTabPage    *page,
                                 int            position,
                                 AdwTabView    *tab_view)
{
  guint n_pages;

  g_assert (CAPSULE_IS_WINDOW (self));
  g_assert (ADW_IS_TAB_PAGE (page));
  g_assert (ADW_IS_TAB_VIEW (tab_view));

  n_pages = adw_tab_view_get_n_pages (tab_view);

  if (n_pages == 0)
    gtk_window_destroy (GTK_WINDOW (self));
}

static void
capsule_window_notify_selected_page_cb (CapsuleWindow *self,
                                        GParamSpec    *pspec,
                                        AdwTabView    *tab_view)
{
  g_autoptr(GPropertyAction) read_only = NULL;
  CapsuleTerminal *terminal = NULL;
  AdwTabPage *page;
  gboolean has_page = FALSE;

  g_assert (CAPSULE_IS_WINDOW (self));
  g_assert (ADW_IS_TAB_VIEW (tab_view));

  if ((page = adw_tab_view_get_selected_page (self->tab_view)))
    {
      CapsuleTab *tab = CAPSULE_TAB (adw_tab_page_get_child (page));
      CapsuleProfile *profile = capsule_tab_get_profile (tab);

      has_page = TRUE;

      terminal = capsule_tab_get_terminal (tab);

      g_signal_group_set_target (self->active_tab_signals, tab);

      g_object_bind_property (profile, "palette",
                              self->dressing, "palette",
                              G_BINDING_SYNC_CREATE);
      g_object_bind_property (profile, "opacity",
                              self->dressing, "opacity",
                              G_BINDING_SYNC_CREATE);

      read_only = g_property_action_new ("tab.read-only", tab, "read-only");

      gtk_widget_grab_focus (GTK_WIDGET (tab));
    }

  if (terminal == NULL)
    gtk_revealer_set_reveal_child (self->find_bar_revealer, FALSE);

  capsule_find_bar_set_terminal (self->find_bar, terminal);

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "win.zoom-in", has_page);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "win.zoom-out", has_page);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "win.zoom-one", has_page);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "win.search", has_page);

  g_action_map_remove_action (G_ACTION_MAP (self), "tab.read-only");
  if (read_only != NULL)
    g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (read_only));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVE_TAB]);
}

static void
capsule_window_apply_current_settings (CapsuleWindow *self,
                                       CapsuleTab    *tab)
{
  CapsuleTab *active_tab;

  g_assert (CAPSULE_IS_WINDOW (self));
  g_assert (CAPSULE_IS_TAB (tab));

  if ((active_tab = capsule_window_get_active_tab (self)))
    {
      const char *current_directory_uri = capsule_tab_get_current_directory_uri (active_tab);
      CapsuleZoomLevel zoom = capsule_tab_get_zoom (active_tab);

      if (current_directory_uri != NULL)
        capsule_tab_set_previous_working_directory_uri (tab, current_directory_uri);

      capsule_tab_set_zoom (tab, zoom);
    }
}

static CapsuleProfile *
capsule_window_dup_profile_for_param (CapsuleWindow *self,
                                      const char    *profile_uuid)
{
  g_autoptr(CapsuleProfile) profile = NULL;
  CapsuleApplication *app;
  CapsuleProfile *active_profile;

  g_assert (CAPSULE_IS_WINDOW (self));
  g_assert (profile_uuid != NULL);

  app = CAPSULE_APPLICATION_DEFAULT;
  active_profile = capsule_window_get_active_profile (self);

  if (profile_uuid[0] == 0 && active_profile != NULL)
    profile = g_object_ref (active_profile);
  else if (profile_uuid[0] == 0 || g_str_equal (profile_uuid, "default"))
    profile = capsule_application_dup_default_profile (app);
  else
    profile = capsule_application_dup_profile (app, profile_uuid);

  return g_steal_pointer (&profile);
}

static void
capsule_window_new_tab_action (GtkWidget  *widget,
                               const char *action_name,
                               GVariant   *param)
{
  CapsuleWindow *self = (CapsuleWindow *)widget;
  g_autoptr(CapsuleProfile) profile = NULL;
  const char *profile_uuid;
  CapsuleTab *tab;

  g_assert (CAPSULE_IS_WINDOW (self));
  g_assert (param != NULL);
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));

  profile_uuid = g_variant_get_string (param, NULL);
  profile = capsule_window_dup_profile_for_param (self, profile_uuid);

  tab = capsule_tab_new (profile);
  capsule_window_apply_current_settings (self, tab);

  capsule_window_add_tab (self, tab);
  capsule_window_set_active_tab (self, tab);
}

static void
capsule_window_new_window_action (GtkWidget  *widget,
                                  const char *action_name,
                                  GVariant   *param)
{
  CapsuleWindow *self = (CapsuleWindow *)widget;
  g_autoptr(CapsuleProfile) profile = NULL;
  CapsuleWindow *window;
  const char *profile_uuid;
  CapsuleTab *tab;

  g_assert (CAPSULE_IS_WINDOW (self));
  g_assert (param != NULL);
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));

  profile_uuid = g_variant_get_string (param, NULL);
  profile = capsule_window_dup_profile_for_param (self, profile_uuid);

  tab = capsule_tab_new (profile);
  capsule_window_apply_current_settings (self, tab);

  window = g_object_new (CAPSULE_TYPE_WINDOW, NULL);
  capsule_window_add_tab (window, tab);

  gtk_window_present (GTK_WINDOW (window));
}

static void
capsule_window_new_terminal_action (GtkWidget  *widget,
                                    const char *action_name,
                                    GVariant   *param)
{
  g_assert (CAPSULE_IS_WINDOW (widget));
  g_assert (param != NULL);
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));

  if (capsule_application_control_is_pressed (CAPSULE_APPLICATION_DEFAULT))
    capsule_window_new_window_action (widget, NULL, param);
  else
    capsule_window_new_tab_action (widget, NULL, param);
}

static void
capsule_window_tab_overview_action (GtkWidget  *widget,
                                    const char *action_name,
                                    GVariant   *param)
{
  CapsuleWindow *self = (CapsuleWindow *)widget;

  g_assert (CAPSULE_IS_WINDOW (self));

  adw_tab_overview_set_open (self->tab_overview, TRUE);
}

static void
capsule_window_zoom_in_action (GtkWidget  *widget,
                               const char *action_name,
                               GVariant   *param)
{
  CapsuleWindow *self = (CapsuleWindow *)widget;
  CapsuleTab *active_tab;

  g_assert (CAPSULE_IS_WINDOW (self));

  if ((active_tab = capsule_window_get_active_tab (self)))
    {
      capsule_tab_zoom_in (active_tab);
      gtk_window_set_default_size (GTK_WINDOW (self), -1, -1);
    }
}

static void
capsule_window_zoom_out_action (GtkWidget  *widget,
                                const char *action_name,
                                GVariant   *param)
{
  CapsuleWindow *self = (CapsuleWindow *)widget;
  CapsuleTab *active_tab;

  g_assert (CAPSULE_IS_WINDOW (self));

  if ((active_tab = capsule_window_get_active_tab (self)))
    {
      capsule_tab_zoom_out (active_tab);
      gtk_window_set_default_size (GTK_WINDOW (self), -1, -1);
    }
}

static void
capsule_window_zoom_one_action (GtkWidget  *widget,
                                const char *action_name,
                                GVariant   *param)
{
  CapsuleWindow *self = (CapsuleWindow *)widget;
  CapsuleTab *active_tab;

  g_assert (CAPSULE_IS_WINDOW (self));

  if ((active_tab = capsule_window_get_active_tab (self)))
    {
      capsule_tab_set_zoom (active_tab, CAPSULE_ZOOM_LEVEL_DEFAULT);
      gtk_window_set_default_size (GTK_WINDOW (self), -1, -1);
    }
}

static void
capsule_window_active_tab_bell_cb (CapsuleWindow *self,
                                   CapsuleTab    *tab)
{
  g_assert (CAPSULE_IS_WINDOW (self));
  g_assert (CAPSULE_IS_TAB (tab));

  capsule_window_visual_bell (self);
}

static void
capsule_window_close_action (GtkWidget  *widget,
                             const char *action_name,
                             GVariant   *param)
{
  CapsuleWindow *self = (CapsuleWindow *)widget;
  CapsuleTab *tab;
  AdwTabPage *tab_page;

  g_assert (CAPSULE_IS_WINDOW (self));

  if (!(tab = capsule_window_get_active_tab (self)))
    return;

  tab_page = adw_tab_view_get_page (self->tab_view, GTK_WIDGET (tab));
  adw_tab_view_close_page (self->tab_view, tab_page);
}

static void
capsule_window_close_others_action (GtkWidget  *widget,
                                    const char *action_name,
                                    GVariant   *param)
{
  CapsuleWindow *self = (CapsuleWindow *)widget;
  CapsuleTab *tab;
  AdwTabPage *tab_page;

  g_assert (CAPSULE_IS_WINDOW (self));

  if (!(tab = capsule_window_get_active_tab (self)))
    return;

  tab_page = adw_tab_view_get_page (self->tab_view, GTK_WIDGET (tab));
  adw_tab_view_close_other_pages (self->tab_view, tab_page);
}

static void
capsule_window_detach_action (GtkWidget  *widget,
                              const char *action_name,
                              GVariant   *param)
{
  CapsuleWindow *self = (CapsuleWindow *)widget;
  CapsuleWindow *new_window;
  CapsuleTab *tab;
  AdwTabPage *tab_page;

  g_assert (CAPSULE_IS_WINDOW (self));

  if (!(tab = capsule_window_get_active_tab (self)))
    return;

  tab_page = adw_tab_view_get_page (self->tab_view, GTK_WIDGET (tab));

  new_window = g_object_new (CAPSULE_TYPE_WINDOW, NULL);
  adw_tab_view_transfer_page (self->tab_view, tab_page, new_window->tab_view, 0);

  gtk_window_present (GTK_WINDOW (new_window));
}

static void
capsule_window_page_previous_action (GtkWidget  *widget,
                                     const char *action_name,
                                     GVariant   *param)
{
  adw_tab_view_select_previous_page (CAPSULE_WINDOW (widget)->tab_view);
}

static void
capsule_window_page_next_action (GtkWidget  *widget,
                                 const char *action_name,
                                 GVariant   *param)
{
  adw_tab_view_select_next_page (CAPSULE_WINDOW (widget)->tab_view);
}

static void
capsule_window_tab_focus_action (GtkWidget  *widget,
                                 const char *action_name,
                                 GVariant   *param)
{
  CapsuleWindow *self = (CapsuleWindow *)widget;
  int position;

  g_assert (CAPSULE_IS_WINDOW (self));
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
capsule_window_tab_reset_action (GtkWidget  *widget,
                                 const char *action_name,
                                 GVariant   *param)
{
  CapsuleWindow *self = (CapsuleWindow *)widget;
  CapsuleTerminal *terminal;
  CapsuleTab *tab;
  gboolean clear;

  g_assert (CAPSULE_IS_WINDOW (self));
  g_assert (param != NULL);
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_BOOLEAN));

  clear = g_variant_get_boolean (param);

  if (!(tab = capsule_window_get_active_tab (self)))
    return;

  terminal = capsule_tab_get_terminal (tab);
  vte_terminal_reset (VTE_TERMINAL (terminal), TRUE, clear);
}

static void
capsule_window_move_left_action (GtkWidget  *widget,
                                 const char *action_name,
                                 GVariant   *param)
{
  CapsuleWindow *self = (CapsuleWindow *)widget;
  CapsuleTab *tab;
  AdwTabPage *tab_page;

  g_assert (CAPSULE_IS_WINDOW (self));

  if (!(tab = capsule_window_get_active_tab (self)))
    return;

  tab_page = adw_tab_view_get_page (self->tab_view, GTK_WIDGET (tab));
  adw_tab_view_reorder_backward (self->tab_view, tab_page);
  capsule_tab_raise (tab);
}

static void
capsule_window_move_right_action (GtkWidget  *widget,
                                  const char *action_name,
                                  GVariant   *param)
{
  CapsuleWindow *self = (CapsuleWindow *)widget;
  CapsuleTab *tab;
  AdwTabPage *tab_page;

  g_assert (CAPSULE_IS_WINDOW (self));

  if (!(tab = capsule_window_get_active_tab (self)))
    return;

  tab_page = adw_tab_view_get_page (self->tab_view, GTK_WIDGET (tab));
  adw_tab_view_reorder_forward (self->tab_view, tab_page);
  capsule_tab_raise (tab);
}

static void
capsule_window_fullscreen_action (GtkWidget  *widget,
                                  const char *action_name,
                                  GVariant   *param)
{
  gtk_window_fullscreen (GTK_WINDOW (widget));
}

static void
capsule_window_unfullscreen_action (GtkWidget  *widget,
                                    const char *action_name,
                                    GVariant   *param)
{
  gtk_window_unfullscreen (GTK_WINDOW (widget));
}

static void
capsule_window_toggle_fullscreen (GtkWidget  *widget,
                                  const char *action_name,
                                  GVariant   *param)
{
  if (gtk_window_is_fullscreen (GTK_WINDOW (widget)))
    gtk_window_unfullscreen (GTK_WINDOW (widget));
  else
    gtk_window_fullscreen (GTK_WINDOW (widget));
}

static void
capsule_window_toplevel_state_changed_cb (CapsuleWindow *self,
                                          GParamSpec    *pspec,
                                          GdkToplevel   *toplevel)
{
  GdkToplevelState state;
  gboolean is_fullscreen;

  g_assert (CAPSULE_IS_WINDOW (self));
  g_assert (GDK_IS_TOPLEVEL (toplevel));

  state = gdk_toplevel_get_state (toplevel);

  is_fullscreen = !!(state & GDK_TOPLEVEL_STATE_FULLSCREEN);

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "win.fullscreen", !is_fullscreen);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "win.unfullscreen", is_fullscreen);
}

static void
capsule_window_realize (GtkWidget *widget)
{
  CapsuleWindow *self = (CapsuleWindow *)widget;
  GdkToplevel *toplevel;

  g_assert (CAPSULE_IS_WINDOW (self));

  GTK_WIDGET_CLASS (capsule_window_parent_class)->realize (widget);

  toplevel = GDK_TOPLEVEL (gtk_native_get_surface (GTK_NATIVE (self)));

  g_signal_connect_object (toplevel,
                           "notify::state",
                           G_CALLBACK (capsule_window_toplevel_state_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
capsule_window_shortcuts_notify_cb (CapsuleWindow    *self,
                                    GParamSpec       *pspec,
                                    CapsuleShortcuts *shortcuts)
{
  g_assert (CAPSULE_IS_WINDOW (self));
  g_assert (CAPSULE_IS_SHORTCUTS (shortcuts));

  capsule_shortcuts_update_menu (shortcuts, self->primary_menu);
  capsule_shortcuts_update_menu (shortcuts, self->tab_menu);
}

static void
capsule_window_set_title_action (GtkWidget  *widget,
                                 const char *action_name,
                                 GVariant   *param)
{
  CapsuleWindow *self = (CapsuleWindow *)widget;
  AdwMessageDialog *dialog;
  CapsuleTab *active_tab;

  g_assert (CAPSULE_IS_WINDOW (self));

  if (!(active_tab = capsule_window_get_active_tab (self)))
    return;

  dialog = g_object_new (CAPSULE_TYPE_TITLE_DIALOG,
                         "modal", TRUE,
                         "resizable", FALSE,
                         "tab", active_tab,
                         "title", _("Set Title"),
                         "transient-for", self,
                         NULL);

  gtk_window_present (GTK_WINDOW (dialog));
}

static void
capsule_window_search_action (GtkWidget  *widget,
                              const char *action_name,
                              GVariant   *param)
{
  CapsuleWindow *self = (CapsuleWindow *)widget;

  g_assert (CAPSULE_IS_WINDOW (self));

  gtk_revealer_set_reveal_child (self->find_bar_revealer, TRUE);
  gtk_widget_grab_focus (GTK_WIDGET (self->find_bar));
}

static gboolean
capsule_window_close_request (GtkWindow *window)
{
  CapsuleWindow *self = (CapsuleWindow *)window;

  g_assert (CAPSULE_IS_WINDOW (self));

  capsule_window_save_size (self);

  return FALSE;
}

static void
capsule_window_constructed (GObject *object)
{
  CapsuleWindow *self = (CapsuleWindow *)object;

  G_OBJECT_CLASS (capsule_window_parent_class)->constructed (object);

  self->dressing = capsule_window_dressing_new (self);

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "win.unfullscreen", FALSE);
}

static void
capsule_window_dispose (GObject *object)
{
  CapsuleWindow *self = (CapsuleWindow *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), CAPSULE_TYPE_WINDOW);

  g_signal_group_set_target (self->active_tab_signals, NULL);
  g_clear_handle_id (&self->focus_active_tab_source, g_source_remove);

  G_OBJECT_CLASS (capsule_window_parent_class)->dispose (object);
}

static void
capsule_window_finalize (GObject *object)
{
  CapsuleWindow *self = (CapsuleWindow *)object;

  g_clear_object (&self->active_tab_signals);

  G_OBJECT_CLASS (capsule_window_parent_class)->finalize (object);
}

static void
capsule_window_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  CapsuleWindow *self = CAPSULE_WINDOW (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_TAB:
      g_value_set_object (value, capsule_window_get_active_tab (self));
      break;

    case PROP_SHORTCUTS:
      g_value_set_object (value, self->shortcuts);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
capsule_window_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  CapsuleWindow *self = CAPSULE_WINDOW (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_TAB:
      capsule_window_set_active_tab (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
capsule_window_class_init (CapsuleWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkWindowClass *window_class = GTK_WINDOW_CLASS (klass);

  object_class->constructed = capsule_window_constructed;
  object_class->dispose = capsule_window_dispose;
  object_class->finalize = capsule_window_finalize;
  object_class->get_property = capsule_window_get_property;
  object_class->set_property = capsule_window_set_property;

  widget_class->realize = capsule_window_realize;

  window_class->close_request = capsule_window_close_request;

  properties[PROP_ACTIVE_TAB] =
    g_param_spec_object ("active-tab", NULL, NULL,
                         CAPSULE_TYPE_TAB,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_SHORTCUTS] =
    g_param_spec_object ("shortcuts", NULL, NULL,
                         CAPSULE_TYPE_SHORTCUTS,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Capsule/capsule-window.ui");

  gtk_widget_class_bind_template_child (widget_class, CapsuleWindow, find_bar);
  gtk_widget_class_bind_template_child (widget_class, CapsuleWindow, find_bar_revealer);
  gtk_widget_class_bind_template_child (widget_class, CapsuleWindow, header_bar);
  gtk_widget_class_bind_template_child (widget_class, CapsuleWindow, primary_menu);
  gtk_widget_class_bind_template_child (widget_class, CapsuleWindow, tab_bar);
  gtk_widget_class_bind_template_child (widget_class, CapsuleWindow, tab_menu);
  gtk_widget_class_bind_template_child (widget_class, CapsuleWindow, tab_overview);
  gtk_widget_class_bind_template_child (widget_class, CapsuleWindow, tab_view);
  gtk_widget_class_bind_template_child (widget_class, CapsuleWindow, visual_bell);

  gtk_widget_class_bind_template_callback (widget_class, capsule_window_page_attached_cb);
  gtk_widget_class_bind_template_callback (widget_class, capsule_window_page_detached_cb);
  gtk_widget_class_bind_template_callback (widget_class, capsule_window_notify_selected_page_cb);
  gtk_widget_class_bind_template_callback (widget_class, capsule_window_create_window_cb);
  gtk_widget_class_bind_template_callback (widget_class, capsule_window_close_page_cb);
  gtk_widget_class_bind_template_callback (widget_class, capsule_window_setup_menu_cb);
  gtk_widget_class_bind_template_callback (widget_class, capsule_window_tab_overview_notify_open_cb);

  gtk_widget_class_install_action (widget_class, "win.new-tab", "s", capsule_window_new_tab_action);
  gtk_widget_class_install_action (widget_class, "win.new-window", "s", capsule_window_new_window_action);
  gtk_widget_class_install_action (widget_class, "win.new-terminal", "s", capsule_window_new_terminal_action);
  gtk_widget_class_install_action (widget_class, "win.fullscreen", NULL, capsule_window_fullscreen_action);
  gtk_widget_class_install_action (widget_class, "win.unfullscreen", NULL, capsule_window_unfullscreen_action);
  gtk_widget_class_install_action (widget_class, "win.toggle-fullscreen", NULL, capsule_window_toggle_fullscreen);
  gtk_widget_class_install_action (widget_class, "win.tab-overview", NULL, capsule_window_tab_overview_action);
  gtk_widget_class_install_action (widget_class, "win.zoom-in", NULL, capsule_window_zoom_in_action);
  gtk_widget_class_install_action (widget_class, "win.zoom-out", NULL, capsule_window_zoom_out_action);
  gtk_widget_class_install_action (widget_class, "win.zoom-one", NULL, capsule_window_zoom_one_action);
  gtk_widget_class_install_action (widget_class, "page.move-left", NULL, capsule_window_move_left_action);
  gtk_widget_class_install_action (widget_class, "page.move-right", NULL, capsule_window_move_right_action);
  gtk_widget_class_install_action (widget_class, "page.close", NULL, capsule_window_close_action);
  gtk_widget_class_install_action (widget_class, "page.close-others", NULL, capsule_window_close_others_action);
  gtk_widget_class_install_action (widget_class, "page.detach", NULL, capsule_window_detach_action);
  gtk_widget_class_install_action (widget_class, "tab.reset", "b", capsule_window_tab_reset_action);
  gtk_widget_class_install_action (widget_class, "tab.focus", "i", capsule_window_tab_focus_action);
  gtk_widget_class_install_action (widget_class, "page.next", NULL, capsule_window_page_next_action);
  gtk_widget_class_install_action (widget_class, "page.previous", NULL, capsule_window_page_previous_action);
  gtk_widget_class_install_action (widget_class, "win.set-title", NULL, capsule_window_set_title_action);
  gtk_widget_class_install_action (widget_class, "win.search", NULL, capsule_window_search_action);

  g_type_ensure (CAPSULE_TYPE_FIND_BAR);
}

static void
capsule_window_init (CapsuleWindow *self)
{
  self->active_tab_signals = g_signal_group_new (CAPSULE_TYPE_TAB);

  g_signal_group_connect_object (self->active_tab_signals,
                                 "bell",
                                 G_CALLBACK (capsule_window_active_tab_bell_cb),
                                 self,
                                 G_CONNECT_SWAPPED);

  self->shortcuts = g_object_ref (capsule_application_get_shortcuts (CAPSULE_APPLICATION_DEFAULT));

  gtk_widget_init_template (GTK_WIDGET (self));

#if DEVELOPMENT_BUILD
  gtk_widget_add_css_class (GTK_WIDGET (self), "devel");
#endif

  g_signal_connect_object (self->shortcuts,
                           "notify",
                           G_CALLBACK (capsule_window_shortcuts_notify_cb),
                           self,
                           G_CONNECT_SWAPPED);
  capsule_window_shortcuts_notify_cb (self, NULL, self->shortcuts);

  adw_tab_view_set_shortcuts (self->tab_view, 0);
}

CapsuleWindow *
capsule_window_new (void)
{
  return capsule_window_new_for_profile (NULL);
}

CapsuleWindow *
capsule_window_new_for_profile (CapsuleProfile *profile)
{
  g_autoptr(CapsuleProfile) default_profile = NULL;
  CapsuleSettings *settings;
  CapsuleTerminal *terminal;
  CapsuleWindow *self;
  CapsuleTab *tab;
  guint columns;
  guint rows;

  g_return_val_if_fail (!profile || CAPSULE_IS_PROFILE (profile), NULL);

  settings = capsule_application_get_settings (CAPSULE_APPLICATION_DEFAULT);

  if (profile == NULL)
    {
      default_profile = capsule_application_dup_default_profile (CAPSULE_APPLICATION_DEFAULT);
      profile = default_profile;
    }

  g_assert (CAPSULE_IS_PROFILE (profile));

  self = g_object_new (CAPSULE_TYPE_WINDOW,
                       "application", CAPSULE_APPLICATION_DEFAULT,
                       NULL);

  tab = capsule_tab_new (profile);
  terminal = capsule_tab_get_terminal (tab);
  capsule_settings_get_window_size (settings, &columns, &rows);
  vte_terminal_set_size (VTE_TERMINAL (terminal), columns, rows);
  capsule_window_append_tab (self, tab);

  gtk_window_set_default_size (GTK_WINDOW (self), -1, -1);

  return self;
}

void
capsule_window_append_tab (CapsuleWindow *self,
                           CapsuleTab    *tab)
{
  g_return_if_fail (CAPSULE_IS_WINDOW (self));
  g_return_if_fail (CAPSULE_IS_TAB (tab));

  adw_tab_view_append (self->tab_view, GTK_WIDGET (tab));

  gtk_widget_grab_focus (GTK_WIDGET (tab));
}

void
capsule_window_add_tab (CapsuleWindow *self,
                        CapsuleTab    *tab)
{
  CapsuleNewTabPosition new_tab_position;
  CapsuleApplication *app;
  CapsuleSettings *settings;
  AdwTabPage *page;
  int position = 0;

  g_return_if_fail (CAPSULE_IS_WINDOW (self));
  g_return_if_fail (CAPSULE_IS_TAB (tab));

  app = CAPSULE_APPLICATION_DEFAULT;
  settings = capsule_application_get_settings (app);
  new_tab_position = capsule_settings_get_new_tab_position (settings);

  if ((page = adw_tab_view_get_selected_page (self->tab_view)))
    {
      position = adw_tab_view_get_page_position (self->tab_view, page);

      switch (new_tab_position)
        {
        case CAPSULE_NEW_TAB_POSITION_NEXT:
          position++;
          break;

        case CAPSULE_NEW_TAB_POSITION_LAST:
          position = adw_tab_view_get_n_pages (self->tab_view);
          break;

        default:
          g_assert_not_reached ();
        }
    }

  adw_tab_view_insert (self->tab_view, GTK_WIDGET (tab), position);

  gtk_widget_grab_focus (GTK_WIDGET (tab));
}

CapsuleTab *
capsule_window_get_active_tab (CapsuleWindow *self)
{
  AdwTabPage *page;

  g_return_val_if_fail (CAPSULE_IS_WINDOW (self), NULL);

  if (self->tab_view == NULL)
    return NULL;

  if (!(page = adw_tab_view_get_selected_page (self->tab_view)))
    return NULL;

  return CAPSULE_TAB (adw_tab_page_get_child (page));
}

void
capsule_window_set_active_tab (CapsuleWindow *self,
                               CapsuleTab    *tab)
{
  AdwTabPage *page;

  g_return_if_fail (CAPSULE_IS_WINDOW (self));
  g_return_if_fail (!tab || CAPSULE_IS_TAB (tab));

  if (self->tab_view == NULL)
    return;

  if (tab == NULL)
    return;

  if (!(page = adw_tab_view_get_page (self->tab_view, GTK_WIDGET (tab))))
    return;

  adw_tab_view_set_selected_page (self->tab_view, page);
}

static gboolean
capsule_window_remove_visual_bell (gpointer data)
{
  CapsuleWindow *self = data;

  g_assert (CAPSULE_IS_WINDOW (self));

  self->visual_bell_source = 0;

  gtk_widget_remove_css_class (GTK_WIDGET (self->visual_bell), "visual-bell");

  return G_SOURCE_REMOVE;
}

void
capsule_window_visual_bell (CapsuleWindow *self)
{
  CapsuleSettings *settings;

  g_return_if_fail (CAPSULE_IS_WINDOW (self));

  settings = capsule_application_get_settings (CAPSULE_APPLICATION_DEFAULT);
  if (!capsule_settings_get_visual_bell (settings))
    return;

  gtk_widget_add_css_class (GTK_WIDGET (self->visual_bell), "visual-bell");

  g_clear_handle_id (&self->visual_bell_source, g_source_remove);

  self->visual_bell_source = g_timeout_add_full (G_PRIORITY_HIGH_IDLE,
                                                 /* Sync duration with style.css */
                                                 500,
                                                 capsule_window_remove_visual_bell,
                                                 g_object_ref (self),
                                                 g_object_unref);
}

/**
 * capsule_window_get_active_profile:
 * @self: a #CapsuleWindow
 *
 * Returns: (transfer none) (nullable): the profile of the active tab
 *   or %NULL if no tab is active.
 */
CapsuleProfile *
capsule_window_get_active_profile (CapsuleWindow *self)
{
  CapsuleTab *active_tab;

  g_return_val_if_fail (CAPSULE_IS_WINDOW (self), NULL);

  if ((active_tab = capsule_window_get_active_tab (self)))
    return capsule_tab_get_profile (active_tab);

  return NULL;
}
