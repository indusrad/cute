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

#include <glib-object.h>

G_BEGIN_DECLS

#define CAPSULE_SETTING_KEY_DEFAULT_PROFILE_UUID "default-profile-uuid"
#define CAPSULE_SETTING_KEY_NEW_TAB_POSITION     "new-tab-position"
#define CAPSULE_SETTING_KEY_PROFILE_UUIDS        "profile-uuids"

typedef enum _CapsuleNewTabPosition
{
  CAPSULE_NEW_TAB_POSITION_LAST = 0,
  CAPSULE_NEW_TAB_POSITION_NEXT,
} CapsuleNewTabPosition;

#define CAPSULE_TYPE_SETTINGS (capsule_settings_get_type())

G_DECLARE_FINAL_TYPE (CapsuleSettings, capsule_settings, CAPSULE, SETTINGS, GObject)

CapsuleSettings        *capsule_settings_new                      (void);
GSettings              *capsule_settings_get_settings             (CapsuleSettings       *self);
char                   *capsule_settings_dup_default_profile_uuid (CapsuleSettings       *self);
void                    capsule_settings_set_default_profile_uuid (CapsuleSettings       *self,
                                                                   const char            *uuid);
char                  **capsule_settings_dup_profile_uuids        (CapsuleSettings       *self);
void                    capsule_settings_add_profile_uuid         (CapsuleSettings       *self,
                                                                   const char            *uuid);
void                    capsule_settings_remove_profile_uuid      (CapsuleSettings       *self,
                                                                   const char            *uuid);
CapsuleNewTabPosition   capsule_settings_get_new_tab_position     (CapsuleSettings       *self);
void                    capsule_settings_set_new_tab_position     (CapsuleSettings       *self,
                                                                   CapsuleNewTabPosition  new_tab_position);

G_END_DECLS
