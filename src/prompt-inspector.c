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
#include "prompt-palette-preview-color.h"

struct _PromptInspector
{
  AdwPreferencesWindow       parent_instance;

  GSignalGroup              *terminal_signals;
  GBindingGroup             *terminal_bindings;
  GtkEventController        *motion;

  AdwActionRow              *cell_size;
  AdwActionRow              *command;
  AdwActionRow              *container_name;
  AdwActionRow              *container_runtime;
  AdwActionRow              *current_directory;
  AdwActionRow              *current_file;
  AdwActionRow              *cursor;
  AdwActionRow              *pointer;
  AdwActionRow              *font_desc;
  AdwActionRow              *grid_size;
  AdwActionRow              *hyperlink_hover;
  AdwActionRow              *window_title;
  GtkLabel                  *pid;
  PromptPalettePreviewColor *color0;
  PromptPalettePreviewColor *color1;
  PromptPalettePreviewColor *color2;
  PromptPalettePreviewColor *color3;
  PromptPalettePreviewColor *color4;
  PromptPalettePreviewColor *color5;
  PromptPalettePreviewColor *color6;
  PromptPalettePreviewColor *color7;
  PromptPalettePreviewColor *color8;
  PromptPalettePreviewColor *color9;
  PromptPalettePreviewColor *color10;
  PromptPalettePreviewColor *color11;
  PromptPalettePreviewColor *color12;
  PromptPalettePreviewColor *color13;
  PromptPalettePreviewColor *color14;
  PromptPalettePreviewColor *color15;
};

enum {
  PROP_0,
  PROP_TAB,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PromptInspector, prompt_inspector, ADW_TYPE_PREFERENCES_WINDOW)

static GParamSpec *properties[N_PROPS];

static void
prompt_inspector_update_font (PromptInspector *self,
                              GParamSpec      *pspec,
                              PromptTerminal  *terminal)
{
  const PangoFontDescription *font_desc = vte_terminal_get_font (VTE_TERMINAL (terminal));
  PromptTab *tab = PROMPT_TAB (gtk_widget_get_ancestor (GTK_WIDGET (terminal), PROMPT_TYPE_TAB));
  g_autofree char *str = font_desc ? pango_font_description_to_string (font_desc) : NULL;
  g_autofree char *label = prompt_tab_dup_zoom_label (tab);
  g_autoptr(GString) gstr = g_string_new (str);

  if (gstr->len == 0)
    g_string_append (gstr, _("unset"));

  g_string_append_printf (gstr, " at %s", label);

  adw_action_row_set_subtitle (self->font_desc, gstr->str);
}

static void
prompt_inspector_cursor_moved_cb (PromptInspector *self,
                                  PromptTerminal  *terminal)
{
  g_autofree char *str = NULL;
  glong column;
  glong row;

  g_assert (PROMPT_IS_INSPECTOR (self));
  g_assert (PROMPT_IS_TERMINAL (terminal));

  vte_terminal_get_cursor_position (VTE_TERMINAL (terminal), &column, &row);

  str = g_strdup_printf ("%s: %3d,  %s: %3d",
                         _("Row"), 1 + (int)row, _("Column"), 1 + (int)column);
  adw_action_row_set_subtitle (self->cursor, str);
}

static void
prompt_inspector_char_size_changed_cb (PromptInspector *self,
                                       guint            width,
                                       guint            height,
                                       PromptTerminal  *terminal)
{
  g_autofree char *str = NULL;
  int scale_factor;

  g_assert (PROMPT_IS_INSPECTOR (self));
  g_assert (PROMPT_IS_TERMINAL (terminal));

  scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (terminal));
  str = g_strdup_printf ("%u × %u Units (Scale Factor %u)", width, height, scale_factor);

  adw_action_row_set_subtitle (self->cell_size, str);
}

static void
prompt_inspector_grid_size_changed_cb (PromptInspector *self,
                                       guint            columns,
                                       guint            rows,
                                       PromptTerminal  *terminal)
{
  g_autofree char *str = NULL;

  g_assert (PROMPT_IS_INSPECTOR (self));
  g_assert (PROMPT_IS_TERMINAL (terminal));

  str = g_strdup_printf ("%u × %u", columns, rows);

  adw_action_row_set_subtitle (self->grid_size, str);
}

static void
prompt_inspector_shell_preexec_cb (PromptInspector *self,
                                   PromptTerminal  *terminal)
{
  g_autoptr(PromptTab) tab = NULL;
  g_autofree char *cmdline = NULL;
  GPid pid;

  g_assert (PROMPT_IS_INSPECTOR (self));
  g_assert (PROMPT_IS_TERMINAL (terminal));

  if ((tab = prompt_inspector_dup_tab (self)) &&
      prompt_tab_has_foreground_process (tab, &pid, &cmdline))
    {
      char pidstr[16];

      g_snprintf (pidstr, sizeof pidstr, "%d", pid);
      gtk_label_set_label (self->pid, pidstr);
      adw_action_row_set_subtitle (self->command, cmdline);
    }
  else
    {
      gtk_label_set_label (self->pid, NULL);
      adw_action_row_set_subtitle (self->command, _("Shell"));
    }
}

static void
prompt_inspector_shell_precmd_cb (PromptInspector *self,
                                  PromptTerminal  *terminal)
{
  g_assert (PROMPT_IS_INSPECTOR (self));
  g_assert (PROMPT_IS_TERMINAL (terminal));

  /* NOTE: If we are in a container that also supports VTE patches
   * then it will send shell-precmd via escape sequences. The reality
   * is that our foreground is the `toolbox enter` process (until we
   * have better patches in VTE) but we show "Shell" instead.
   *
   * This is fine for now, but I'd like it to be better and actually
   * show the proper `tcgetpgrp()` foreground.
   */

  adw_action_row_set_subtitle (self->command, _("Shell"));
  gtk_label_set_label (self->pid, NULL);
}

static void
prompt_inspector_bind_terminal_cb (PromptInspector *self,
                                   PromptTerminal  *terminal,
                                   GSignalGroup    *group)
{
  guint width;
  guint height;
  guint rows;
  guint columns;

  g_assert (PROMPT_IS_INSPECTOR (self));
  g_assert (PROMPT_IS_TERMINAL (terminal));
  g_assert (G_IS_SIGNAL_GROUP (group));

  width = vte_terminal_get_char_width (VTE_TERMINAL (terminal));
  height = vte_terminal_get_char_height (VTE_TERMINAL (terminal));

  columns = vte_terminal_get_column_count (VTE_TERMINAL (terminal));
  rows = vte_terminal_get_row_count (VTE_TERMINAL (terminal));

  prompt_inspector_cursor_moved_cb (self, terminal);
  prompt_inspector_char_size_changed_cb (self, width, height, terminal);
  prompt_inspector_grid_size_changed_cb (self, columns, rows, terminal);
  prompt_inspector_update_font (self, NULL, terminal);
  prompt_inspector_shell_preexec_cb (self, terminal);
}

static PromptTerminal *
get_terminal (PromptInspector *self)
{
  return PROMPT_TERMINAL (gtk_event_controller_get_widget (self->motion));
}

static gboolean
get_coord_at_xy (PromptInspector *self,
                 double           x,
                 double           y,
                 guint           *row,
                 guint           *col)
{
  PromptTerminal *terminal = get_terminal (self);
  guint width = gtk_widget_get_width (GTK_WIDGET (terminal));
  guint height = gtk_widget_get_height (GTK_WIDGET (terminal));
  guint char_width;
  guint char_height;

  if (x < 0 || x >= width)
    return FALSE;

  if (y < 0 || y >= height)
    return FALSE;

  char_width = vte_terminal_get_char_width (VTE_TERMINAL (terminal));
  char_height = vte_terminal_get_char_height (VTE_TERMINAL (terminal));

  *row = x / char_width;
  *col = y / char_height;

  return TRUE;
}


static void
prompt_inspector_update_pointer (PromptInspector *self,
                                 double           x,
                                 double           y)
{
  g_autofree char *str = NULL;
  guint row, col;

  g_assert (PROMPT_IS_INSPECTOR (self));

  if (get_coord_at_xy (self, x, y, &col, &row))
    str = g_strdup_printf ("%s: %u,  %s: %u", _("Row"), row + 1, _("Column"), col + 1);

  adw_action_row_set_subtitle (self->pointer, str ? str : _("untracked"));
}

static void
prompt_inspector_motion_enter_cb (PromptInspector          *self,
                                  double                    x,
                                  double                    y,
                                  GtkEventControllerMotion *motion)
{
  g_assert (PROMPT_IS_INSPECTOR (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_MOTION (motion));

  prompt_inspector_update_pointer (self, x, y);
}

static void
prompt_inspector_motion_leave_cb (PromptInspector          *self,
                                  GtkEventControllerMotion *motion)
{
  g_assert (PROMPT_IS_INSPECTOR (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_MOTION (motion));

  prompt_inspector_update_pointer (self, -1, -1);
}

static void
prompt_inspector_motion_notify_cb (PromptInspector          *self,
                                   double                    x,
                                   double                    y,
                                   GtkEventControllerMotion *motion)
{
  g_assert (PROMPT_IS_INSPECTOR (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_MOTION (motion));

  prompt_inspector_update_pointer (self, x, y);
}

static void
prompt_inspector_set_tab (PromptInspector *self,
                          PromptTab       *tab)
{
  PromptTerminal *terminal;

  g_assert (PROMPT_IS_INSPECTOR (self));
  g_assert (PROMPT_IS_TAB (tab));

  terminal = prompt_tab_get_terminal (tab);

  self->motion = gtk_event_controller_motion_new ();
  g_signal_connect_object (self->motion,
                           "enter",
                           G_CALLBACK (prompt_inspector_motion_enter_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->motion,
                           "leave",
                           G_CALLBACK (prompt_inspector_motion_leave_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->motion,
                           "motion",
                           G_CALLBACK (prompt_inspector_motion_notify_cb),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_widget_add_controller (GTK_WIDGET (terminal), g_object_ref (self->motion));

  g_binding_group_set_source (self->terminal_bindings, terminal);
  g_signal_group_set_target (self->terminal_signals, terminal);
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

PromptTab *
prompt_inspector_dup_tab (PromptInspector *self)
{
  g_autoptr(PromptTerminal) terminal = NULL;
  PromptTab *tab = NULL;

  g_return_val_if_fail (PROMPT_IS_INSPECTOR (self), NULL);

  if ((terminal = g_signal_group_dup_target (self->terminal_signals)))
    tab = PROMPT_TAB (gtk_widget_get_ancestor (GTK_WIDGET (terminal), PROMPT_TYPE_TAB));

  return tab ? g_object_ref (tab) : NULL;
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

  if (self->terminal_signals)
    {
      g_autoptr(PromptTerminal) terminal = g_signal_group_dup_target (self->terminal_signals);

      if (terminal && self->motion)
        gtk_widget_remove_controller (GTK_WIDGET (terminal), self->motion);
    }

  gtk_widget_dispose_template (GTK_WIDGET (self), PROMPT_TYPE_INSPECTOR);

  g_clear_object (&self->terminal_bindings);
  g_clear_object (&self->terminal_signals);
  g_clear_object (&self->motion);

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
      g_value_take_object (value, prompt_inspector_dup_tab (self));
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
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, cell_size);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, color0);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, color1);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, color2);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, color3);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, color4);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, color5);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, color6);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, color7);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, color8);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, color9);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, color10);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, color11);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, color12);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, color13);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, color14);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, color15);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, command);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, container_name);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, container_runtime);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, current_directory);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, current_file);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, cursor);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, font_desc);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, grid_size);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, hyperlink_hover);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, pid);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, pointer);
  gtk_widget_class_bind_template_child (widget_class, PromptInspector, window_title);

  g_type_ensure (PROMPT_TYPE_PALETTE_PREVIEW_COLOR);
}

static void
prompt_inspector_init (PromptInspector *self)
{
  self->terminal_bindings = g_binding_group_new ();
  self->terminal_signals = g_signal_group_new (PROMPT_TYPE_TERMINAL);

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

#define BIND_PALETTE(n) \
  G_STMT_START { \
    g_binding_group_bind (self->terminal_bindings, "palette", \
                          self->color##n, "palette", \
                          G_BINDING_SYNC_CREATE); \
    g_object_bind_property (adw_style_manager_get_default (), "dark", \
                            self->color##n, "dark", \
                            G_BINDING_SYNC_CREATE); \
  } G_STMT_END

  BIND_PALETTE (0);
  BIND_PALETTE (1);
  BIND_PALETTE (2);
  BIND_PALETTE (3);
  BIND_PALETTE (4);
  BIND_PALETTE (5);
  BIND_PALETTE (6);
  BIND_PALETTE (7);
  BIND_PALETTE (8);
  BIND_PALETTE (9);
  BIND_PALETTE (10);
  BIND_PALETTE (11);
  BIND_PALETTE (12);
  BIND_PALETTE (13);
  BIND_PALETTE (14);
  BIND_PALETTE (15);

  g_signal_connect_object (self->terminal_signals,
                           "bind",
                           G_CALLBACK (prompt_inspector_bind_terminal_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->terminal_signals,
                                 "cursor-moved",
                                 G_CALLBACK (prompt_inspector_cursor_moved_cb),
                                 self,
                                 G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->terminal_signals,
                                 "char-size-changed",
                                 G_CALLBACK (prompt_inspector_char_size_changed_cb),
                                 self,
                                 G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->terminal_signals,
                                 "grid-size-changed",
                                 G_CALLBACK (prompt_inspector_grid_size_changed_cb),
                                 self,
                                 G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->terminal_signals,
                                 "notify::font-desc",
                                 G_CALLBACK (prompt_inspector_update_font),
                                 self,
                                 G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->terminal_signals,
                                 "shell-precmd",
                                 G_CALLBACK (prompt_inspector_shell_precmd_cb),
                                 self,
                                 G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->terminal_signals,
                                 "shell-preexec",
                                 G_CALLBACK (prompt_inspector_shell_preexec_cb),
                                 self,
                                 G_CONNECT_SWAPPED);
}

PromptInspector *
prompt_inspector_new (PromptTab *tab)
{
  g_return_val_if_fail (PROMPT_IS_TAB (tab), NULL);

  return g_object_new (PROMPT_TYPE_INSPECTOR,
                       "tab", tab,
                       NULL);
}
