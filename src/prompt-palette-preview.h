/* prompt-palette-preview.h
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

#include <gtk/gtk.h>

#include "prompt-palette.h"

G_BEGIN_DECLS

#define PROMPT_TYPE_PALETTE_PREVIEW (prompt_palette_preview_get_type())

G_DECLARE_FINAL_TYPE (PromptPalettePreview, prompt_palette_preview, PROMPT, PALETTE_PREVIEW, GtkWidget)

GtkWidget     *prompt_palette_preview_new         (PromptPalette        *palette);
PromptPalette *prompt_palette_preview_get_palette (PromptPalettePreview *self);
gboolean       prompt_palette_preview_get_dark    (PromptPalettePreview *self);
void           prompt_palette_preview_set_dark    (PromptPalettePreview *self,
                                                   gboolean              dark);

G_END_DECLS
