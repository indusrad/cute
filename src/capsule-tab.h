/*
 * capsule-tab.h
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

#include "capsule-profile.h"

G_BEGIN_DECLS

typedef enum _CapsuleZoomLevel
{
  CAPSULE_ZOOM_LEVEL_MINUS_7 = 1,
  CAPSULE_ZOOM_LEVEL_MINUS_6,
  CAPSULE_ZOOM_LEVEL_MINUS_5,
  CAPSULE_ZOOM_LEVEL_MINUS_4,
  CAPSULE_ZOOM_LEVEL_MINUS_3,
  CAPSULE_ZOOM_LEVEL_MINUS_2,
  CAPSULE_ZOOM_LEVEL_MINUS_1,
  CAPSULE_ZOOM_LEVEL_DEFAULT,
  CAPSULE_ZOOM_LEVEL_PLUS_1,
  CAPSULE_ZOOM_LEVEL_PLUS_2,
  CAPSULE_ZOOM_LEVEL_PLUS_3,
  CAPSULE_ZOOM_LEVEL_PLUS_4,
  CAPSULE_ZOOM_LEVEL_PLUS_5,
  CAPSULE_ZOOM_LEVEL_PLUS_6,
  CAPSULE_ZOOM_LEVEL_PLUS_7,
} CapsuleZoomLevel;

#define CAPSULE_TYPE_TAB (capsule_tab_get_type())

G_DECLARE_FINAL_TYPE (CapsuleTab, capsule_tab, CAPSULE, TAB, GtkWidget)

CapsuleTab       *capsule_tab_new                                (CapsuleProfile   *profile);
CapsuleProfile   *capsule_tab_get_profile                        (CapsuleTab       *self);
char             *capsule_tab_dup_subtitle                       (CapsuleTab       *self);
char             *capsule_tab_dup_title                          (CapsuleTab       *self);
const char       *capsule_tab_get_title_prefix                   (CapsuleTab       *self);
void              capsule_tab_set_title_prefix                   (CapsuleTab       *self,
                                                                  const char       *title_prefix);
const char       *capsule_tab_get_current_directory_uri          (CapsuleTab       *self);
void              capsule_tab_set_previous_working_directory_uri (CapsuleTab       *self,
                                                                  const char       *previous_working_directory_uri);
CapsuleZoomLevel  capsule_tab_get_zoom                           (CapsuleTab       *self);
void              capsule_tab_set_zoom                           (CapsuleTab       *self,
                                                                  CapsuleZoomLevel  zoom);
void              capsule_tab_zoom_in                            (CapsuleTab       *self);
void              capsule_tab_zoom_out                           (CapsuleTab       *self);

G_END_DECLS
