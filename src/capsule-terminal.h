/*
 * capsule-terminal.h
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

#include <vte/vte.h>

#include "capsule-palette.h"

G_BEGIN_DECLS

#define CAPSULE_TYPE_TERMINAL (capsule_terminal_get_type())

G_DECLARE_FINAL_TYPE (CapsuleTerminal, capsule_terminal, CAPSULE, TERMINAL, VteTerminal)

CapsulePalette *capsule_terminal_get_palette (CapsuleTerminal *self);
void            capsule_terminal_set_palette (CapsuleTerminal *self,
                                              CapsulePalette  *palette);

G_END_DECLS
