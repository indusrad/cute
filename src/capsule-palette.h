/*
 * capsule-palette.h
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

#define CAPSULE_TYPE_PALETTE (capsule_palette_get_type())

G_DECLARE_FINAL_TYPE (CapsulePalette, capsule_palette, CAPSULE, PALETTE, GObject)

CapsulePalette *capsule_palette_new_from_name      (const char     *name);
const char     *capsule_palette_get_id             (CapsulePalette *self);
const char     *capsule_palette_get_name           (CapsulePalette *self);
const GdkRGBA  *capsule_palette_get_background     (CapsulePalette *self,
                                                    gboolean        dark);
const GdkRGBA  *capsule_palette_get_foreground     (CapsulePalette *self,
                                                    gboolean        dark);
const GdkRGBA  *capsule_palette_get_indexed_colors (CapsulePalette *self,
                                                    guint          *n_colors);

G_END_DECLS
