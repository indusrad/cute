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

#include "capsule-application.h"
#include "capsule-window.h"

struct _CapsuleWindow
{
  AdwApplicationWindow  parent_instance;

  GSignalGroup         *active_tab_signals;

  AdwHeaderBar         *header_bar;
  AdwTabBar            *tab_bar;
  AdwTabOverview       *tab_overview;
  AdwTabView           *tab_view;
  GtkBox               *visual_bell;

  guint                 visual_bell_source;
};

G_DEFINE_FINAL_TYPE (CapsuleWindow, capsule_window, ADW_TYPE_APPLICATION_WINDOW)

enum {
  PROP_0,
  PROP_ACTIVE_TAB,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

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
  AdwTabPage *page;
  gboolean has_page = FALSE;

  g_assert (CAPSULE_IS_WINDOW (self));
  g_assert (ADW_IS_TAB_VIEW (tab_view));

  if ((page = adw_tab_view_get_selected_page (self->tab_view)))
    {
      CapsuleTab *tab = CAPSULE_TAB (adw_tab_page_get_child (page));
      g_signal_group_set_target (self->active_tab_signals, tab);
      gtk_widget_grab_focus (GTK_WIDGET (tab));
      has_page = TRUE;
    }

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "win.zoom-in", has_page);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "win.zoom-out", has_page);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "win.zoom-one", has_page);

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

static void
capsule_window_new_tab_action (GtkWidget  *widget,
                               const char *action_name,
                               GVariant   *param)
{
  CapsuleWindow *self = (CapsuleWindow *)widget;
  g_autoptr(CapsuleProfile) profile = NULL;
  CapsuleApplication *app;
  CapsuleTab *tab;
  const char *profile_uuid;

  g_assert (CAPSULE_IS_WINDOW (self));
  g_assert (param != NULL);
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));

  app = CAPSULE_APPLICATION_DEFAULT;
  profile_uuid = g_variant_get_string (param, NULL);

  if (profile_uuid[0] == 0 || g_str_equal (profile_uuid, "default"))
    profile = capsule_application_dup_default_profile (app);
  else
    profile = capsule_application_dup_profile (app, profile_uuid);

  tab = capsule_tab_new (profile);
  capsule_window_apply_current_settings (self, tab);

  capsule_window_append_tab (self, tab);
  capsule_window_set_active_tab (self, tab);
}

static void
capsule_window_new_window_action (GtkWidget  *widget,
                                  const char *action_name,
                                  GVariant   *param)
{
  CapsuleWindow *self = (CapsuleWindow *)widget;
  g_autoptr(CapsuleProfile) profile = NULL;
  CapsuleApplication *app;
  CapsuleWindow *window;
  CapsuleTab *tab;
  const char *profile_uuid;

  g_assert (CAPSULE_IS_WINDOW (self));
  g_assert (param != NULL);
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));

  app = CAPSULE_APPLICATION_DEFAULT;
  profile_uuid = g_variant_get_string (param, NULL);

  if (profile_uuid[0] == 0 || g_str_equal (profile_uuid, "default"))
    profile = capsule_application_dup_default_profile (app);
  else
    profile = capsule_application_dup_profile (app, profile_uuid);

  tab = capsule_tab_new (profile);
  capsule_window_apply_current_settings (self, tab);

  window = g_object_new (CAPSULE_TYPE_WINDOW, NULL);
  capsule_window_append_tab (window, tab);

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
capsule_window_dispose (GObject *object)
{
  CapsuleWindow *self = (CapsuleWindow *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), CAPSULE_TYPE_WINDOW);

  g_signal_group_set_target (self->active_tab_signals, NULL);

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
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = capsule_window_dispose;
  object_class->finalize = capsule_window_finalize;
  object_class->get_property = capsule_window_get_property;
  object_class->set_property = capsule_window_set_property;

  properties [PROP_ACTIVE_TAB] =
    g_param_spec_object ("active-tab", NULL, NULL,
                         CAPSULE_TYPE_TAB,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Capsule/capsule-window.ui");

  gtk_widget_class_bind_template_child (widget_class, CapsuleWindow, header_bar);
  gtk_widget_class_bind_template_child (widget_class, CapsuleWindow, tab_bar);
  gtk_widget_class_bind_template_child (widget_class, CapsuleWindow, tab_overview);
  gtk_widget_class_bind_template_child (widget_class, CapsuleWindow, tab_view);
  gtk_widget_class_bind_template_child (widget_class, CapsuleWindow, visual_bell);

  gtk_widget_class_bind_template_callback (widget_class, capsule_window_page_attached_cb);
  gtk_widget_class_bind_template_callback (widget_class, capsule_window_page_detached_cb);
  gtk_widget_class_bind_template_callback (widget_class, capsule_window_notify_selected_page_cb);
  gtk_widget_class_bind_template_callback (widget_class, capsule_window_create_window_cb);

  gtk_widget_class_install_action (widget_class, "win.new-tab", "s", capsule_window_new_tab_action);
  gtk_widget_class_install_action (widget_class, "win.new-window", "s", capsule_window_new_window_action);
  gtk_widget_class_install_action (widget_class, "win.new-terminal", "s", capsule_window_new_terminal_action);
  gtk_widget_class_install_action (widget_class, "win.tab-overview", NULL, capsule_window_tab_overview_action);
  gtk_widget_class_install_action (widget_class, "win.zoom-in", NULL, capsule_window_zoom_in_action);
  gtk_widget_class_install_action (widget_class, "win.zoom-out", NULL, capsule_window_zoom_out_action);
  gtk_widget_class_install_action (widget_class, "win.zoom-one", NULL, capsule_window_zoom_one_action);
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

  gtk_widget_init_template (GTK_WIDGET (self));

#if DEVELOPMENT_BUILD
  gtk_widget_add_css_class (GTK_WIDGET (self), "devel");
#endif
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
  CapsuleWindow *self;
  CapsuleTab *tab;

  g_return_val_if_fail (!profile || CAPSULE_IS_PROFILE (profile), NULL);

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

  capsule_window_append_tab (self, tab);

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
  g_return_if_fail (CAPSULE_IS_WINDOW (self));

  gtk_widget_add_css_class (GTK_WIDGET (self->visual_bell), "visual-bell");

  g_clear_handle_id (&self->visual_bell_source, g_source_remove);

  self->visual_bell_source = g_timeout_add_full (G_PRIORITY_HIGH_IDLE,
                                                 /* Sync duration with style.css */
                                                 500,
                                                 capsule_window_remove_visual_bell,
                                                 g_object_ref (self),
                                                 g_object_unref);
}
