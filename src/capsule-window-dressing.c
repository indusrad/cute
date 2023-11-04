/*
 * capsule-window-dressing.c
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

#include <math.h>

#include "capsule-window-dressing.h"

struct _CapsuleWindowDressing
{
  GObject         parent_instance;
  GWeakRef        window_wr;
  CapsulePalette *palette;
  GtkCssProvider *css_provider;
  char           *css_class;
  double          opacity;
  guint           queued_update;
};

enum {
  PROP_0,
  PROP_PALETTE,
  PROP_WINDOW,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (CapsuleWindowDressing, capsule_window_dressing, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];
static guint last_sequence;

static gboolean
rgba_is_dark (const GdkRGBA *rgba)
{
  /* http://alienryderflex.com/hsp.html */
  double r = rgba->red * 255.0;
  double g = rgba->green * 255.0;
  double b = rgba->blue * 255.0;
  double hsp = sqrt (0.299 * (r * r) +
                     0.587 * (g * g) +
                     0.114 * (b * b));

  return hsp <= 127.5;
}

static void
capsule_window_dressing_update (CapsuleWindowDressing *self)
{
  g_autoptr(GString) string = NULL;

  g_assert (CAPSULE_IS_WINDOW_DRESSING (self));

  string = g_string_new (NULL);

  if (self->palette != NULL)
    {
      AdwStyleManager *style_manager = adw_style_manager_get_default ();
      g_autoptr(GString) gstring = g_string_new (NULL);
      g_autofree char *bg = NULL;
      g_autofree char *fg = NULL;
      char window_alpha_str[G_ASCII_DTOSTR_BUF_SIZE];
      char popover_alpha_str[G_ASCII_DTOSTR_BUF_SIZE];
      double window_alpha;
      double popover_alpha;
      const GdkRGBA *bg_rgba;
      const GdkRGBA *fg_rgba;

      bg_rgba = capsule_palette_get_background (self->palette);
      fg_rgba = capsule_palette_get_foreground (self->palette);

      if (adw_style_manager_get_dark (style_manager))
        {
          const GdkRGBA *tmp = fg_rgba;
          fg_rgba = bg_rgba;
          bg_rgba = tmp;
        }

      bg = gdk_rgba_to_string (bg_rgba);
      fg = gdk_rgba_to_string (fg_rgba);

      window_alpha = self->opacity;
      popover_alpha = MAX (window_alpha, 0.85);

      g_ascii_dtostr (popover_alpha_str, sizeof popover_alpha_str, popover_alpha);
      g_ascii_dtostr (window_alpha_str, sizeof window_alpha_str, window_alpha);

      g_string_append_printf (string,
                              "window.%s { dressing: %s; background: alpha(%s, %s); }\n",
                              self->css_class, fg, bg, window_alpha_str);
      g_string_append_printf (string,
                              "window.%s popover > contents { dressing: %s; background: alpha(%s, %s); }\n",
                              self->css_class, fg, bg, popover_alpha_str);
      g_string_append_printf (string,
                              "window.%s popover > arrow { background: alpha(%s, %s); }\n",
                              self->css_class, bg, popover_alpha_str);
      g_string_append_printf (string,
                              "window.%s vte-capsule > revealer.size label { dressing: %s; background-dressing: alpha(%s, %s); }\n",
                              self->css_class, fg, bg, popover_alpha_str);

      if (rgba_is_dark (bg_rgba))
        g_string_append_printf (string,
                                "window.%s toolbarview > revealer > windowhandle { dressing: %s; background: shade(%s,1.25); }\n",
                                self->css_class, fg, bg);
      else
        g_string_append_printf (string,
                                "window.%s toolbarview > revealer > windowhandle { dressing: %s; background: shade(%s, .95); }\n",
                                self->css_class, fg, bg);
    }

  gtk_css_provider_load_from_string (self->css_provider, string->str);
}

static gboolean
capsule_window_dressing_update_idle (gpointer user_data)
{
  CapsuleWindowDressing *self = CAPSULE_WINDOW_DRESSING (user_data);

  self->queued_update = 0;

  capsule_window_dressing_update (self);

  return G_SOURCE_REMOVE;
}

static void
capsule_window_dressing_queue_update (CapsuleWindowDressing *self)
{
  g_assert (CAPSULE_IS_WINDOW_DRESSING (self));

  if (self->queued_update == 0)
    self->queued_update = g_idle_add_full (G_PRIORITY_HIGH_IDLE,
                                           capsule_window_dressing_update_idle,
                                           self, NULL);
}

static void
capsule_window_dressing_set_window (CapsuleWindowDressing *self,
                                    CapsuleWindow         *window)
{
  g_assert (CAPSULE_IS_WINDOW_DRESSING (self));
  g_assert (CAPSULE_IS_WINDOW (window));

  g_weak_ref_set (&self->window_wr, window);

  if (self->opacity < 1.0)
    gtk_widget_add_css_class (GTK_WIDGET (window), self->css_class);
}

static void
capsule_window_dressing_constructed (GObject *object)
{
  CapsuleWindowDressing *self = (CapsuleWindowDressing *)object;

  G_OBJECT_CLASS (capsule_window_dressing_parent_class)->constructed (object);

  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (self->css_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION+1);

  capsule_window_dressing_queue_update (self);
}

static void
capsule_window_dressing_dispose (GObject *object)
{
  CapsuleWindowDressing *self = (CapsuleWindowDressing *)object;

  capsule_window_dressing_set_palette (self, NULL);

  if (self->css_provider != NULL) {
    gtk_style_context_remove_provider_for_display (gdk_display_get_default (),
                                                   GTK_STYLE_PROVIDER (self->css_provider));
    g_clear_object (&self->css_provider);
  }

  g_clear_object (&self->palette);
  g_weak_ref_set (&self->window_wr, NULL);

  g_clear_handle_id (&self->queued_update, g_source_remove);

  G_OBJECT_CLASS (capsule_window_dressing_parent_class)->dispose (object);
}

static void
capsule_window_dressing_finalize (GObject *object)
{
  CapsuleWindowDressing *self = (CapsuleWindowDressing *)object;

  g_weak_ref_clear (&self->window_wr);
  g_clear_pointer (&self->css_class, g_free);

  g_assert (self->palette == NULL);
  g_assert (self->queued_update == 0);
  g_assert (self->css_provider == NULL);

  G_OBJECT_CLASS (capsule_window_dressing_parent_class)->finalize (object);
}

static void
capsule_window_dressing_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  CapsuleWindowDressing *self = CAPSULE_WINDOW_DRESSING (object);

  switch (prop_id)
    {
    case PROP_PALETTE:
      g_value_set_object (value, capsule_window_dressing_get_palette (self));
      break;

    case PROP_WINDOW:
      g_value_take_object (value, capsule_window_dressing_dup_window (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
capsule_window_dressing_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  CapsuleWindowDressing *self = CAPSULE_WINDOW_DRESSING (object);

  switch (prop_id)
    {
    case PROP_PALETTE:
      capsule_window_dressing_set_palette (self, CAPSULE_PALETTE (g_value_get_object (value)));
      break;

    case PROP_WINDOW:
      capsule_window_dressing_set_window (self, CAPSULE_WINDOW (g_value_get_object (value)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
capsule_window_dressing_class_init (CapsuleWindowDressingClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = capsule_window_dressing_constructed;
  object_class->dispose = capsule_window_dressing_dispose;
  object_class->finalize = capsule_window_dressing_finalize;
  object_class->get_property = capsule_window_dressing_get_property;
  object_class->set_property = capsule_window_dressing_set_property;

  properties [PROP_PALETTE] =
    g_param_spec_object ("palette", NULL, NULL,
                         CAPSULE_TYPE_PALETTE,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_WINDOW] =
    g_param_spec_object ("window", NULL, NULL,
                         CAPSULE_TYPE_WINDOW,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
capsule_window_dressing_init (CapsuleWindowDressing *self)
{
  self->css_provider = gtk_css_provider_new ();
  self->css_class = g_strdup_printf ("window-dressing-%u", ++last_sequence);
  self->opacity = 1.0;

  g_weak_ref_init (&self->window_wr, NULL);
}

CapsuleWindow *
capsule_window_dressing_dup_window (CapsuleWindowDressing *self)
{
  g_return_val_if_fail (CAPSULE_IS_WINDOW_DRESSING (self), NULL);

  return CAPSULE_WINDOW (g_weak_ref_get (&self->window_wr));
}

CapsuleWindowDressing *
capsule_window_dressing_new (CapsuleWindow *window)
{
  g_return_val_if_fail (CAPSULE_IS_WINDOW (window), NULL);

  return (CapsuleWindowDressing *)g_object_new (CAPSULE_TYPE_WINDOW_DRESSING,
                                               "window", window,
                                               NULL);
}

CapsulePalette *
capsule_window_dressing_get_palette (CapsuleWindowDressing *self)
{
  g_return_val_if_fail (CAPSULE_IS_WINDOW_DRESSING (self), NULL);

  return self->palette;
}

void
capsule_window_dressing_set_palette (CapsuleWindowDressing *self,
                                     CapsulePalette         *palette)
{
  g_return_if_fail (CAPSULE_IS_WINDOW_DRESSING (self));
  g_return_if_fail (!palette || CAPSULE_IS_PALETTE (palette));

  if (g_set_object (&self->palette, palette))
    {
      capsule_window_dressing_queue_update (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PALETTE]);
    }
}

