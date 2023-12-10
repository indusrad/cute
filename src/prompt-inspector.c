/* prompt-inspector.c
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

#include "prompt-inspector.h"

struct _PromptInspector
{
  AdwPreferencesWindow  parent_instance;

  GSignalGroup         *tab_signals;
  GBindingGroup        *terminal_bindings;

  AdwActionRow         *container_name;
  AdwActionRow         *container_runtime;
  AdwActionRow         *current_directory;
  AdwActionRow         *current_file;
  AdwActionRow         *hyperlink_hover;
  AdwActionRow         *window_title;
};

enum {
  PROP_0,
  PROP_TAB,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PromptInspector, prompt_inspector, ADW_TYPE_PREFERENCES_WINDOW)

static GParamSpec *properties[N_PROPS];

static void
prompt_inspector_set_tab (PromptInspector *self,
                          PromptTab       *tab)
{
  PromptTerminal *terminal;

  g_assert (PROMPT_IS_INSPECTOR (self));
  g_assert (PROMPT_IS_TAB (tab));

  terminal = prompt_tab_get_terminal (tab);

  g_binding_group_set_source (self->terminal_bindings, terminal);
  g_signal_group_set_target (self->tab_signals, tab);
}

static gboolean
bind_with_empty (GBinding     *binding,
                 const GValue *from_value,
                 GValue       *to_value,
                 gpointer      user_data)
{
  const char *str = g_value_get_string (from_value);

  if (str && str[0])
    g_value_set_string (to_value, str);
  else
    g_value_set_static_string (to_value, _("unset"));

  return TRUE;
}

static void
prompt_inspector_constructed (GObject *object)
{
  G_OBJECT_CLASS (prompt_inspector_parent_class)->constructed (object);
}

static void
prompt_inspector_dispose (GObject *object)
{
  PromptInspector *self = (PromptInspector *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), PROMPT_TYPE_INSPECTOR);

  g_clear_object (&self->terminal_bindings);
  g_clear_object (&self->tab_signals);

  G_OBJECT_CLASS (prompt_inspector_parent_class)->dispose (object);
}

static void
prompt_inspector_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  PromptInspector *self = PROMPT_INSPECTOR (object);

  switch (prop_id)
    {
    case PROP_TAB:
      g_value_take_object (value, g_signal_group_dup_target (self->tab_signals));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_inspector_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  PromptInspector *self = PROMPT_INSPECTOR (object);

  switch (prop_id)
    {
    case PROP_TAB:
      prompt_inspector_set_tab (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_inspector_class_init (PromptInspectorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = prompt_inspector_constructed;
  object_class->dispose = prompt_inspector_dispose;
  object_class->get_property = prompt_inspector_get_property;
  object_class->set_property = prompt_inspector_set_property;

  properties[PROP_TAB] =
    g_param_spec_object ("tab", NULL, NULL,
                         PROMPT_TYPE_TAB,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Prompt/prompt-inspector.ui");
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, container_name);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, container_runtime);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, current_directory);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, current_file);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, hyperlink_hover);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, window_title);
}

static void
prompt_inspector_init (PromptInspector *self)
{
  self->terminal_bindings = g_binding_group_new ();
  self->tab_signals = g_signal_group_new (PROMPT_TYPE_TAB);

  gtk_widget_init_template (GTK_WIDGET (self));

  g_binding_group_bind_full (self->terminal_bindings, "current-directory-uri",
                             self->current_directory, "subtitle",
                             G_BINDING_SYNC_CREATE,
                             bind_with_empty, NULL, NULL, NULL);
  g_binding_group_bind_full (self->terminal_bindings, "current-file-uri",
                             self->current_file, "subtitle",
                             G_BINDING_SYNC_CREATE,
                             bind_with_empty, NULL, NULL, NULL);
  g_binding_group_bind_full (self->terminal_bindings, "current-container-name",
                             self->container_name, "subtitle",
                             G_BINDING_SYNC_CREATE,
                             bind_with_empty, NULL, NULL, NULL);
  g_binding_group_bind_full (self->terminal_bindings, "current-container-runtime",
                             self->container_runtime, "subtitle",
                             G_BINDING_SYNC_CREATE,
                             bind_with_empty, NULL, NULL, NULL);
  g_binding_group_bind_full (self->terminal_bindings, "window-title",
                             self->window_title, "subtitle",
                             G_BINDING_SYNC_CREATE,
                             bind_with_empty, NULL, NULL, NULL);
  g_binding_group_bind_full (self->terminal_bindings, "hyperlink-hover-uri",
                             self->hyperlink_hover, "subtitle",
                             G_BINDING_SYNC_CREATE,
                             bind_with_empty, NULL, NULL, NULL);
}

PromptInspector *
prompt_inspector_new (PromptTab *tab)
{
  g_return_val_if_fail (PROMPT_IS_TAB (tab), NULL);

  return g_object_new (PROMPT_TYPE_INSPECTOR,
                       "tab", tab,
                       NULL);
}

PromptTab *
prompt_inspector_dup_tab (PromptInspector *self)
{
  g_return_val_if_fail (PROMPT_IS_INSPECTOR (self), NULL);

  return g_signal_group_dup_target (self->tab_signals);
}
