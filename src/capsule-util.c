/* capsule-util.c
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

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <wordexp.h>

#include <glib-unix.h>
#include <glib/gstdio.h>

#include <gio/gio.h>

#include "gconstructor.h"

#include "capsule-util.h"

static CapsuleProcessKind kind = CAPSULE_PROCESS_KIND_HOST;
static char *user_shell;
static char *user_default_path;

G_DEFINE_CONSTRUCTOR(capsule_init_ctor)

CapsuleProcessKind
capsule_get_process_kind (void)
{
  return kind;
}

static gboolean
shell_supports_dash_c (const char *shell)
{
  if (shell == NULL)
    return FALSE;

  return strcmp (shell, "bash") == 0 || g_str_has_suffix (shell, "/bash") ||
#if 0
         /* Fish does apparently support -l and -c in testing, but it is causing
          * issues with users, so we will disable it for now so that we fallback
          * to using `sh -l -c ''` instead.
          */
         strcmp (shell, "fish") == 0 || g_str_has_suffix (shell, "/fish") ||
#endif
         strcmp (shell, "zsh") == 0 || g_str_has_suffix (shell, "/zsh") ||
         strcmp (shell, "dash") == 0 || g_str_has_suffix (shell, "/dash") ||
         strcmp (shell, "tcsh") == 0 || g_str_has_suffix (shell, "/tcsh") ||
         strcmp (shell, "sh") == 0 || g_str_has_suffix (shell, "/sh");
}

/**
 * shell_supports_dash_login:
 * @shell: the name of the shell, such as `sh` or `/bin/sh`
 *
 * Checks if the shell is known to support login semantics. Originally,
 * this meant `--login`, but now is meant to mean `-l` as more shells
 * support `-l` than `--login` (notably dash).
 *
 * Returns: %TRUE if @shell likely supports `-l`.
 */
static gboolean
shell_supports_dash_login (const char *shell)
{
  if (shell == NULL)
    return FALSE;

  return strcmp (shell, "bash") == 0 || g_str_has_suffix (shell, "/bash") ||
#if 0
         strcmp (shell, "fish") == 0 || g_str_has_suffix (shell, "/fish") ||
#endif
         strcmp (shell, "zsh") == 0 || g_str_has_suffix (shell, "/zsh") ||
         strcmp (shell, "dash") == 0 || g_str_has_suffix (shell, "/dash") ||
#if 0
         /* tcsh supports -l and -c but not combined! To do that, you'd have
          * to instead launch the login shell like `-tcsh -c 'command'`, which
          * is possible, but we lack the abstractions for that currently.
          */
         strcmp (shell, "tcsh") == 0 || g_str_has_suffix (shell, "/tcsh") ||
#endif
         strcmp (shell, "sh") == 0 || g_str_has_suffix (shell, "/sh");
}

/**
 * capsule_pty_create_producer:
 * @consumer_fd: a pty
 * @blocking: use %FALSE to set O_NONBLOCK
 *
 * This creates a new producer to the PTY consumer @consumer_fd.
 *
 * This uses grantpt(), unlockpt(), and ptsname() to open a new
 * PTY producer.
 *
 * Returns: a FD for the producer PTY that should be closed with close().
 *   Upon error, -1 is returned.
 */
int
capsule_pty_create_producer (int      consumer_fd,
                             gboolean blocking)
{
  g_autofd int ret = -1;
  gint extra = blocking ? 0 : O_NONBLOCK;
#if defined(HAVE_PTSNAME_R) || defined(__FreeBSD__)
  char name[256];
#else
  const char *name;
#endif

  g_return_val_if_fail (consumer_fd != -1, -1);

  if (grantpt (consumer_fd) != 0)
    return -1;

  if (unlockpt (consumer_fd) != 0)
    return -1;

#ifdef HAVE_PTSNAME_R
  if (ptsname_r (consumer_fd, name, sizeof name - 1) != 0)
    return -1;
  name[sizeof name - 1] = '\0';
#elif defined(__FreeBSD__)
  if (fdevname_r (consumer_fd, name + 5, sizeof name - 6) == NULL)
    return -1;
  memcpy (name, "/dev/", 5);
  name[sizeof name - 1] = '\0';
#else
  if (NULL == (name = ptsname (consumer_fd)))
    return -1;
#endif

  ret = open (name, O_NOCTTY | O_RDWR | O_CLOEXEC | extra);

  if (ret == -1 && errno == EINVAL)
    {
      gint flags;

      ret = open (name, O_NOCTTY | O_RDWR | O_CLOEXEC);
      if (ret == -1 && errno == EINVAL)
        ret = open (name, O_NOCTTY | O_RDWR);

      if (ret == -1)
        return -1;

      /* Add FD_CLOEXEC if O_CLOEXEC failed */
      flags = fcntl (ret, F_GETFD, 0);
      if ((flags & FD_CLOEXEC) == 0)
        {
          if (fcntl (ret, F_SETFD, flags | FD_CLOEXEC) < 0)
            return -1;
        }

      if (!blocking)
        {
          if (!g_unix_set_fd_nonblocking (ret, TRUE, NULL))
            return -1;
        }
    }

  return g_steal_fd (&ret);
}

static char **
get_environ_from_stdout (GSubprocess *subprocess)
{
  g_autofree char *stdout_buf = NULL;

  if (g_subprocess_communicate_utf8 (subprocess, NULL, NULL, &stdout_buf, NULL, NULL))
    {
      g_auto(GStrv) lines = g_strsplit (stdout_buf, "\n", 0);
      g_autoptr(GPtrArray) env = g_ptr_array_new_with_free_func (g_free);

      for (guint i = 0; lines[i]; i++)
        {
          const char *line = lines[i];

          if (!g_ascii_isalpha (*line) && *line != '_')
            continue;

          for (const char *iter = line; *iter; iter = g_utf8_next_char (iter))
            {
              if (*iter == '=')
                {
                  g_ptr_array_add (env, g_strdup (line));
                  break;
                }

              if (!g_ascii_isalnum (*iter) && *iter != '_')
                break;
            }
        }

      if (env->len > 0)
        {
          g_ptr_array_add (env, NULL);
          return (char **)g_ptr_array_free (g_steal_pointer (&env), FALSE);
        }
    }

  return NULL;
}

const char * const *
capsule_host_environ (void)
{
  static char **host_environ;

  if (host_environ == NULL)
    {
      if (capsule_get_process_kind () == CAPSULE_PROCESS_KIND_FLATPAK)
        {
          g_autoptr(GSubprocessLauncher) launcher = NULL;
          g_autoptr(GSubprocess) subprocess = NULL;
          g_autoptr(GError) error = NULL;

          launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);
          subprocess = g_subprocess_launcher_spawn (launcher, &error,
                                                    "flatpak-spawn", "--host", "printenv", NULL);
          if (subprocess != NULL)
            host_environ = get_environ_from_stdout (subprocess);
        }

      if (host_environ == NULL)
        host_environ = g_get_environ ();
    }

  return (const char * const *)host_environ;
}

/**
 * capsule_path_expand:
 *
 * This function will expand various "shell-like" features of the provided
 * path using the POSIX wordexp(3) function. Command substitution will
 * not be enabled, but path features such as ~user will be expanded.
 *
 * Returns: (transfer full): A newly allocated string containing the
 *   expansion. A copy of the input string upon failure to expand.
 */
char *
capsule_path_expand (const char *path)
{
  wordexp_t state = { 0 };
  char *replace_home = NULL;
  char *ret = NULL;
  char *escaped;
  int r;

  if (path == NULL)
    return NULL;

  /* Special case some path prefixes */
  if (path[0] == '~')
    {
      if (path[1] == 0)
        path = g_get_home_dir ();
      else if (path[1] == G_DIR_SEPARATOR)
        path = replace_home = g_strdup_printf ("%s%s", g_get_home_dir (), &path[1]);
    }
  else if (strncmp (path, "$HOME", 5) == 0)
    {
      if (path[5] == 0)
        path = g_get_home_dir ();
      else if (path[5] == G_DIR_SEPARATOR)
        path = replace_home = g_strdup_printf ("%s%s", g_get_home_dir (), &path[5]);
    }

  escaped = g_shell_quote (path);
  r = wordexp (escaped, &state, WRDE_NOCMD);
  if (r == 0 && state.we_wordc > 0)
    ret = g_strdup (state.we_wordv [0]);
  wordfree (&state);

  if (!g_path_is_absolute (ret))
    {
      g_autofree char *freeme = ret;

      ret = g_build_filename (g_get_home_dir (), freeme, NULL);
    }

  g_free (replace_home);
  g_free (escaped);

  return ret;
}

/**
 * capsule_path_collapse:
 *
 * This function will collapse a path that starts with the users home
 * directory into a shorthand notation using ~/ for the home directory.
 *
 * If the path does not have the home directory as a prefix, it will
 * simply return a copy of @path.
 *
 * Returns: (transfer full): A new path, possibly collapsed.
 */
char *
capsule_path_collapse (const char *path)
{
  g_autofree char *expanded = NULL;

  if (path == NULL)
    return NULL;

  expanded = capsule_path_expand (path);

  if (g_str_has_prefix (expanded, g_get_home_dir ()))
    return g_build_filename ("~",
                             expanded + strlen (g_get_home_dir ()),
                             NULL);

  return g_steal_pointer (&expanded);
}

/**
 * capsule_get_user_shell:
 *
 * Gets the user preferred shell on the host.
 *
 * If the background shell discovery has not yet finished due to
 * slow or misconfigured getent on the host, this will provide a
 * sensible fallback.
 *
 * Returns: (not nullable): a shell such as "/bin/sh"
 */
const char *
capsule_get_user_shell (void)
{
  return user_shell;
}

static void
capsule_guess_shell_communicate_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  GSubprocess *subprocess = (GSubprocess *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree char *stdout_buf = NULL;
  const char *key;

  g_assert (G_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  key = g_task_get_task_data (task);

  if (!g_subprocess_communicate_utf8_finish (subprocess, result, &stdout_buf, NULL, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (stdout_buf != NULL)
    g_strstrip (stdout_buf);

  g_debug ("Guessed %s as \"%s\"", key, stdout_buf);

  if (g_str_equal (key, "SHELL"))
    {
      if (stdout_buf[0] == '/')
        user_shell = g_steal_pointer (&stdout_buf);
    }
  else if (g_str_equal (key, "PATH"))
    {
      if (stdout_buf && stdout_buf[0])
        user_default_path = g_steal_pointer (&stdout_buf);
    }
  else
    {
      g_critical ("Unknown key %s", key);
    }

  g_task_return_boolean (task, TRUE);
}

static void
capsule_guess_shell (GCancellable        *cancellable,
                     GAsyncReadyCallback  callback,
                     gpointer             user_data)
{
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GStrvBuilder) builder = NULL;
  g_autoptr(GTask) task = NULL;
  g_autofree char *command = NULL;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) argv = NULL;

  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_task_data (task, g_strdup ("SHELL"), g_free);

#ifdef __APPLE__
  command = g_strdup_printf ("sh -c 'dscacheutil -q user -a name %s | grep ^shell: | cut -f 2 -d \" \"'",
                             g_get_user_name ());
#else
  command = g_strdup_printf ("sh -c 'getent passwd %s | head -n1 | cut -f 7 -d :'",
                             g_get_user_name ());
#endif

  builder = g_strv_builder_new ();

  if (capsule_get_process_kind () == CAPSULE_PROCESS_KIND_FLATPAK)
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

  if (!(subprocess = g_subprocess_launcher_spawnv (launcher, (const char * const *)argv, &error)))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_subprocess_communicate_utf8_async (subprocess,
                                         NULL,
                                         cancellable,
                                         capsule_guess_shell_communicate_cb,
                                         g_steal_pointer (&task));
}

static void
capsule_shell_init_guess_path_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  g_autoptr(GError) error = NULL;

  g_assert (object == NULL);
  g_assert (G_IS_TASK (result));
  g_assert (user_data == NULL);

  if (!g_task_propagate_boolean (G_TASK (result), &error))
    g_warning ("Failed to guess user $PATH using $SHELL %s: %s",
               user_shell, error->message);
}

static void
_capsule_guess_user_path (GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GStrvBuilder) builder = NULL;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) argv = NULL;

  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_task_data (task, g_strdup ("PATH"), g_free);

  /* This works by running 'echo $PATH' on the host, preferably
   * through the user $SHELL we discovered.
   */
  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  g_subprocess_launcher_set_cwd (launcher, g_get_home_dir ());

  builder = g_strv_builder_new ();

  if (shell_supports_dash_c (user_shell))
    {
      g_strv_builder_add (builder, user_shell);
      if (shell_supports_dash_login (user_shell))
        g_strv_builder_add (builder, "-l");
      g_strv_builder_add (builder, "-c");
      g_strv_builder_add (builder, "echo $PATH");
    }
  else
    {
      g_strv_builder_add (builder, "/bin/sh");
      g_strv_builder_add (builder, "-l");
      g_strv_builder_add (builder, "-c");
      g_strv_builder_add (builder, "echo $PATH");
    }

  argv = g_strv_builder_end (builder);

  if (!(subprocess = g_subprocess_launcher_spawnv (launcher, (const char * const *)argv, &error)))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_subprocess_communicate_utf8_async (subprocess,
                                         NULL,
                                         NULL,
                                         capsule_guess_shell_communicate_cb,
                                         g_steal_pointer (&task));
}

static void
capsule_shell_init_guess_shell_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  g_autoptr(GError) error = NULL;

  g_assert (object == NULL);
  g_assert (G_IS_TASK (result));
  g_assert (user_data == NULL);

  if (!g_task_propagate_boolean (G_TASK (result), &error))
    g_warning ("Failed to guess user $SHELL: %s", error->message);

  _capsule_guess_user_path (NULL,
                            capsule_shell_init_guess_path_cb,
                            NULL);
}

static void
capsule_init_ctor (void)
{
  if (g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS))
    kind = CAPSULE_PROCESS_KIND_FLATPAK;

  /* First we need to guess the user shell, so that we can potentially
   * get the path using that shell (instead of just /bin/sh which might
   * not include things like .bashrc).
   */
  capsule_guess_shell (NULL,
                       capsule_shell_init_guess_shell_cb,
                       NULL);
}
