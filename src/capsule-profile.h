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

G_BEGIN_DECLS

#define CAPSULE_TYPE_PROFILE (capsule_profile_get_type())

G_DECLARE_FINAL_TYPE (CapsuleProfile, capsule_profile, CAPSULE, PROFILE, GObject)

CapsuleProfile       *capsule_profile_new                     (const char                 *uuid);
const char           *capsule_profile_get_uuid                (CapsuleProfile             *self);
gboolean              capsule_profile_get_audible_bell        (CapsuleProfile             *self);
void                  capsule_profile_set_audible_bell        (CapsuleProfile             *self,
                                                               gboolean                    audible_bell);
char                 *capsule_profile_dup_default_container   (CapsuleProfile             *self);
void                  capsule_profile_set_default_container   (CapsuleProfile             *self,
                                                               const char                 *default_container);
PangoFontDescription *capsule_profile_dup_font_desc           (CapsuleProfile             *self);
void                  capsule_profile_set_font_desc           (CapsuleProfile             *self,
                                                               const PangoFontDescription *font_desc);
char                 *capsule_profile_dup_font_name           (CapsuleProfile             *self);
void                  capsule_profile_set_font_name           (CapsuleProfile             *self,
                                                               const char                 *font_name);
gboolean              capsule_profile_get_use_system_font     (CapsuleProfile             *self);
void                  capsule_profile_set_use_system_font     (CapsuleProfile             *self,
                                                               gboolean                    use_system_font);
char                 *capsule_profile_dup_label               (CapsuleProfile             *self);
void                  capsule_profile_set_label               (CapsuleProfile             *self,
                                                               const char                 *label);
gboolean              capsule_profile_get_scroll_on_keystroke (CapsuleProfile             *self);
void                  capsule_profile_set_scroll_on_keystroke (CapsuleProfile             *self,
                                                               gboolean                    scroll_on_keystroke);
gboolean              capsule_profile_get_scroll_on_output    (CapsuleProfile             *self);
void                  capsule_profile_set_scroll_on_output    (CapsuleProfile             *self,
                                                               gboolean                    scroll_on_output);

G_END_DECLS
