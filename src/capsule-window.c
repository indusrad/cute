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
  AdwHeaderBar         *header_bar;
  AdwTabBar            *tab_bar;
  AdwTabView           *tab_view;
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
  g_assert (CAPSULE_IS_WINDOW (self));
  g_assert (ADW_IS_TAB_PAGE (page));
  g_assert (ADW_IS_TAB_VIEW (tab_view));

}

static void
capsule_window_notify_selected_page_cb (CapsuleWindow *self,
                                        GParamSpec    *pspec,
                                        AdwTabView    *tab_view)
{
  g_assert (CAPSULE_IS_WINDOW (self));
  g_assert (ADW_IS_TAB_VIEW (tab_view));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVE_TAB]);
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

  window = capsule_window_new_for_profile (profile);

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
capsule_window_dispose (GObject *object)
{
  CapsuleWindow *self = (CapsuleWindow *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), CAPSULE_TYPE_WINDOW);

  G_OBJECT_CLASS (capsule_window_parent_class)->dispose (object);
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
  gtk_widget_class_bind_template_child (widget_class, CapsuleWindow, tab_view);

  gtk_widget_class_bind_template_callback (widget_class, capsule_window_page_attached_cb);
  gtk_widget_class_bind_template_callback (widget_class, capsule_window_page_detached_cb);
  gtk_widget_class_bind_template_callback (widget_class, capsule_window_notify_selected_page_cb);
  gtk_widget_class_bind_template_callback (widget_class, capsule_window_create_window_cb);

  gtk_widget_class_install_action (widget_class, "win.new-tab", "s", capsule_window_new_tab_action);
  gtk_widget_class_install_action (widget_class, "win.new-window", "s", capsule_window_new_window_action);
  gtk_widget_class_install_action (widget_class, "win.new-terminal", "s", capsule_window_new_terminal_action);
}

static void
capsule_window_init (CapsuleWindow *self)
{
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
