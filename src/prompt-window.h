/* prompt-window.h
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

#include "prompt-profile.h"
#include "prompt-tab.h"

G_BEGIN_DECLS

#define PROMPT_TYPE_WINDOW (prompt_window_get_type())

G_DECLARE_FINAL_TYPE (PromptWindow, prompt_window, PROMPT, WINDOW, AdwApplicationWindow)

PromptWindow  *prompt_window_new                (void);
PromptWindow  *prompt_window_new_for_command    (const char * const *argv,
                                                 const char         *cwd_uri);
PromptWindow  *prompt_window_new_for_profile    (PromptProfile      *profile);
void           prompt_window_add_tab            (PromptWindow       *self,
                                                 PromptTab          *tab);
GListModel    *prompt_window_list_pages         (PromptWindow       *self);
void           prompt_window_append_tab         (PromptWindow       *self,
                                                 PromptTab          *tab);
PromptProfile *prompt_window_get_active_profile (PromptWindow       *self);
PromptTab     *prompt_window_get_active_tab     (PromptWindow       *self);
void           prompt_window_set_active_tab     (PromptWindow       *self,
                                                 PromptTab          *active_tab);
void           prompt_window_visual_bell        (PromptWindow       *self);
gboolean       prompt_window_focus_tab_by_uuid  (PromptWindow       *self,
                                                 const char         *uuid);
gboolean       prompt_window_is_animating       (PromptWindow       *self);

G_END_DECLS
