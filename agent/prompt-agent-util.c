/*
 * prompt-agent-util.c
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
#include <stdlib.h>

#ifndef __linux__
# include <fcntl.h>
#endif

#include <glib-unix.h>

#include "prompt-agent-compat.h"
#include "prompt-agent-util.h"

static int
prompt_pty_create_producer (int      consumer_fd,
                            gboolean blocking)
{
  _g_autofd int ret = -1;
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

#ifndef __linux__
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
#endif

  return _g_steal_fd (&ret);
}

int
prompt_agent_pty_new_producer (int      consumer_fd,
                               GError **error)
{
  int fd;

  if (-1 == (fd = prompt_pty_create_producer (consumer_fd, FALSE)))
    {
      int errsv = errno;
      g_set_error_literal (error,
                           G_IO_ERROR,
                           g_io_error_from_errno (errsv),
                           g_strerror (errsv));
    }

  return fd;
}

int
prompt_agent_pty_new (GError **error)
{
  int pty_fd = posix_openpt (O_RDWR | O_NOCTTY | O_CLOEXEC);

#ifndef __linux__
  if (pty_fd == -1 && errno == EINVAL)
    {
      int flags;

      pty_fd = posix_openpt (O_RDWR | O_NOCTTY);
      if (pty_fd == -1)
        return -1;

      flags = fcntl (pty_fd, F_GETFD, 0);
      if (flags < 0)
        return -1;

      if (fcntl (pty_fd, F_SETFD, flags | FD_CLOEXEC) < 0)
        return -1;
    }
#endif

  return pty_fd;
}

void
prompt_agent_push_spawn (PromptRunContext   *run_context,
                         GUnixFDList        *fd_list,
                         const char         *cwd,
                         const char * const *argv,
                         GVariant           *fds,
                         GVariant           *env)
{
  GVariantIter iter;

  g_return_if_fail (PROMPT_IS_RUN_CONTEXT (run_context));
  g_return_if_fail (G_IS_UNIX_FD_LIST (fd_list));

  if (cwd && cwd[0])
    prompt_run_context_set_cwd (run_context, cwd);

  prompt_run_context_append_args (run_context, argv);

  prompt_run_context_setenv (run_context, "COLORTERM", "truecolor");
  prompt_run_context_setenv (run_context, "TERM", "xterm-256color");

  if (env && g_variant_iter_init (&iter, env) > 0)
    {
      char *key;
      char *value;

      while (g_variant_iter_loop (&iter, "{&s&s}", &key, &value))
        prompt_run_context_setenv (run_context, key, value);
    }

  if (fds && g_variant_iter_init (&iter, fds) > 0)
    {
      guint dest_fd_num;
      guint handle;

      while (g_variant_iter_loop (&iter, "{uh}", &dest_fd_num, &handle))
        {
          g_autoptr(GError) error = NULL;
          int fd;

          if (-1 == (fd = g_unix_fd_list_get (fd_list, handle, &error)))
            {
              prompt_run_context_push_error (run_context, g_steal_pointer (&error));
              break;
            }

          prompt_run_context_take_fd (run_context, fd, dest_fd_num);
        }
    }
}
