/*
 * prompt-translate-path.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include "line-reader.h"

#include "prompt-mount.h"
#include "prompt-mount-namespace.h"
#include "prompt-translate-path.h"

static void
decode_space (gchar **str)
{
  /* Replace encoded space "\040" with ' ' */
  if (strstr (*str, "\\040"))
    {
      g_auto(GStrv) parts = g_strsplit (*str, "\\040", 0);
      g_free (*str);
      *str = g_strjoinv (" ", parts);
    }
}

static PromptMountNamespace *
load_our_namespace (void)
{
  g_autoptr(PromptMountNamespace) mount_namespace = prompt_mount_namespace_new ();
  g_autofree char *mounts_contents = NULL;
  LineReader reader;
  gsize mounts_len;
  gsize line_len;
  char *line;

  if (!g_file_get_contents ("/proc/mounts", &mounts_contents, &mounts_len, NULL))
    return g_steal_pointer (&mount_namespace);

  line_reader_init (&reader, (char *)mounts_contents, mounts_len);
  while ((line = line_reader_next (&reader, &line_len)))
    {
      g_autoptr(PromptMountDevice) mount_device = NULL;
      g_autofree char *subvol = NULL;
      g_auto(GStrv) parts = NULL;
      const char *filesystem;
      const char *mountpoint;
      const char *device;
      const char *options;

      line[line_len] = 0;

      parts = g_strsplit (line, " ", 5);

      /* Field 0: device
       * Field 1: mountpoint
       * Field 2: filesystem
       * Field 3: Options
       * .. Ignored ..
       */
      if (g_strv_length (parts) != 5)
        continue;

      for (guint j = 0; parts[j]; j++)
        decode_space (&parts[j]);

      device = parts[0];
      mountpoint = parts[1];
      filesystem = parts[2];
      options = parts[3];

      if (g_strcmp0 (filesystem, "btrfs") == 0)
        {
          g_auto(GStrv) opts = g_strsplit (options, ",", 0);

          for (guint k = 0; opts[k]; k++)
            {
              if (g_str_has_prefix (opts[k], "subvol="))
                {
                  subvol = g_strdup (opts[k] + strlen ("subvol="));
                  break;
                }
            }
        }

      prompt_mount_namespace_add_device (mount_namespace,
                                         prompt_mount_device_new (g_strdup (device),
                                                                  g_strdup (mountpoint),
                                                                  g_strdup (subvol)));
    }

  return g_steal_pointer (&mount_namespace);
}

char *
prompt_translate_path (GPid        pid,
                       const char *path)
{
  g_autoptr(PromptMountNamespace) mount_namespace = NULL;
  g_autofree char *mountinfo_path = NULL;
  g_autofree char *mountinfo_contents = NULL;
  g_auto(GStrv) translated = NULL;
  LineReader reader;
  gsize mountinfo_len;
  gsize line_len;
  char *line;

  g_return_val_if_fail (pid > 0, NULL);
  g_return_val_if_fail (path, NULL);

  mount_namespace = load_our_namespace ();
  mountinfo_path = g_strdup_printf ("/proc/%d/mountinfo", pid);
  if (!g_file_get_contents (mountinfo_path, &mountinfo_contents, &mountinfo_len, NULL))
    return NULL;

  line_reader_init (&reader, (char *)mountinfo_contents, mountinfo_len);
  while ((line = line_reader_next (&reader, &line_len)))
    {
      g_autoptr(PromptMount) mount = NULL;

      line[line_len] = 0;

      if ((mount = prompt_mount_new_for_mountinfo (line)))
        prompt_mount_namespace_add_mount (mount_namespace, g_steal_pointer (&mount));
    }

  if ((translated = prompt_mount_namespace_translate (mount_namespace, path)))
    {
      for (guint i = 0; translated[i]; i++)
        {
          if (g_file_test (translated[i], G_FILE_TEST_EXISTS))
            return g_strdup (translated[i]);
        }
    }

  return NULL;
}
