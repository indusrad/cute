/*
 * prompt-palette-preview-color.c
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

#include "prompt-palette.h"
#include "prompt-palette-preview-color.h"

struct _PromptPalettePreviewColor
{
  GtkWidget parent_instance;
  PromptPalette *palette;
  guint index;
  guint dark : 1;
};

enum {
  PROP_0,
  PROP_DARK,
  PROP_INDEX,
  PROP_PALETTE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PromptPalettePreviewColor, prompt_palette_preview_color, GTK_TYPE_WIDGET)

static GParamSpec *properties [N_PROPS];

static void
prompt_palette_preview_color_snapshot (GtkWidget   *widget,
                                       GtkSnapshot *snapshot)
{
  PromptPalettePreviewColor *self = (PromptPalettePreviewColor *)widget;
  const PromptPaletteFace *face;
  const GdkRGBA *color;
  int width;
  int height;

  g_assert (PROMPT_IS_PALETTE_PREVIEW_COLOR (self));
  g_assert (!self->palette || PROMPT_IS_PALETTE (self->palette));
  g_assert (self->index <= 15);

  if (self->palette == NULL)
    return;

  face = prompt_palette_get_face (self->palette, self->dark);
  color = &face->indexed[self->index];
  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  gtk_snapshot_append_color (snapshot,
                             color,
                             &GRAPHENE_RECT_INIT (0, 0, width, height));
}

static void
prompt_palette_preview_color_dispose (GObject *object)
{
  PromptPalettePreviewColor *self = (PromptPalettePreviewColor *)object;

  g_clear_object (&self->palette);

  G_OBJECT_CLASS (prompt_palette_preview_color_parent_class)->dispose (object);
}

static void
prompt_palette_preview_color_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  PromptPalettePreviewColor *self = PROMPT_PALETTE_PREVIEW_COLOR (object);

  switch (prop_id)
    {
    case PROP_DARK:
      g_value_set_boolean (value, self->dark);
      break;

    case PROP_INDEX:
      g_value_set_uint (value, self->index);
      break;

    case PROP_PALETTE:
      g_value_set_object (value, self->palette);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_palette_preview_color_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  PromptPalettePreviewColor *self = PROMPT_PALETTE_PREVIEW_COLOR (object);

  switch (prop_id)
    {
    case PROP_DARK:
      self->dark = g_value_get_boolean (value);
      gtk_widget_queue_draw (GTK_WIDGET (self));
      break;

    case PROP_INDEX:
      self->index = g_value_get_uint (value);
      gtk_widget_queue_draw (GTK_WIDGET (self));
      break;

    case PROP_PALETTE:
      if (g_set_object (&self->palette, g_value_get_object (value)))
        gtk_widget_queue_draw (GTK_WIDGET (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_palette_preview_color_class_init (PromptPalettePreviewColorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = prompt_palette_preview_color_dispose;
  object_class->get_property = prompt_palette_preview_color_get_property;
  object_class->set_property = prompt_palette_preview_color_set_property;

  widget_class->snapshot = prompt_palette_preview_color_snapshot;

  properties[PROP_DARK] =
    g_param_spec_boolean ("dark", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_INDEX] =
    g_param_spec_uint ("index", NULL, NULL,
                       0, 15, 0,
                       (G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_PALETTE] =
    g_param_spec_object ("palette", NULL, NULL,
                         PROMPT_TYPE_PALETTE,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "palettepreviewcolor");
}

static void
prompt_palette_preview_color_init (PromptPalettePreviewColor *self)
{
  gtk_widget_set_overflow (GTK_WIDGET (self), GTK_OVERFLOW_HIDDEN);
}
