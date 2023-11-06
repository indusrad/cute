/*
 * capsule-settings.h
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

G_BEGIN_DECLS

#define CAPSULE_SETTING_KEY_AUDIBLE_BELL         "audible-bell"
#define CAPSULE_SETTING_KEY_CURSOR_BLINK_MODE    "cursor-blink-mode"
#define CAPSULE_SETTING_KEY_CURSOR_SHAPE         "cursor-shape"
#define CAPSULE_SETTING_KEY_DEFAULT_PROFILE_UUID "default-profile-uuid"
#define CAPSULE_SETTING_KEY_FONT_NAME            "font-name"
#define CAPSULE_SETTING_KEY_NEW_TAB_POSITION     "new-tab-position"
#define CAPSULE_SETTING_KEY_PROFILE_UUIDS        "profile-uuids"
#define CAPSULE_SETTING_KEY_SCROLLBAR_POLICY     "scrollbar-policy"
#define CAPSULE_SETTING_KEY_USE_SYSTEM_FONT      "use-system-font"
#define CAPSULE_SETTING_KEY_VISUAL_BELL          "visual-bell"

typedef enum _CapsuleNewTabPosition
{
  CAPSULE_NEW_TAB_POSITION_LAST = 0,
  CAPSULE_NEW_TAB_POSITION_NEXT,
} CapsuleNewTabPosition;

typedef enum _CapsuleScrollbarPolicy
{
  CAPSULE_SCROLLBAR_POLICY_NEVER  = 0,
  CAPSULE_SCROLLBAR_POLICY_SYSTEM = 1,
  CAPSULE_SCROLLBAR_POLICY_ALWAYS = 2,
} CapsuleScrollbarPolicy;

#define CAPSULE_TYPE_SETTINGS (capsule_settings_get_type())

G_DECLARE_FINAL_TYPE (CapsuleSettings, capsule_settings, CAPSULE, SETTINGS, GObject)

CapsuleSettings        *capsule_settings_new                      (void);
GSettings              *capsule_settings_get_settings             (CapsuleSettings            *self);
char                   *capsule_settings_dup_default_profile_uuid (CapsuleSettings            *self);
void                    capsule_settings_set_default_profile_uuid (CapsuleSettings            *self,
                                                                   const char                 *uuid);
char                  **capsule_settings_dup_profile_uuids        (CapsuleSettings            *self);
void                    capsule_settings_add_profile_uuid         (CapsuleSettings            *self,
                                                                   const char                 *uuid);
void                    capsule_settings_remove_profile_uuid      (CapsuleSettings            *self,
                                                                   const char                 *uuid);
CapsuleNewTabPosition   capsule_settings_get_new_tab_position     (CapsuleSettings            *self);
void                    capsule_settings_set_new_tab_position     (CapsuleSettings            *self,
                                                                   CapsuleNewTabPosition       new_tab_position);
gboolean                capsule_settings_get_audible_bell         (CapsuleSettings            *self);
void                    capsule_settings_set_audible_bell         (CapsuleSettings            *self,
                                                                   gboolean                    audible_bell);
gboolean                capsule_settings_get_visual_bell          (CapsuleSettings            *self);
void                    capsule_settings_set_visual_bell          (CapsuleSettings            *self,
                                                                   gboolean                    visual_bell);
VteCursorBlinkMode      capsule_settings_get_cursor_blink_mode    (CapsuleSettings            *self);
void                    capsule_settings_set_cursor_blink_mode    (CapsuleSettings            *self,
                                                                   VteCursorBlinkMode          blink_mode);
VteCursorShape          capsule_settings_get_cursor_shape         (CapsuleSettings            *self);
void                    capsule_settings_set_cursor_shape         (CapsuleSettings            *self,
                                                                   VteCursorShape              cursor_shape);
PangoFontDescription   *capsule_settings_dup_font_desc            (CapsuleSettings            *self);
void                    capsule_settings_set_font_desc            (CapsuleSettings            *self,
                                                                   const PangoFontDescription *font_desc);
char                   *capsule_settings_dup_font_name            (CapsuleSettings            *self);
void                    capsule_settings_set_font_name            (CapsuleSettings            *self,
                                                                   const char                 *font_name);
gboolean                capsule_settings_get_use_system_font      (CapsuleSettings            *self);
void                    capsule_settings_set_use_system_font      (CapsuleSettings            *self,
                                                                   gboolean                    use_system_font);
CapsuleScrollbarPolicy  capsule_settings_get_scrollbar_policy     (CapsuleSettings            *self);
void                    capsule_settings_set_scrollbar_policy     (CapsuleSettings            *self,
                                                                   CapsuleScrollbarPolicy      scrollbar_policy);

G_END_DECLS
