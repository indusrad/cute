/* prompt-user.c
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

#include "config.h"

#include "prompt-user.h"
#include "prompt-util.h"

#include "eggshell.h"

struct _PromptUser
{
  GObject parent_instance;
};

G_DEFINE_FINAL_TYPE (PromptUser, prompt_user, G_TYPE_OBJECT)

static void
prompt_user_class_init (PromptUserClass *klass)
{
}

static void
prompt_user_init (PromptUser *self)
{
}

PromptUser *
prompt_user_get_default (void)
{
  static PromptUser *instance;

  if (instance == NULL)
    {
      instance = g_object_new (PROMPT_TYPE_USER, NULL);
      g_object_add_weak_pointer (G_OBJECT (instance), (gpointer *)&instance);
    }

  return instance;
}

static void
prompt_user_discover_shell (GTask        *task,
                            gpointer      source_object,
                            gpointer      task_data,
                            GCancellable *cancellable)
{
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GStrvBuilder) builder = NULL;
  g_autofree char *command = NULL;
  g_autofree char *stdout_buf = NULL;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) argv = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (PROMPT_IS_USER (source_object));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

#ifdef __APPLE__
  command = g_strdup_printf ("sh -c 'dscacheutil -q user -a name %s | grep ^shell: | cut -f 2 -d \" \"'",
                             g_get_user_name ());
#else
  command = g_strdup_printf ("sh -c 'getent passwd %s | head -n1 | cut -f 7 -d :'",
                             g_get_user_name ());
#endif

  builder = g_strv_builder_new ();

  if (prompt_get_process_kind () == PROMPT_PROCESS_KIND_FLATPAK)
    {
      g_strv_builder_add (builder, "flatpak-spawn");
      g_strv_builder_add (builder, "--host");
      g_strv_builder_add (builder, "--watch-bus");
    }

  if (!g_shell_parse_argv (command, NULL, &argv, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_strv_builder_addv (builder, (const char **)argv);
  g_clear_pointer (&argv, g_strfreev);
  argv = g_strv_builder_end (builder);

  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  g_subprocess_launcher_set_cwd (launcher, g_get_home_dir ());

  if (!(subprocess = g_subprocess_launcher_spawnv (launcher, (const char * const *)argv, &error)) ||
      !g_subprocess_communicate_utf8 (subprocess, NULL, cancellable, &stdout_buf, NULL, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_strstrip (stdout_buf);

  if (stdout_buf[0] == 0)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Failed to discover user shell");
      return;
    }

  g_task_return_pointer (task, g_strdup (stdout_buf), g_free);
}

void
prompt_user_discover_shell_async (PromptUser          *self,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (PROMPT_IS_USER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, prompt_user_discover_shell_async);

  switch (prompt_get_process_kind ())
    {
    case PROMPT_PROCESS_KIND_HOST:
      g_task_return_pointer (task, egg_shell (g_getenv ("SHELL")), g_free);
      return;

    case PROMPT_PROCESS_KIND_FLATPAK:
      g_task_run_in_thread (task, prompt_user_discover_shell);
      break;

    default:
      g_assert_not_reached ();
    }
}

char *
prompt_user_discover_shell_finish (PromptUser    *self,
                                   GAsyncResult  *result,
                                   GError       **error)
{
  g_return_val_if_fail (PROMPT_IS_USER (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}
