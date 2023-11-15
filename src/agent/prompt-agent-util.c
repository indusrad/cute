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
#include <fcntl.h>
#include <stdlib.h>

#include <glib-unix.h>

#include "prompt-agent-util.h"

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
