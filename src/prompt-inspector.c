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

#include "prompt-inspector.h"

struct _PromptInspector
{
  AdwApplicationWindow  parent_instance;
  PromptTab            *tab;
};

enum {
  PROP_0,
  PROP_TAB,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PromptInspector, prompt_inspector, ADW_TYPE_APPLICATION_WINDOW)

static GParamSpec *properties[N_PROPS];

static void
prompt_inspector_dispose (GObject *object)
{
  PromptInspector *self = (PromptInspector *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), PROMPT_TYPE_INSPECTOR);

  g_clear_object (&self->tab);

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
      g_value_set_object (value, self->tab);
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
      self->tab = g_value_dup_object (value);
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

  object_class->dispose = prompt_inspector_dispose;
  object_class->get_property = prompt_inspector_get_property;
  object_class->set_property = prompt_inspector_set_property;

  properties[PROP_TAB] =
    g_param_spec_object ("tab", NULL, NULL,
                         PROMPT_TYPE_TAB,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Prompt/prompt-inspector.ui");
}

static void
prompt_inspector_init (PromptInspector *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
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
prompt_inspector_get_tab (PromptInspector *self)
{
  g_return_val_if_fail (PROMPT_IS_INSPECTOR (self), NULL);

  return self->tab;
}
