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

#define CAPSULE_TYPE_SETTINGS (capsule_settings_get_type())

G_DECLARE_FINAL_TYPE (CapsuleSettings, capsule_settings, CAPSULE, SETTINGS, GObject)

CapsuleSettings  *capsule_settings_new                      (void);
char             *capsule_settings_dup_default_profile_uuid (CapsuleSettings *self);
void              capsule_settings_set_default_profile_uuid (CapsuleSettings *self,
                                                             const char      *uuid);
char            **capsule_settings_dup_profile_uuids        (CapsuleSettings *self);
void              capsule_settings_add_profile_uuid         (CapsuleSettings *self,
                                                             const char      *uuid);
void              capsule_settings_remove_profile_uuid      (CapsuleSettings *self,
                                                             const char      *uuid);

G_END_DECLS
