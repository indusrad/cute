/*
 * capsule-window-dressing.h
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

#include "capsule-palette.h"
#include "capsule-window.h"

G_BEGIN_DECLS

#define CAPSULE_TYPE_WINDOW_DRESSING (capsule_window_dressing_get_type())

G_DECLARE_FINAL_TYPE (CapsuleWindowDressing, capsule_window_dressing, CAPSULE, WINDOW_DRESSING, GObject)

CapsuleWindowDressing *capsule_window_dressing_new         (CapsuleWindow         *window);
CapsuleWindow         *capsule_window_dressing_dup_window  (CapsuleWindowDressing *self);
CapsulePalette        *capsule_window_dressing_get_palette (CapsuleWindowDressing *self);
void                   capsule_window_dressing_set_palette (CapsuleWindowDressing *self,
                                                            CapsulePalette        *palette);

G_END_DECLS

