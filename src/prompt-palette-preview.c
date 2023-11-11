/* prompt-palette-preview.c
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

#include "prompt-palette-preview.h"

#define CELL_WIDTH 12
#define CELL_HEIGHT 12
#define ROW_SPACING 3
#define COLUMN_SPACING 3
#define VPADDING 9
#define HPADDING 9

struct _PromptPalettePreview
{
  GtkWidget parent_instance;
  PromptPalette *palette;
  guint dark : 1;
};

enum {
  PROP_0,
  PROP_DARK,
  PROP_PALETTE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PromptPalettePreview, prompt_palette_preview, GTK_TYPE_WIDGET)

static GParamSpec *properties [N_PROPS];

static void
prompt_palette_preview_snapshot (GtkWidget   *widget,
                                 GtkSnapshot *snapshot)
{
  PromptPalettePreview *self = (PromptPalettePreview *)widget;
  const PromptPaletteFace *face;
  int width;
  int height;

  g_assert (PROMPT_IS_PALETTE_PREVIEW (self));
  g_assert (GTK_IS_SNAPSHOT (snapshot));

  if (!self->palette || !(face = prompt_palette_get_face (self->palette, self->dark)))
    return;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  gtk_snapshot_append_color (snapshot,
                             &face->background,
                             &GRAPHENE_RECT_INIT (0, 0, width, height));

  for (guint i = 0; i < 16; i++)
    {
      gtk_snapshot_append_color (snapshot,
                                 &face->indexed[i],
                                 &GRAPHENE_RECT_INIT (HPADDING + (CELL_WIDTH + COLUMN_SPACING) * (i % 8),
                                                      VPADDING + (CELL_HEIGHT + ROW_SPACING) * (i >= 8),
                                                      CELL_WIDTH,
                                                      CELL_HEIGHT));
    }
}

static gboolean
prompt_palette_preview_query_tooltip (GtkWidget  *widget,
                                      int         x,
                                      int         y,
                                      gboolean    keyboard_mode,
                                      GtkTooltip *tooltip)
{
  PromptPalettePreview *self = (PromptPalettePreview *)widget;

  g_assert (PROMPT_IS_PALETTE_PREVIEW (self));

  if (self->palette == NULL)
    return FALSE;

  gtk_tooltip_set_text (tooltip, prompt_palette_get_name (self->palette));

  return TRUE;
}

static void
prompt_palette_preview_finalize (GObject *object)
{
  PromptPalettePreview *self = (PromptPalettePreview *)object;

  g_clear_object (&self->palette);

  G_OBJECT_CLASS (prompt_palette_preview_parent_class)->finalize (object);
}

static void
prompt_palette_preview_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  PromptPalettePreview *self = PROMPT_PALETTE_PREVIEW (object);

  switch (prop_id)
    {
    case PROP_PALETTE:
      g_value_set_object (value, self->palette);
      break;

    case PROP_DARK:
      g_value_set_boolean (value, prompt_palette_preview_get_dark (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_palette_preview_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  PromptPalettePreview *self = PROMPT_PALETTE_PREVIEW (object);

  switch (prop_id)
    {
    case PROP_PALETTE:
      self->palette = g_value_dup_object (value);
      break;

    case PROP_DARK:
      prompt_palette_preview_set_dark (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_palette_preview_class_init (PromptPalettePreviewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = prompt_palette_preview_finalize;
  object_class->get_property = prompt_palette_preview_get_property;
  object_class->set_property = prompt_palette_preview_set_property;

  widget_class->snapshot = prompt_palette_preview_snapshot;
  widget_class->query_tooltip = prompt_palette_preview_query_tooltip;

  properties[PROP_PALETTE] =
    g_param_spec_object ("palette", NULL, NULL,
                         PROMPT_TYPE_PALETTE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_DARK] =
    g_param_spec_boolean ("dark", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
prompt_palette_preview_init (PromptPalettePreview *self)
{
  gtk_widget_set_halign (GTK_WIDGET (self), GTK_ALIGN_CENTER);
  gtk_widget_set_has_tooltip (GTK_WIDGET (self), TRUE);
  gtk_widget_set_size_request (GTK_WIDGET (self),
                               (HPADDING * 2) + (8 * CELL_WIDTH) + (7 * COLUMN_SPACING),
                               (VPADDING * 2) + (2 * CELL_HEIGHT) + (ROW_SPACING));

}

GtkWidget *
prompt_palette_preview_new (PromptPalette *palette)
{
  g_return_val_if_fail (PROMPT_IS_PALETTE (palette), NULL);

  return g_object_new (PROMPT_TYPE_PALETTE_PREVIEW,
                       "palette", palette,
                       NULL);
}

gboolean
prompt_palette_preview_get_dark (PromptPalettePreview *self)
{
  g_return_val_if_fail (PROMPT_IS_PALETTE_PREVIEW (self), FALSE);

  return self->dark;
}

void
prompt_palette_preview_set_dark (PromptPalettePreview *self,
                                 gboolean              dark)
{
  g_return_if_fail (PROMPT_IS_PALETTE_PREVIEW (self));

  dark = !!dark;

  if (dark != self->dark)
    {
      self->dark = dark;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DARK]);
      gtk_widget_queue_draw (GTK_WIDGET (self));
    }
}
