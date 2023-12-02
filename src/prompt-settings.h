/*
 * prompt-settings.h
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

#include <adwaita.h>
#include <vte/vte.h>

G_BEGIN_DECLS

#define PROMPT_SETTING_KEY_AUDIBLE_BELL         "audible-bell"
#define PROMPT_SETTING_KEY_CURSOR_BLINK_MODE    "cursor-blink-mode"
#define PROMPT_SETTING_KEY_CURSOR_SHAPE         "cursor-shape"
#define PROMPT_SETTING_KEY_DEFAULT_PROFILE_UUID "default-profile-uuid"
#define PROMPT_SETTING_KEY_FONT_NAME            "font-name"
#define PROMPT_SETTING_KEY_INTERFACE_STYLE      "interface-style"
#define PROMPT_SETTING_KEY_NEW_TAB_POSITION     "new-tab-position"
#define PROMPT_SETTING_KEY_PROFILE_UUIDS        "profile-uuids"
#define PROMPT_SETTING_KEY_RESTORE_WINDOW_SIZE  "restore-window-size"
#define PROMPT_SETTING_KEY_SCROLLBAR_POLICY     "scrollbar-policy"
#define PROMPT_SETTING_KEY_TEXT_BLINK_MODE      "text-blink-mode"
#define PROMPT_SETTING_KEY_USE_SYSTEM_FONT      "use-system-font"
#define PROMPT_SETTING_KEY_VISUAL_BELL          "visual-bell"

typedef enum _PromptNewTabPosition
{
  PROMPT_NEW_TAB_POSITION_LAST = 0,
  PROMPT_NEW_TAB_POSITION_NEXT,
} PromptNewTabPosition;

typedef enum _PromptScrollbarPolicy
{
  PROMPT_SCROLLBAR_POLICY_NEVER  = 0,
  PROMPT_SCROLLBAR_POLICY_SYSTEM = 1,
  PROMPT_SCROLLBAR_POLICY_ALWAYS = 2,
} PromptScrollbarPolicy;

#define PROMPT_TYPE_SETTINGS (prompt_settings_get_type())

G_DECLARE_FINAL_TYPE (PromptSettings, prompt_settings, PROMPT, SETTINGS, GObject)

PromptSettings         *prompt_settings_new                      (void);
GSettings              *prompt_settings_get_settings             (PromptSettings             *self);
char                   *prompt_settings_dup_default_profile_uuid (PromptSettings             *self);
void                    prompt_settings_set_default_profile_uuid (PromptSettings             *self,
                                                                  const char                 *uuid);
char                  **prompt_settings_dup_profile_uuids        (PromptSettings             *self);
void                    prompt_settings_add_profile_uuid         (PromptSettings             *self,
                                                                  const char                 *uuid);
void                    prompt_settings_remove_profile_uuid      (PromptSettings             *self,
                                                                  const char                 *uuid);
PromptNewTabPosition    prompt_settings_get_new_tab_position     (PromptSettings             *self);
void                    prompt_settings_set_new_tab_position     (PromptSettings             *self,
                                                                  PromptNewTabPosition        new_tab_position);
gboolean                prompt_settings_get_audible_bell         (PromptSettings             *self);
void                    prompt_settings_set_audible_bell         (PromptSettings             *self,
                                                                  gboolean                    audible_bell);
gboolean                prompt_settings_get_visual_bell          (PromptSettings             *self);
void                    prompt_settings_set_visual_bell          (PromptSettings             *self,
                                                                  gboolean                    visual_bell);
VteCursorBlinkMode      prompt_settings_get_cursor_blink_mode    (PromptSettings             *self);
void                    prompt_settings_set_cursor_blink_mode    (PromptSettings             *self,
                                                                  VteCursorBlinkMode          blink_mode);
VteCursorShape          prompt_settings_get_cursor_shape         (PromptSettings             *self);
void                    prompt_settings_set_cursor_shape         (PromptSettings             *self,
                                                                  VteCursorShape              cursor_shape);
PangoFontDescription   *prompt_settings_dup_font_desc            (PromptSettings             *self);
void                    prompt_settings_set_font_desc            (PromptSettings             *self,
                                                                  const PangoFontDescription *font_desc);
char                   *prompt_settings_dup_font_name            (PromptSettings             *self);
void                    prompt_settings_set_font_name            (PromptSettings             *self,
                                                                  const char                 *font_name);
gboolean                prompt_settings_get_use_system_font      (PromptSettings             *self);
void                    prompt_settings_set_use_system_font      (PromptSettings             *self,
                                                                  gboolean                    use_system_font);
gboolean                prompt_settings_get_restore_window_size  (PromptSettings             *self);
void                    prompt_settings_set_restore_window_size  (PromptSettings             *self,
                                                                  gboolean                    restore_window_size);
PromptScrollbarPolicy   prompt_settings_get_scrollbar_policy     (PromptSettings             *self);
void                    prompt_settings_set_scrollbar_policy     (PromptSettings             *self,
                                                                  PromptScrollbarPolicy       scrollbar_policy);
VteTextBlinkMode        prompt_settings_get_text_blink_mode      (PromptSettings             *self);
void                    prompt_settings_set_text_blink_mode      (PromptSettings             *self,
                                                                  VteTextBlinkMode            text_blink_mode);
void                    prompt_settings_get_window_size          (PromptSettings             *self,
                                                                  guint                      *columns,
                                                                  guint                      *rows);
void                    prompt_settings_set_window_size          (PromptSettings             *self,
                                                                  guint                       columns,
                                                                  guint                       rows);
AdwColorScheme          prompt_settings_get_interface_style      (PromptSettings             *self);
void                    prompt_settings_set_interface_style      (PromptSettings             *self,
                                                                  AdwColorScheme              color_scheme);
char                  **prompt_settings_get_proxy_environment    (PromptSettings             *self);

G_END_DECLS
