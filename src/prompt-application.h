/* prompt-application.h
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

#include "prompt-container.h"
#include "prompt-profile.h"
#include "prompt-settings.h"
#include "prompt-shortcuts.h"

G_BEGIN_DECLS

#define PROMPT_TYPE_APPLICATION    (prompt_application_get_type())
#define PROMPT_APPLICATION_DEFAULT (PROMPT_APPLICATION(g_application_get_default()))

G_DECLARE_FINAL_TYPE (PromptApplication, prompt_application, PROMPT, APPLICATION, AdwApplication)

PromptApplication *prompt_application_new                  (const char        *application_id,
                                                            GApplicationFlags  flags);
PromptSettings    *prompt_application_get_settings         (PromptApplication *self);
PromptShortcuts   *prompt_application_get_shortcuts        (PromptApplication *self);
const char        *prompt_application_get_system_font_name (PromptApplication *self);
gboolean           prompt_application_control_is_pressed   (PromptApplication *self);
void               prompt_application_add_profile          (PromptApplication *self,
                                                            PromptProfile     *profile);
void               prompt_application_remove_profile       (PromptApplication *self,
                                                            PromptProfile     *profile);
PromptProfile     *prompt_application_dup_default_profile  (PromptApplication *self);
void               prompt_application_set_default_profile  (PromptApplication *self,
                                                            PromptProfile     *profile);
PromptProfile     *prompt_application_dup_profile          (PromptApplication *self,
                                                            const char        *profile_uuid);
GMenuModel        *prompt_application_dup_profile_menu     (PromptApplication *self);
GListModel        *prompt_application_list_profiles        (PromptApplication *self);
GListModel        *prompt_application_list_containers      (PromptApplication *self);
PromptContainer   *prompt_application_lookup_container     (PromptApplication *self,
                                                            const char        *container_id);
void               prompt_application_report_error         (PromptApplication *self,
                                                            GType              subsystem,
                                                            const GError      *error);

G_END_DECLS
