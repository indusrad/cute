/*
 * ptyxis-settings.h
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

#define PTYXIS_SETTING_KEY_AUDIBLE_BELL            "audible-bell"
#define PTYXIS_SETTING_KEY_CURSOR_BLINK_MODE       "cursor-blink-mode"
#define PTYXIS_SETTING_KEY_CURSOR_SHAPE            "cursor-shape"
#define PTYXIS_SETTING_KEY_DEFAULT_PROFILE_UUID    "default-profile-uuid"
#define PTYXIS_SETTING_KEY_ENABLE_A11Y             "enable-a11y"
#define PTYXIS_SETTING_KEY_FONT_NAME               "font-name"
#define PTYXIS_SETTING_KEY_INTERFACE_STYLE         "interface-style"
#define PTYXIS_SETTING_KEY_NEW_TAB_POSITION        "new-tab-position"
#define PTYXIS_SETTING_KEY_PROFILE_UUIDS           "profile-uuids"
#define PTYXIS_SETTING_KEY_RESTORE_SESSION         "restore-session"
#define PTYXIS_SETTING_KEY_RESTORE_WINDOW_SIZE     "restore-window-size"
#define PTYXIS_SETTING_KEY_DEFAULT_COLUMNS         "default-columns"
#define PTYXIS_SETTING_KEY_DEFAULT_ROWS            "default-rows"
#define PTYXIS_SETTING_KEY_SCROLLBAR_POLICY        "scrollbar-policy"
#define PTYXIS_SETTING_KEY_TEXT_BLINK_MODE         "text-blink-mode"
#define PTYXIS_SETTING_KEY_TOAST_ON_COPY_CLIPBOARD "toast-on-copy-clipboard"
#define PTYXIS_SETTING_KEY_USE_SYSTEM_FONT         "use-system-font"
#define PTYXIS_SETTING_KEY_VISUAL_BELL             "visual-bell"
#define PTYXIS_SETTING_KEY_VISUAL_PROCESS_LEADER   "visual-process-leader"
#define PTYXIS_SETTING_KEY_DISABLE_PADDING         "disable-padding"

typedef enum _PtyxisNewTabPosition
{
  PTYXIS_NEW_TAB_POSITION_LAST = 0,
  PTYXIS_NEW_TAB_POSITION_NEXT,
} PtyxisNewTabPosition;

typedef enum _PtyxisScrollbarPolicy
{
  PTYXIS_SCROLLBAR_POLICY_NEVER  = 0,
  PTYXIS_SCROLLBAR_POLICY_SYSTEM = 1,
  PTYXIS_SCROLLBAR_POLICY_ALWAYS = 2,
} PtyxisScrollbarPolicy;

#define PTYXIS_TYPE_SETTINGS (ptyxis_settings_get_type())

G_DECLARE_FINAL_TYPE (PtyxisSettings, ptyxis_settings, PTYXIS, SETTINGS, GObject)

PtyxisSettings         *ptyxis_settings_new                         (void);
GSettings              *ptyxis_settings_get_settings                (PtyxisSettings             *self);
char                   *ptyxis_settings_dup_default_profile_uuid    (PtyxisSettings             *self);
void                    ptyxis_settings_set_default_profile_uuid    (PtyxisSettings             *self,
                                                                     const char                 *uuid);
char                  **ptyxis_settings_dup_profile_uuids           (PtyxisSettings             *self);
void                    ptyxis_settings_add_profile_uuid            (PtyxisSettings             *self,
                                                                     const char                 *uuid);
void                    ptyxis_settings_remove_profile_uuid         (PtyxisSettings             *self,
                                                                     const char                 *uuid);
PtyxisNewTabPosition    ptyxis_settings_get_new_tab_position        (PtyxisSettings             *self);
void                    ptyxis_settings_set_new_tab_position        (PtyxisSettings             *self,
                                                                     PtyxisNewTabPosition        new_tab_position);
gboolean                ptyxis_settings_get_enable_a11y             (PtyxisSettings             *self);
void                    ptyxis_settings_set_enable_a11y             (PtyxisSettings             *self,
                                                                     gboolean                    enable_a11y);
gboolean                ptyxis_settings_get_audible_bell            (PtyxisSettings             *self);
void                    ptyxis_settings_set_audible_bell            (PtyxisSettings             *self,
                                                                     gboolean                    audible_bell);
gboolean                ptyxis_settings_get_visual_bell             (PtyxisSettings             *self);
void                    ptyxis_settings_set_visual_bell             (PtyxisSettings             *self,
                                                                     gboolean                    visual_bell);
gboolean                ptyxis_settings_get_visual_process_leader   (PtyxisSettings             *self);
void                    ptyxis_settings_set_visual_process_leader   (PtyxisSettings             *self,
                                                                     gboolean                    visual_process_leader);
VteCursorBlinkMode      ptyxis_settings_get_cursor_blink_mode       (PtyxisSettings             *self);
void                    ptyxis_settings_set_cursor_blink_mode       (PtyxisSettings             *self,
                                                                     VteCursorBlinkMode          blink_mode);
VteCursorShape          ptyxis_settings_get_cursor_shape            (PtyxisSettings             *self);
void                    ptyxis_settings_set_cursor_shape            (PtyxisSettings             *self,
                                                                     VteCursorShape              cursor_shape);
PangoFontDescription   *ptyxis_settings_dup_font_desc               (PtyxisSettings             *self);
void                    ptyxis_settings_set_font_desc               (PtyxisSettings             *self,
                                                                     const PangoFontDescription *font_desc);
char                   *ptyxis_settings_dup_font_name               (PtyxisSettings             *self);
void                    ptyxis_settings_set_font_name               (PtyxisSettings             *self,
                                                                     const char                 *font_name);
gboolean                ptyxis_settings_get_use_system_font         (PtyxisSettings             *self);
void                    ptyxis_settings_set_use_system_font         (PtyxisSettings             *self,
                                                                     gboolean                    use_system_font);
gboolean                ptyxis_settings_get_restore_session         (PtyxisSettings             *self);
void                    ptyxis_settings_set_restore_session         (PtyxisSettings             *self,
                                                                     gboolean                    restore_session);
gboolean                ptyxis_settings_get_restore_window_size     (PtyxisSettings             *self);
void                    ptyxis_settings_set_restore_window_size     (PtyxisSettings             *self,
                                                                     gboolean                    restore_window_size);
PtyxisScrollbarPolicy   ptyxis_settings_get_scrollbar_policy        (PtyxisSettings             *self);
void                    ptyxis_settings_set_scrollbar_policy        (PtyxisSettings             *self,
                                                                     PtyxisScrollbarPolicy       scrollbar_policy);
VteTextBlinkMode        ptyxis_settings_get_text_blink_mode         (PtyxisSettings             *self);
void                    ptyxis_settings_set_text_blink_mode         (PtyxisSettings             *self,
                                                                     VteTextBlinkMode            text_blink_mode);
void                    ptyxis_settings_get_window_size             (PtyxisSettings             *self,
                                                                     guint                      *columns,
                                                                     guint                      *rows);
void                    ptyxis_settings_set_window_size             (PtyxisSettings             *self,
                                                                     guint                       columns,
                                                                     guint                       rows);
void                    ptyxis_settings_get_default_size            (PtyxisSettings             *self,
                                                                     guint                      *columns,
                                                                     guint                      *rows);
guint                   ptyxis_settings_get_default_columns         (PtyxisSettings             *self);
void                    ptyxis_settings_set_default_columns         (PtyxisSettings             *self,
                                                                     guint                      columns);
guint                   ptyxis_settings_get_default_rows            (PtyxisSettings             *self);
void                    ptyxis_settings_set_default_rows            (PtyxisSettings             *self,
                                                                     guint                      rows);
AdwColorScheme          ptyxis_settings_get_interface_style         (PtyxisSettings             *self);
void                    ptyxis_settings_set_interface_style         (PtyxisSettings             *self,
                                                                     AdwColorScheme              color_scheme);
gboolean                ptyxis_settings_get_toast_on_copy_clipboard (PtyxisSettings             *self);
void                    ptyxis_settings_set_toast_on_copy_clipboard (PtyxisSettings             *self,
                                                                     gboolean                    toast_on_copy_clipboard);
gboolean                ptyxis_settings_get_disable_padding         (PtyxisSettings             *self);
void                    ptyxis_settings_set_disable_padding         (PtyxisSettings             *self,
                                                                     gboolean                    disable_padding);

G_END_DECLS
