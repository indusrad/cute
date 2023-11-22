/*
 * prompt-find-bar.c
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

#include "prompt-find-bar.h"
#include "prompt-util.h"

struct _PromptFindBar
{
  GtkWidget        parent_instance;

  PromptTerminal *terminal;

  GtkEntry        *entry;
  GtkCheckButton  *use_regex;
  GtkCheckButton  *whole_words;
  GtkCheckButton  *match_case;
};

enum {
  PROP_0,
  PROP_TERMINAL,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PromptFindBar, prompt_find_bar, GTK_TYPE_WIDGET)

static GParamSpec *properties [N_PROPS];

static void
prompt_find_bar_dismiss (GtkWidget  *widget,
                         const char *action_name,
                         GVariant   *param)
{
  PromptFindBar *self = (PromptFindBar *)widget;
  GtkWidget *revealer;

  g_assert (PROMPT_IS_FIND_BAR (self));

  if ((revealer = gtk_widget_get_ancestor (widget, GTK_TYPE_REVEALER)))
    gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), FALSE);

  if (self->terminal != NULL)
    gtk_widget_grab_focus (GTK_WIDGET (self->terminal));
}

static gboolean
prompt_find_bar_grab_focus (GtkWidget *widget)
{
  return gtk_widget_grab_focus (GTK_WIDGET (PROMPT_FIND_BAR (widget)->entry));
}

static char *
prompt_find_bar_get_search (PromptFindBar *self,
                            guint         *flags)
{
  g_autofree char *escaped = NULL;
  g_autofree char *boundaries = NULL;
  const char *text;

  g_assert (PROMPT_IS_FIND_BAR (self));

  text = gtk_editable_get_text (GTK_EDITABLE (self->entry));

  if (prompt_str_empty0 (text))
    return NULL;

  if (!gtk_check_button_get_active (GTK_CHECK_BUTTON (self->match_case)))
    *flags |= VTE_PCRE2_CASELESS;

  if (!gtk_check_button_get_active (GTK_CHECK_BUTTON (self->use_regex)))
    text = escaped = g_regex_escape_string (text, -1);

  if (gtk_check_button_get_active (GTK_CHECK_BUTTON (self->whole_words)))
    text = boundaries = g_strdup_printf ("\\b%s\\b", text);

  return g_strdup (text);
}

static void
prompt_find_bar_next (GtkWidget  *widget,
                      const char *action_name,
                      GVariant   *param)
{
  PromptFindBar *self = (PromptFindBar *)widget;
  g_autoptr(VteRegex) regex = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *query = NULL;
  guint flags = VTE_PCRE2_MULTILINE;

  g_assert (PROMPT_IS_FIND_BAR (self));

  if (self->terminal == NULL)
    return;

  if ((query = prompt_find_bar_get_search (self, &flags)))
    regex = vte_regex_new_for_search (query, -1, flags, &error);

  vte_terminal_search_set_regex (VTE_TERMINAL (self->terminal), regex, 0);
  vte_terminal_search_find_next (VTE_TERMINAL (self->terminal));
}

static void
prompt_find_bar_previous (GtkWidget  *widget,
                          const char *action_name,
                          GVariant   *param)
{
  PromptFindBar *self = (PromptFindBar *)widget;
  g_autoptr(VteRegex) regex = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *query = NULL;
  guint flags = VTE_PCRE2_MULTILINE;

  g_assert (PROMPT_IS_FIND_BAR (self));

  if (self->terminal == NULL)
    return;

  if ((query = prompt_find_bar_get_search (self, &flags)))
    regex = vte_regex_new_for_search (query, -1, flags, &error);

  vte_terminal_search_set_regex (VTE_TERMINAL (self->terminal), regex, 0);
  vte_terminal_search_find_previous (VTE_TERMINAL (self->terminal));
}

static void
prompt_find_bar_dispose (GObject *object)
{
  PromptFindBar *self = (PromptFindBar *)object;
  GtkWidget *child;

  gtk_widget_dispose_template (GTK_WIDGET (self), PROMPT_TYPE_FIND_BAR);

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self))))
    gtk_widget_unparent (child);

  g_clear_object (&self->terminal);

  G_OBJECT_CLASS (prompt_find_bar_parent_class)->dispose (object);
}

static void
prompt_find_bar_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  PromptFindBar *self = PROMPT_FIND_BAR (object);

  switch (prop_id)
    {
    case PROP_TERMINAL:
      g_value_set_object (value, self->terminal);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_find_bar_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  PromptFindBar *self = PROMPT_FIND_BAR (object);

  switch (prop_id)
    {
    case PROP_TERMINAL:
      self->terminal = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_find_bar_class_init (PromptFindBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = prompt_find_bar_dispose;
  object_class->get_property = prompt_find_bar_get_property;
  object_class->set_property = prompt_find_bar_set_property;

  widget_class->grab_focus = prompt_find_bar_grab_focus;

  properties[PROP_TERMINAL] =
    g_param_spec_object ("terminal", NULL, NULL,
                         PROMPT_TYPE_TERMINAL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Prompt/prompt-find-bar.ui");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "findbar");

  gtk_widget_class_bind_template_child (widget_class, PromptFindBar, entry);
  gtk_widget_class_bind_template_child (widget_class, PromptFindBar, use_regex);
  gtk_widget_class_bind_template_child (widget_class, PromptFindBar, whole_words);
  gtk_widget_class_bind_template_child (widget_class, PromptFindBar, match_case);

  gtk_widget_class_install_action (widget_class, "search.dismiss", NULL, prompt_find_bar_dismiss);
  gtk_widget_class_install_action (widget_class, "search.down", NULL, prompt_find_bar_next);
  gtk_widget_class_install_action (widget_class, "search.up", NULL, prompt_find_bar_previous);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "search.dismiss", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_g, GDK_CONTROL_MASK, "search.up", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_g, GDK_CONTROL_MASK|GDK_SHIFT_MASK, "search.down", NULL);
}

static void
prompt_find_bar_init (PromptFindBar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

PromptTerminal *
prompt_find_bar_get_terminal (PromptFindBar *self)
{
  g_return_val_if_fail (PROMPT_IS_FIND_BAR (self), NULL);

  return self->terminal;
}

void
prompt_find_bar_set_terminal (PromptFindBar  *self,
                               PromptTerminal *terminal)
{
  g_return_if_fail (PROMPT_IS_FIND_BAR (self));
  g_return_if_fail (!terminal || PROMPT_IS_TERMINAL (terminal));

  if (g_set_object (&self->terminal, terminal))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TERMINAL]);
}
