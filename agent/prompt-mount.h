/* prompt-mount.h
 *
 * Copyright 2023-2024 Christian Hergert <chergert@redhat.com>
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

#define PROMPT_TYPE_MOUNT (prompt_mount_get_type())

G_DECLARE_FINAL_TYPE (PromptMount, prompt_mount, PROMPT, MOUNT, GObject)

struct _PromptMount
{
  GObject parent_instance;
  int mount_id;
  int parent_mount_id;
  int device_major;
  int device_minor;
  GRefString *root;
  GRefString *mount_point;
  GRefString *mount_source;
  GRefString *filesystem_type;
  GRefString *superblock_options;
  guint is_overlay : 1;
  guint layer : 15;
};

PromptMount  *prompt_mount_new_for_mountinfo      (const char    *mountinfo);
PromptMount  *prompt_mount_new_for_overlay        (const char    *mount_point,
                                                   const char    *host_path,
                                                   int            layer);
const char   *prompt_mount_get_relative_path      (PromptMount   *self,
                                                   const char    *path);
int           prompt_mount_get_device_major       (PromptMount   *self);
int           prompt_mount_get_device_minor       (PromptMount   *self);
int           prompt_mount_get_mount_id           (PromptMount   *self);
int           prompt_mount_get_parent_mount_id    (PromptMount   *self);
const char   *prompt_mount_get_root               (PromptMount   *self);
const char   *prompt_mount_get_mount_point        (PromptMount   *self);
const char   *prompt_mount_get_mount_source       (PromptMount   *self);
const char   *prompt_mount_get_filesystem_type    (PromptMount   *self);
const char   *prompt_mount_get_superblock_options (PromptMount   *self);
char         *prompt_mount_get_superblock_option  (PromptMount   *self,
                                                   const char     *option);

G_END_DECLS
