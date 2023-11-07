/*
 * capsule-terminal.c
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

#include <adwaita.h>

#include "capsule-tab.h"
#include "capsule-terminal.h"
#include "capsule-window.h"

#define SIZE_DISMISS_TIMEOUT_MSEC 1000
#define BUILDER_PCRE2_UCP 0x00020000u
#define BUILDER_PCRE2_MULTILINE 0x00000400u

struct _CapsuleTerminal
{
  VteTerminal     parent_instance;

  CapsulePalette *palette;

  GMenuModel     *terminal_menu;

  GtkRevealer    *size_revealer;
  GtkLabel       *size_label;
  guint           size_dismiss_source;
};

enum {
  PROP_0,
  PROP_PALETTE,
  N_PROPS
};

enum {
  MATCH_CLICKED,
  N_SIGNALS
};

G_DEFINE_FINAL_TYPE (CapsuleTerminal, capsule_terminal, VTE_TYPE_TERMINAL)

static GParamSpec *properties [N_PROPS];
static guint signals[N_SIGNALS];
static VteRegex *builtin_dingus_regex[2];
static const char * const builtin_dingus[] = {
  "(((gopher|news|telnet|nntp|file|http|ftp|https)://)|(www|ftp)[-A-Za-z0-9]*\\.)[-A-Za-z0-9\\.]+(:[0-9]*)?",
  "(((gopher|news|telnet|nntp|file|http|ftp|https)://)|(www|ftp)[-A-Za-z0-9]*\\.)[-A-Za-z0-9\\.]+(:[0-9]*)?/[-A-Za-z0-9_\\$\\.\\+\\!\\*\\(\\),;:@&=\\?/~\\#\\%]*[^]'\\.}>\\) ,\\\"]",
};

static gboolean
capsule_terminal_is_active (CapsuleTerminal *self)
{
  CapsuleTerminal *active_terminal = NULL;
  CapsuleWindow *window;
  CapsuleTab *active_tab;

  g_assert (CAPSULE_IS_TERMINAL (self));

  if ((window = CAPSULE_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (self), CAPSULE_TYPE_WINDOW))) &&
      (active_tab = capsule_window_get_active_tab (window)))
    active_terminal = capsule_tab_get_terminal (active_tab);

  return active_terminal == self;
}

static gboolean
capsule_terminal_match_clicked (CapsuleTerminal *self,
                                double           x,
                                double           y,
                                int              button,
                                GdkModifierType  state,
                                const char      *match)
{
  gboolean ret = FALSE;

  g_assert (CAPSULE_IS_TERMINAL (self));
  g_assert (match != NULL);

  g_signal_emit (self, signals[MATCH_CLICKED], 0, x, y, button, state, match, &ret);

  return ret;
}

static void
capsule_terminal_popup (CapsuleTerminal *self,
                        double           x,
                        double           y)
{
  g_assert (CAPSULE_IS_TERMINAL (self));

}

static void
capsule_terminal_bubble_click_pressed_cb (CapsuleTerminal *self,
                                          int              n_press,
                                          double           x,
                                          double           y,
                                          GtkGestureClick *click)
{
  g_assert (CAPSULE_IS_TERMINAL (self));
  g_assert (GTK_IS_GESTURE_CLICK (click));

  if (n_press == 1)
    {
      GdkModifierType state = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (click));
      int button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (click));

      if (button == 3)
        {
          if (!(state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_ALT_MASK)) ||
              !(state & (GDK_CONTROL_MASK | GDK_ALT_MASK)))
            {
              capsule_terminal_popup (self, x, y);
              gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_CLAIMED);
              return;
            }
        }
    }

  gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_DENIED);
}

static void
capsule_terminal_capture_click_pressed_cb (CapsuleTerminal *self,
                                           int              n_press,
                                           double           x,
                                           double           y,
                                           GtkGestureClick *click)
{
  g_autofree char *hyperlink = NULL;
  g_autofree char *match = NULL;
  GdkModifierType state;
  gboolean handled = FALSE;
  GdkEvent *event;
  int button;
  int tag = 0;

  g_assert (CAPSULE_IS_TERMINAL (self));
  g_assert (GTK_IS_GESTURE_CLICK (click));

  event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (click));
  state = gdk_event_get_modifier_state (event) & gtk_accelerator_get_default_mod_mask ();
  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (click));

  hyperlink = vte_terminal_check_hyperlink_at (VTE_TERMINAL (self), x, y);
  match = vte_terminal_check_match_at (VTE_TERMINAL (self), x, y, &tag);

  if (n_press == 1 &&
      !handled &&
      (button == 1 || button == 2) &&
      (state & GDK_CONTROL_MASK))
    {
      if (hyperlink != NULL)
        handled = capsule_terminal_match_clicked (self, x, y, button, state, hyperlink);
      else if (match != NULL)
        handled = capsule_terminal_match_clicked (self, x, y, button, state, match);
    }

  if (handled)
    gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_CLAIMED);
  else
    gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_DENIED);
}

static void
capsule_terminal_measure (GtkWidget      *widget,
                          GtkOrientation  orientation,
                          int             for_size,
                          int            *minimum,
                          int            *natural,
                          int            *minimum_baseline,
                          int            *natural_baseline)
{
  CapsuleTerminal *self = CAPSULE_TERMINAL (widget);
  int min_revealer;
  int nat_revealer;

  GTK_WIDGET_CLASS (capsule_terminal_parent_class)->measure (widget,
                                                             orientation,
                                                             for_size,
                                                             minimum,
                                                             natural,
                                                             minimum_baseline,
                                                             natural_baseline);

  gtk_widget_measure (GTK_WIDGET (self->size_revealer),
                      orientation, for_size,
                      &min_revealer, &nat_revealer, NULL, NULL);

  *minimum = MAX (*minimum, min_revealer);
  *natural = MAX (*natural, nat_revealer);
}

static gboolean
dismiss_size_label_cb (gpointer user_data)
{
  CapsuleTerminal *self = CAPSULE_TERMINAL (user_data);

  gtk_revealer_set_reveal_child (self->size_revealer, FALSE);
  self->size_dismiss_source = 0;

  return G_SOURCE_REMOVE;
}

static void
capsule_terminal_size_allocate (GtkWidget *widget,
                                int        width,
                                int        height,
                                int        baseline)
{
  CapsuleTerminal *self = CAPSULE_TERMINAL (widget);
  GtkRequisition min;
  GtkAllocation revealer_alloc;
  GtkRoot *root;
  int prev_column_count, column_count;
  int prev_row_count, row_count;

  g_assert (CAPSULE_IS_TERMINAL (self));

  prev_column_count = vte_terminal_get_column_count (VTE_TERMINAL (self));
  prev_row_count = vte_terminal_get_row_count (VTE_TERMINAL (self));

  GTK_WIDGET_CLASS (capsule_terminal_parent_class)->size_allocate (widget, width, height, baseline);

  column_count = vte_terminal_get_column_count (VTE_TERMINAL (self));
  row_count = vte_terminal_get_row_count (VTE_TERMINAL (self));

  root = gtk_widget_get_root (widget);

  if (capsule_terminal_is_active (self) &&
      GTK_IS_WINDOW (root) &&
      !gtk_window_is_maximized (GTK_WINDOW (root)) &&
      !gtk_window_is_fullscreen (GTK_WINDOW (root)) &&
      (prev_column_count != column_count || prev_row_count != row_count))
    {
      char format[32];

      g_snprintf (format, sizeof format, "%ld × %ld",
                  vte_terminal_get_column_count (VTE_TERMINAL (self)),
                  vte_terminal_get_row_count (VTE_TERMINAL (self)));
      gtk_label_set_label (self->size_label, format);

      gtk_revealer_set_reveal_child (self->size_revealer, TRUE);

      g_clear_handle_id (&self->size_dismiss_source, g_source_remove);
      self->size_dismiss_source = g_timeout_add (SIZE_DISMISS_TIMEOUT_MSEC,
                                                 dismiss_size_label_cb,
                                                 self);
    }
  else if (gtk_window_is_maximized (GTK_WINDOW (root)) ||
           gtk_window_is_fullscreen (GTK_WINDOW (root)))
    {
      g_clear_handle_id (&self->size_dismiss_source, g_source_remove);
      gtk_revealer_set_reveal_child (self->size_revealer, FALSE);
    }

  gtk_widget_get_preferred_size (GTK_WIDGET (self->size_revealer), &min, NULL);
  revealer_alloc.x = width - min.width;
  revealer_alloc.y = height - min.height;
  revealer_alloc.width = min.width;
  revealer_alloc.height = min.height;
  gtk_widget_size_allocate (GTK_WIDGET (self->size_revealer), &revealer_alloc, -1);
}

static void
capsule_terminal_snapshot (GtkWidget   *widget,
                           GtkSnapshot *snapshot)
{
  CapsuleTerminal *self = CAPSULE_TERMINAL (widget);

  GTK_WIDGET_CLASS (capsule_terminal_parent_class)->snapshot (widget, snapshot);

  gtk_widget_snapshot_child (widget, GTK_WIDGET (self->size_revealer), snapshot);
}

static void
capsule_terminal_dispose (GObject *object)
{
  CapsuleTerminal *self = (CapsuleTerminal *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), CAPSULE_TYPE_TERMINAL);

  g_clear_object (&self->palette);
  g_clear_handle_id (&self->size_dismiss_source, g_source_remove);

  G_OBJECT_CLASS (capsule_terminal_parent_class)->dispose (object);
}

static void
capsule_terminal_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  CapsuleTerminal *self = CAPSULE_TERMINAL (object);

  switch (prop_id)
    {
    case PROP_PALETTE:
      capsule_terminal_set_palette (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
capsule_terminal_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  CapsuleTerminal *self = CAPSULE_TERMINAL (object);

  switch (prop_id)
    {
    case PROP_PALETTE:
      capsule_terminal_set_palette (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
capsule_terminal_class_init (CapsuleTerminalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = capsule_terminal_dispose;
  object_class->get_property = capsule_terminal_get_property;
  object_class->set_property = capsule_terminal_set_property;

  widget_class->measure = capsule_terminal_measure;
  widget_class->size_allocate = capsule_terminal_size_allocate;
  widget_class->snapshot = capsule_terminal_snapshot;

  properties [PROP_PALETTE] =
    g_param_spec_object ("palette", NULL, NULL,
                         CAPSULE_TYPE_PALETTE,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals[MATCH_CLICKED] =
    g_signal_new ("match-clicked",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  g_signal_accumulator_true_handled, NULL,
                  NULL,
                  G_TYPE_BOOLEAN,
                  5,
                  G_TYPE_DOUBLE,
                  G_TYPE_DOUBLE,
                  G_TYPE_INT,
                  GDK_TYPE_MODIFIER_TYPE,
                  G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Capsule/capsule-terminal.ui");

  gtk_widget_class_bind_template_child (widget_class, CapsuleTerminal, size_label);
  gtk_widget_class_bind_template_child (widget_class, CapsuleTerminal, size_revealer);
  gtk_widget_class_bind_template_child (widget_class, CapsuleTerminal, terminal_menu);

  gtk_widget_class_bind_template_callback (widget_class, capsule_terminal_bubble_click_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, capsule_terminal_capture_click_pressed_cb);

  for (guint i = 0; i < G_N_ELEMENTS (builtin_dingus); i++)
    {
      g_autoptr(GError) error = NULL;

      builtin_dingus_regex[i] = vte_regex_new_for_match (builtin_dingus[i],
                                                         strlen (builtin_dingus[i]),
                                                         VTE_REGEX_FLAGS_DEFAULT | BUILDER_PCRE2_MULTILINE | BUILDER_PCRE2_UCP,
                                                         NULL);

      if (!vte_regex_jit (builtin_dingus_regex[i], 0, &error))
        g_warning ("Failed to JIT regex: %s: Regex was: %s",
                   error->message,
                   builtin_dingus[i]);
    }
}

static void
capsule_terminal_init (CapsuleTerminal *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  for (guint i = 0; i < G_N_ELEMENTS (builtin_dingus_regex); i++)
    {
      int tag = vte_terminal_match_add_regex (VTE_TERMINAL (self),
                                              builtin_dingus_regex[i],
                                              0);
      vte_terminal_match_set_cursor_name (VTE_TERMINAL (self), tag, "hand2");
    }
}

CapsulePalette *
capsule_terminal_get_palette (CapsuleTerminal *self)
{
  g_return_val_if_fail (CAPSULE_IS_TERMINAL (self), NULL);

  return self->palette;
}

void
capsule_terminal_set_palette (CapsuleTerminal *self,
                              CapsulePalette  *palette)
{
  g_return_if_fail (CAPSULE_IS_TERMINAL (self));

  if (g_set_object (&self->palette, palette))
    {
      AdwStyleManager *style_manager = adw_style_manager_get_default ();
      g_autoptr(CapsulePalette) fallback = NULL;
      const GdkRGBA *background;
      const GdkRGBA *foreground;
      const GdkRGBA *colors;
      gboolean dark;
      guint n_colors;

      if (palette == NULL)
        palette = fallback = capsule_palette_new_from_name ("gnome");

      dark = adw_style_manager_get_dark (style_manager);
      colors = capsule_palette_get_indexed_colors (palette, &n_colors);
      background = capsule_palette_get_background (palette, dark);
      foreground = capsule_palette_get_foreground (palette, dark);

      g_assert (foreground != NULL);
      g_assert (background != NULL);
      g_assert (colors != NULL);
      g_assert (n_colors == 16);

      vte_terminal_set_colors (VTE_TERMINAL (self),
                               foreground,
                               background,
                               colors,
                               n_colors);

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PALETTE]);
    }
}
