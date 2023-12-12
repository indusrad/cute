/*
 * prompt-palette.h
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

#pragma once

#include <gdk/gdk.h>

G_BEGIN_DECLS

#define PROMPT_TYPE_PALETTE (prompt_palette_get_type())

typedef struct _PromptPaletteScarf
{
  GdkRGBA foreground;
  GdkRGBA background;
} PromptPaletteScarf;

#define PROMPT_PALETTE_SCARF_VISUAL_BELL 0
#define PROMPT_PALETTE_SCARF_SUPERUSER   1
#define PROMPT_PALETTE_SCARF_REMOTE      2
#define PROMPT_PALETTE_N_SCARVES         3

typedef struct _PromptPaletteFace
{
  GdkRGBA background;
  GdkRGBA foreground;
  GdkRGBA titlebar_background;
  GdkRGBA titlebar_foreground;
  GdkRGBA cursor;
  GdkRGBA indexed[16];
  union {
    PromptPaletteScarf scarves[PROMPT_PALETTE_N_SCARVES];
    struct {
      PromptPaletteScarf visual_bell;
      PromptPaletteScarf superuser;
      PromptPaletteScarf remote;
    };
  };
} PromptPaletteFace;

G_DECLARE_FINAL_TYPE (PromptPalette, prompt_palette, PROMPT, PALETTE, GObject)

GListModel              *prompt_palette_get_all                (void);
GListModel              *prompt_palette_list_model_get_default (void);
PromptPalette           *prompt_palette_lookup                 (const char     *name);
PromptPalette           *prompt_palette_new_from_file          (const char     *file,
                                                                GError        **error);
PromptPalette           *prompt_palette_new_from_resource      (const char     *file,
                                                                GError        **error);
const char              *prompt_palette_get_id                 (PromptPalette  *self);
const char              *prompt_palette_get_name               (PromptPalette  *self);
const PromptPaletteFace *prompt_palette_get_face               (PromptPalette  *self,
                                                                gboolean        dark);
gboolean                 prompt_palette_use_adwaita            (PromptPalette  *self);
gboolean                 prompt_palette_is_primary             (PromptPalette  *self);

G_END_DECLS
