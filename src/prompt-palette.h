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

G_DECLARE_FINAL_TYPE (PromptPalette, prompt_palette, PROMPT, PALETTE, GObject)

GListModel     *prompt_palette_list_model_get_default (void);

PromptPalette *prompt_palette_new_from_name      (const char    *name);
const char    *prompt_palette_get_id             (PromptPalette *self);
const char    *prompt_palette_get_name           (PromptPalette *self);
const GdkRGBA *prompt_palette_get_background     (PromptPalette *self,
                                                  gboolean       dark);
const GdkRGBA *prompt_palette_get_foreground     (PromptPalette *self,
                                                  gboolean       dark);
const GdkRGBA *prompt_palette_get_indexed_colors (PromptPalette *self,
                                                  guint         *n_colors);

G_END_DECLS
