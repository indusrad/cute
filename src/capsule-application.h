/* capsule-application.h
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

#include "capsule-container.h"
#include "capsule-profile.h"

G_BEGIN_DECLS

#define CAPSULE_TYPE_APPLICATION    (capsule_application_get_type())
#define CAPSULE_APPLICATION_DEFAULT (CAPSULE_APPLICATION(g_application_get_default()))

G_DECLARE_FINAL_TYPE (CapsuleApplication, capsule_application, CAPSULE, APPLICATION, AdwApplication)

CapsuleApplication *capsule_application_new                  (const char         *application_id,
                                                              GApplicationFlags   flags);
const char         *capsule_application_get_system_font_name (CapsuleApplication *self);
gboolean            capsule_application_control_is_pressed   (CapsuleApplication *self);
void                capsule_application_add_profile          (CapsuleApplication *self,
                                                              CapsuleProfile     *profile);
void                capsule_application_remove_profile       (CapsuleApplication *self,
                                                              CapsuleProfile     *profile);
CapsuleProfile     *capsule_application_dup_default_profile  (CapsuleApplication *self);
void                capsule_application_set_default_profile  (CapsuleApplication *self,
                                                              CapsuleProfile     *profile);
CapsuleProfile     *capsule_application_dup_profile          (CapsuleApplication *self,
                                                              const char         *profile_uuid);
GMenuModel         *capsule_application_dup_profile_menu     (CapsuleApplication *self);
GListModel         *capsule_application_list_profiles        (CapsuleApplication *self);
GListModel         *capsule_application_list_containers      (CapsuleApplication *self);

G_END_DECLS
