/*
 * prompt-profile.h
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

#include <pango/pango.h>
#include <vte/vte.h>

#include "prompt-palette.h"

G_BEGIN_DECLS

#define PROMPT_PROFILE_KEY_BACKSPACE_BINDING   "backspace-binding"
#define PROMPT_PROFILE_KEY_BOLD_IS_BRIGHT      "bold-is-bright"
#define PROMPT_PROFILE_KEY_CJK_AMBIGUOUS_WIDTH "cjk-ambiguous-width"
#define PROMPT_PROFILE_KEY_CUSTOM_COMMAND      "custom-command"
#define PROMPT_PROFILE_KEY_DEFAULT_CONTAINER   "default-container"
#define PROMPT_PROFILE_KEY_DELETE_BINDING      "delete-binding"
#define PROMPT_PROFILE_KEY_EXIT_ACTION         "exit-action"
#define PROMPT_PROFILE_KEY_LABEL               "label"
#define PROMPT_PROFILE_KEY_LIMIT_SCROLLBACK    "limit-scrollback"
#define PROMPT_PROFILE_KEY_LOGIN_SHELL         "login-shell"
#define PROMPT_PROFILE_KEY_OPACITY             "opacity"
#define PROMPT_PROFILE_KEY_PALETTE             "palette"
#define PROMPT_PROFILE_KEY_PRESERVE_CONTAINER  "preserve-container"
#define PROMPT_PROFILE_KEY_PRESERVE_DIRECTORY  "preserve-directory"
#define PROMPT_PROFILE_KEY_SCROLL_ON_KEYSTROKE "scroll-on-keystroke"
#define PROMPT_PROFILE_KEY_SCROLL_ON_OUTPUT    "scroll-on-output"
#define PROMPT_PROFILE_KEY_SCROLLBACK_LINES    "scrollback-lines"
#define PROMPT_PROFILE_KEY_USE_CUSTOM_COMMAND  "use-custom-command"

typedef enum _PromptExitAction
{
  PROMPT_EXIT_ACTION_NONE    = 0,
  PROMPT_EXIT_ACTION_RESTART = 1,
  PROMPT_EXIT_ACTION_CLOSE   = 2,
} PromptExitAction;

typedef enum _PromptPreserveContainer
{
  PROMPT_PRESERVE_CONTAINER_NEVER  = 0,
  PROMPT_PRESERVE_CONTAINER_ALWAYS = 1,
} PromptPreserveContainer;

typedef enum _PromptPreserveDirectory
{
  PROMPT_PRESERVE_DIRECTORY_NEVER  = 0,
  PROMPT_PRESERVE_DIRECTORY_SAFE   = 1,
  PROMPT_PRESERVE_DIRECTORY_ALWAYS = 2,
} PromptPreserveDirectory;

typedef enum _PromptCjkAmbiguousWidth
{
  PROMPT_CJK_AMBIGUOUS_WIDTH_NARROW = 1,
  PROMPT_CJK_AMBIGUOUS_WIDTH_WIDE   = 2,
} PromptCjkAmbiguousWidth;

#define PROMPT_TYPE_PROFILE (prompt_profile_get_type())

G_DECLARE_FINAL_TYPE (PromptProfile, prompt_profile, PROMPT, PROFILE, GObject)

PromptProfile           *prompt_profile_new                     (const char               *uuid);
PromptProfile           *prompt_profile_duplicate               (PromptProfile            *self);
GSettings               *prompt_profile_dup_settings            (PromptProfile            *self);
const char              *prompt_profile_get_uuid                (PromptProfile            *self);
char                    *prompt_profile_dup_default_container   (PromptProfile            *self);
void                     prompt_profile_set_default_container   (PromptProfile            *self,
                                                                 const char               *default_container);
char                    *prompt_profile_dup_label               (PromptProfile            *self);
void                     prompt_profile_set_label               (PromptProfile            *self,
                                                                 const char               *label);
gboolean                 prompt_profile_get_limit_scrollback    (PromptProfile            *self);
void                     prompt_profile_set_limit_scrollback    (PromptProfile            *self,
                                                                 gboolean                  limit_scrollback);
int                      prompt_profile_get_scrollback_lines    (PromptProfile            *self);
void                     prompt_profile_set_scrollback_lines    (PromptProfile            *self,
                                                                 int                       scrollback_lines);
gboolean                 prompt_profile_get_scroll_on_keystroke (PromptProfile            *self);
void                     prompt_profile_set_scroll_on_keystroke (PromptProfile            *self,
                                                                 gboolean                  scroll_on_keystroke);
gboolean                 prompt_profile_get_scroll_on_output    (PromptProfile            *self);
void                     prompt_profile_set_scroll_on_output    (PromptProfile            *self,
                                                                 gboolean                  scroll_on_output);
gboolean                 prompt_profile_get_bold_is_bright      (PromptProfile            *self);
void                     prompt_profile_set_bold_is_bright      (PromptProfile            *self,
                                                                 gboolean                  bold_is_bright);
PromptExitAction         prompt_profile_get_exit_action         (PromptProfile            *self);
void                     prompt_profile_set_exit_action         (PromptProfile            *self,
                                                                 PromptExitAction          exit_action);
PromptPreserveContainer  prompt_profile_get_preserve_container  (PromptProfile            *self);
void                     prompt_profile_set_preserve_container  (PromptProfile            *self,
                                                                 PromptPreserveContainer   preserve_container);
PromptPreserveDirectory  prompt_profile_get_preserve_directory  (PromptProfile            *self);
void                     prompt_profile_set_preserve_directory  (PromptProfile            *self,
                                                                 PromptPreserveDirectory   preserve_directory);
char                    *prompt_profile_dup_palette_id          (PromptProfile            *self);
PromptPalette           *prompt_profile_dup_palette             (PromptProfile            *self);
void                     prompt_profile_set_palette             (PromptProfile            *self,
                                                                 PromptPalette            *palette);
double                   prompt_profile_get_opacity             (PromptProfile            *self);
void                     prompt_profile_set_opacity             (PromptProfile            *self,
                                                                 double                    opacity);
VteEraseBinding          prompt_profile_get_backspace_binding   (PromptProfile            *self);
void                     prompt_profile_set_backspace_binding   (PromptProfile            *self,
                                                                 VteEraseBinding           backspace_binding);
VteEraseBinding          prompt_profile_get_delete_binding      (PromptProfile            *self);
void                     prompt_profile_set_delete_binding      (PromptProfile            *self,
                                                                 VteEraseBinding           delete_binding);
PromptCjkAmbiguousWidth  prompt_profile_get_cjk_ambiguous_width (PromptProfile            *self);
void                     prompt_profile_set_cjk_ambiguous_width (PromptProfile            *self,
                                                                 PromptCjkAmbiguousWidth   cjk_ambiguous_width);
gboolean                 prompt_profile_get_login_shell         (PromptProfile            *self);
void                     prompt_profile_set_login_shell         (PromptProfile            *self,
                                                                 gboolean                  login_shell);
char                    *prompt_profile_dup_custom_command      (PromptProfile            *self);
void                     prompt_profile_set_custom_command      (PromptProfile            *self,
                                                                 const char               *custom_command);
gboolean                 prompt_profile_get_use_custom_command  (PromptProfile            *self);
void                     prompt_profile_set_use_custom_command  (PromptProfile            *self,
                                                                 gboolean                  use_custom_command);

G_END_DECLS
