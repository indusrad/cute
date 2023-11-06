/*
 * capsule-profile.h
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

#include "capsule-palette.h"
#include "capsule-run-context.h"

G_BEGIN_DECLS

#define CAPSULE_PROFILE_KEY_BACKSPACE_BINDING   "backspace-binding"
#define CAPSULE_PROFILE_KEY_CJK_AMBIGUOUS_WIDTH "cjk-ambiguous-width"
#define CAPSULE_PROFILE_KEY_DEFAULT_CONTAINER   "default-container"
#define CAPSULE_PROFILE_KEY_DELETE_BINDING      "delete-binding"
#define CAPSULE_PROFILE_KEY_EXIT_ACTION         "exit-action"
#define CAPSULE_PROFILE_KEY_LABEL               "label"
#define CAPSULE_PROFILE_KEY_LIMIT_SCROLLBACK    "limit-scrollback"
#define CAPSULE_PROFILE_KEY_OPACITY             "opacity"
#define CAPSULE_PROFILE_KEY_PALETTE             "palette"
#define CAPSULE_PROFILE_KEY_PRESERVE_DIRECTORY  "preserve-directory"
#define CAPSULE_PROFILE_KEY_SCROLL_ON_KEYSTROKE "scroll-on-keystroke"
#define CAPSULE_PROFILE_KEY_SCROLL_ON_OUTPUT    "scroll-on-output"
#define CAPSULE_PROFILE_KEY_SCROLLBACK_LINES    "scrollback-lines"

typedef enum _CapsuleExitAction
{
  CAPSULE_EXIT_ACTION_NONE = 0,
  CAPSULE_EXIT_ACTION_RESTART,
  CAPSULE_EXIT_ACTION_CLOSE,
} CapsuleExitAction;

typedef enum _CapsulePreserveDirectory
{
  CAPSULE_PRESERVE_DIRECTORY_NEVER = 0,
  CAPSULE_PRESERVE_DIRECTORY_SAFE,
  CAPSULE_PRESERVE_DIRECTORY_ALWAYS,
} CapsulePreserveDirectory;

typedef enum _CapsuleCjkAmbiguousWidth
{
  CAPSULE_CJK_AMBIGUOUS_WIDTH_NARROW = 1,
  CAPSULE_CJK_AMBIGUOUS_WIDTH_WIDE   = 2,
} CapsuleCjkAmbiguousWidth;

#define CAPSULE_TYPE_PROFILE (capsule_profile_get_type())

G_DECLARE_FINAL_TYPE (CapsuleProfile, capsule_profile, CAPSULE, PROFILE, GObject)

CapsuleProfile           *capsule_profile_new                     (const char                 *uuid);
CapsuleProfile           *capsule_profile_duplicate               (CapsuleProfile             *self);
GSettings                *capsule_profile_dup_settings            (CapsuleProfile             *self);
void                      capsule_profile_apply                   (CapsuleProfile             *self,
                                                                   CapsuleRunContext          *run_context,
                                                                   VtePty                     *pty,
                                                                   const char                 *current_directory_uri,
                                                                   const char                 *default_shell);
const char               *capsule_profile_get_uuid                (CapsuleProfile             *self);
char                     *capsule_profile_dup_default_container   (CapsuleProfile             *self);
void                      capsule_profile_set_default_container   (CapsuleProfile             *self,
                                                                   const char                 *default_container);
char                     *capsule_profile_dup_label               (CapsuleProfile             *self);
void                      capsule_profile_set_label               (CapsuleProfile             *self,
                                                                   const char                 *label);
gboolean                  capsule_profile_get_limit_scrollback    (CapsuleProfile             *self);
void                      capsule_profile_set_limit_scrollback    (CapsuleProfile             *self,
                                                                   gboolean                    limit_scrollback);
int                       capsule_profile_get_scrollback_lines    (CapsuleProfile             *self);
void                      capsule_profile_set_scrollback_lines    (CapsuleProfile             *self,
                                                                   int                         scrollback_lines);
gboolean                  capsule_profile_get_scroll_on_keystroke (CapsuleProfile             *self);
void                      capsule_profile_set_scroll_on_keystroke (CapsuleProfile             *self,
                                                                   gboolean                    scroll_on_keystroke);
gboolean                  capsule_profile_get_scroll_on_output    (CapsuleProfile             *self);
void                      capsule_profile_set_scroll_on_output    (CapsuleProfile             *self,
                                                                   gboolean                    scroll_on_output);
CapsuleExitAction         capsule_profile_get_exit_action         (CapsuleProfile             *self);
void                      capsule_profile_set_exit_action         (CapsuleProfile             *self,
                                                                   CapsuleExitAction           exit_action);
CapsulePreserveDirectory  capsule_profile_get_preserve_directory  (CapsuleProfile             *self);
void                      capsule_profile_set_preserve_directory  (CapsuleProfile             *self,
                                                                   CapsulePreserveDirectory    preserve_directory);
CapsulePalette           *capsule_profile_dup_palette             (CapsuleProfile             *self);
void                      capsule_profile_set_palette             (CapsuleProfile             *self,
                                                                   CapsulePalette             *palette);
double                    capsule_profile_get_opacity             (CapsuleProfile             *self);
void                      capsule_profile_set_opacity             (CapsuleProfile             *self,
                                                                   double                      opacity);
VteEraseBinding           capsule_profile_get_backspace_binding   (CapsuleProfile             *self);
void                      capsule_profile_set_backspace_binding   (CapsuleProfile             *self,
                                                                   VteEraseBinding             backspace_binding);
VteEraseBinding           capsule_profile_get_delete_binding      (CapsuleProfile             *self);
void                      capsule_profile_set_delete_binding      (CapsuleProfile             *self,
                                                                   VteEraseBinding             delete_binding);
CapsuleCjkAmbiguousWidth  capsule_profile_get_cjk_ambiguous_width (CapsuleProfile             *self);
void                      capsule_profile_set_cjk_ambiguous_width (CapsuleProfile             *self,
                                                                   CapsuleCjkAmbiguousWidth    cjk_ambiguous_width);

G_END_DECLS
