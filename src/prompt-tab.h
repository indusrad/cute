/*
 * prompt-tab.h
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

#include "prompt-process.h"
#include "prompt-profile.h"
#include "prompt-terminal.h"

G_BEGIN_DECLS

typedef enum _PromptZoomLevel
{
  PROMPT_ZOOM_LEVEL_MINUS_7 = 1,
  PROMPT_ZOOM_LEVEL_MINUS_6,
  PROMPT_ZOOM_LEVEL_MINUS_5,
  PROMPT_ZOOM_LEVEL_MINUS_4,
  PROMPT_ZOOM_LEVEL_MINUS_3,
  PROMPT_ZOOM_LEVEL_MINUS_2,
  PROMPT_ZOOM_LEVEL_MINUS_1,
  PROMPT_ZOOM_LEVEL_DEFAULT,
  PROMPT_ZOOM_LEVEL_PLUS_1,
  PROMPT_ZOOM_LEVEL_PLUS_2,
  PROMPT_ZOOM_LEVEL_PLUS_3,
  PROMPT_ZOOM_LEVEL_PLUS_4,
  PROMPT_ZOOM_LEVEL_PLUS_5,
  PROMPT_ZOOM_LEVEL_PLUS_6,
  PROMPT_ZOOM_LEVEL_PLUS_7,
} PromptZoomLevel;

#define PROMPT_TYPE_TAB (prompt_tab_get_type())

G_DECLARE_FINAL_TYPE (PromptTab, prompt_tab, PROMPT, TAB, GtkWidget)

PromptTab       *prompt_tab_new                                (PromptProfile   *profile);
PromptTerminal  *prompt_tab_get_terminal                       (PromptTab       *self);
PromptProfile   *prompt_tab_get_profile                        (PromptTab       *self);
PromptProcess   *prompt_tab_get_process                        (PromptTab       *self);
char            *prompt_tab_dup_subtitle                       (PromptTab       *self);
char            *prompt_tab_dup_title                          (PromptTab       *self);
const char      *prompt_tab_get_title_prefix                   (PromptTab       *self);
void             prompt_tab_set_title_prefix                   (PromptTab       *self,
                                                                const char      *title_prefix);
const char      *prompt_tab_get_current_directory_uri          (PromptTab       *self);
void             prompt_tab_set_previous_working_directory_uri (PromptTab       *self,
                                                                const char      *previous_working_directory_uri);
PromptZoomLevel  prompt_tab_get_zoom                           (PromptTab       *self);
void             prompt_tab_set_zoom                           (PromptTab       *self,
                                                                PromptZoomLevel  zoom);
void             prompt_tab_zoom_in                            (PromptTab       *self);
void             prompt_tab_zoom_out                           (PromptTab       *self);
char            *prompt_tab_dup_zoom_label                     (PromptTab       *self);
void             prompt_tab_raise                              (PromptTab       *self);
gboolean         prompt_tab_is_running                         (PromptTab       *self);
void             prompt_tab_force_quit                         (PromptTab       *self);

G_END_DECLS
