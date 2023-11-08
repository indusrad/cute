/* capsule-window.h
 *
 * Copyright 2023 Christian Hergert
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

#include <adwaita.h>

#include "capsule-profile.h"
#include "capsule-tab.h"

G_BEGIN_DECLS

#define CAPSULE_TYPE_WINDOW (capsule_window_get_type())

G_DECLARE_FINAL_TYPE (CapsuleWindow, capsule_window, CAPSULE, WINDOW, AdwApplicationWindow)

CapsuleWindow  *capsule_window_new                (void);
CapsuleWindow  *capsule_window_new_for_profile    (CapsuleProfile *profile);
void            capsule_window_add_tab            (CapsuleWindow  *self,
                                                   CapsuleTab     *tab);
void            capsule_window_append_tab         (CapsuleWindow  *self,
                                                   CapsuleTab     *tab);
CapsuleProfile *capsule_window_get_active_profile (CapsuleWindow  *self);
CapsuleTab     *capsule_window_get_active_tab     (CapsuleWindow  *self);
void            capsule_window_set_active_tab     (CapsuleWindow  *self,
                                                   CapsuleTab     *active_tab);
void            capsule_window_visual_bell        (CapsuleWindow  *self);
void            capsule_window_confirm_close      (CapsuleWindow  *self,
                                                   CapsuleTab     *tab,
                                                   gboolean        confirm);

G_END_DECLS
