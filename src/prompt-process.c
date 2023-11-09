/* prompt-process.c
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

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "prompt-process.h"

typedef struct
{
  GSubprocess *subprocess;
  VtePty      *pty;
  guint        wait_completed : 1;
} PromptProcessPrivate;

enum {
  PROP_0,
  PROP_SUBPROCESS,
  PROP_PTY,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (PromptProcess, prompt_process, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];
static GHashTable *exec_to_kind;

static PromptProcessLeaderKind
prompt_process_real_get_leader_kind (PromptProcess *self)
{
  PromptProcessPrivate *priv = prompt_process_get_instance_private (self);

  if (!priv->wait_completed && priv->pty != NULL)
    {
      int fd = vte_pty_get_fd (priv->pty);
      GPid pid = tcgetpgrp (fd);

      if (pid > 0)
        {
          g_autofree char *path = g_strdup_printf ("/proc/%d/", pid);
          g_autofree char *exe = g_strdup_printf ("/proc/%d/exe", pid);
          char execpath[512];
          gssize len;
          struct stat st;

          if (stat (path, &st) == 0)
            {
              if (st.st_uid == 0)
                return PROMPT_PROCESS_LEADER_KIND_SUPERUSER;
            }

          len = readlink (exe, execpath, sizeof execpath-1);

          if (len > 0)
            {
              const char *end;

              execpath[len] = 0;

              if ((end = strrchr (execpath, '/')))
                return GPOINTER_TO_UINT (g_hash_table_lookup (exec_to_kind, end+1));
            }
        }
    }

  return PROMPT_PROCESS_LEADER_KIND_UNKNOWN;
}

static gboolean
prompt_process_real_has_leader (PromptProcess *self)
{
  PromptProcessPrivate *priv = prompt_process_get_instance_private (self);
  int fd = -1;

  g_assert (PROMPT_IS_PROCESS (self));

  if (priv->wait_completed)
    return FALSE;

  if (priv->pty != NULL)
    fd = vte_pty_get_fd (priv->pty);

  if (fd != -1)
    {
      GPid pid = tcgetpgrp (fd);
      const char *ident;
      GPid child_pid;

      /* This is not documented to be something that can happen, but if the
       * subprocess is in a different PID namespace, then the Linux kernel is
       * going to probably give us 0 back from put_user() in the TTY layer.
       *
       * For now, just bail as if there is a process running.
       */
      if (pid == 0)
        return TRUE;

      /* Bail if we got some sort of error when trying to retrieve the
       * value from the PTY. Perhaps something was closed.
       */
      if (pid < 0)
        {
          int errsv = errno;
          g_debug ("tcgetpgrp() failure: %s", g_strerror (errsv));
          return FALSE;
        }

      if (priv->subprocess != NULL &&
          (ident = g_subprocess_get_identifier (priv->subprocess)) &&
          (child_pid = atoi (ident)))
        return pid != child_pid;
    }

  return FALSE;
}

static void
prompt_process_real_force_exit (PromptProcess *self)
{
  PromptProcessPrivate *priv = prompt_process_get_instance_private (self);

  g_assert (PROMPT_IS_PROCESS (self));

  if (priv->subprocess)
    g_subprocess_force_exit (priv->subprocess);
}

static gboolean
prompt_process_real_get_if_exited (PromptProcess *self)
{
  PromptProcessPrivate *priv = prompt_process_get_instance_private (self);

  g_assert (PROMPT_IS_PROCESS (self));

  return g_subprocess_get_if_exited (priv->subprocess);
}

static gboolean
prompt_process_real_get_if_signaled (PromptProcess *self)
{
  PromptProcessPrivate *priv = prompt_process_get_instance_private (self);

  g_assert (PROMPT_IS_PROCESS (self));

  return g_subprocess_get_if_signaled (priv->subprocess);
}

static int
prompt_process_real_get_exit_status (PromptProcess *self)
{
  PromptProcessPrivate *priv = prompt_process_get_instance_private (self);

  g_assert (PROMPT_IS_PROCESS (self));

  return g_subprocess_get_exit_status (priv->subprocess);
}

static int
prompt_process_real_get_term_sig (PromptProcess *self)
{
  PromptProcessPrivate *priv = prompt_process_get_instance_private (self);

  g_assert (PROMPT_IS_PROCESS (self));

  return g_subprocess_get_term_sig (priv->subprocess);
}

static void
prompt_process_real_wait_check_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  GSubprocess *subprocess = (GSubprocess *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  PromptProcessPrivate *priv;
  PromptProcess *self;

  g_assert (G_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  priv = prompt_process_get_instance_private (self);

  priv->wait_completed = TRUE;

  if (!g_subprocess_wait_check_finish (subprocess, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);
}

static void
prompt_process_real_wait_check_async (PromptProcess       *self,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  PromptProcessPrivate *priv = prompt_process_get_instance_private (self);
  g_autoptr(GTask) task = NULL;

  g_assert (PROMPT_IS_PROCESS (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, prompt_process_real_wait_check_async);

  g_subprocess_wait_check_async (priv->subprocess,
                                 cancellable,
                                 prompt_process_real_wait_check_cb,
                                 g_steal_pointer (&task));
}

static gboolean
prompt_process_real_wait_check_finish (PromptProcess  *self,
                                       GAsyncResult   *result,
                                       GError        **error)
{
  g_assert (PROMPT_IS_PROCESS (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
prompt_process_finalize (GObject *object)
{
  PromptProcess *self = (PromptProcess *)object;
  PromptProcessPrivate *priv = prompt_process_get_instance_private (self);

  g_clear_object (&priv->subprocess);

  G_OBJECT_CLASS (prompt_process_parent_class)->finalize (object);
}

static void
prompt_process_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  PromptProcess *self = PROMPT_PROCESS (object);
  PromptProcessPrivate *priv = prompt_process_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_SUBPROCESS:
      g_value_set_object (value, priv->subprocess);
      break;

    case PROP_PTY:
      g_value_set_object (value, priv->pty);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_process_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  PromptProcess *self = PROMPT_PROCESS (object);
  PromptProcessPrivate *priv = prompt_process_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_SUBPROCESS:
      priv->subprocess = g_value_dup_object (value);
      break;

    case PROP_PTY:
      priv->pty = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_process_class_init (PromptProcessClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = prompt_process_finalize;
  object_class->get_property = prompt_process_get_property;
  object_class->set_property = prompt_process_set_property;

  klass->force_exit = prompt_process_real_force_exit;
  klass->get_exit_status = prompt_process_real_get_exit_status;
  klass->get_if_exited = prompt_process_real_get_if_exited;
  klass->get_if_signaled = prompt_process_real_get_if_signaled;
  klass->get_leader_kind = prompt_process_real_get_leader_kind;
  klass->get_term_sig = prompt_process_real_get_term_sig;
  klass->has_leader = prompt_process_real_has_leader;
  klass->wait_check_async = prompt_process_real_wait_check_async;
  klass->wait_check_finish = prompt_process_real_wait_check_finish;

  /**
   * PromptProcess:pty:
   *
   * The #VtePty for the process.
   *
   * This is used in the #GSubprocess case so that we can query
   * the foreground leader of the PTY. Other implementations may
   * also find it useful, even if via other means.
   */
  properties[PROP_PTY] =
    g_param_spec_object ("pty", NULL, NULL,
                         VTE_TYPE_PTY,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * PromptProcess:subprocess:
   *
   * If the process can be managed with a local #GSubprocess this
   * is a convenient way to handle things in the default manner.
   *
   * Some container systems may not be able to represent processes
   * across the container boundary and therefore must implement the
   * various virtual methods instead.
   */
  properties[PROP_SUBPROCESS] =
    g_param_spec_object ("subprocess", NULL, NULL,
                         G_TYPE_SUBPROCESS,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  exec_to_kind = g_hash_table_new (g_str_hash, g_str_equal);
#define ADD_MAPPING(name, kind) \
    g_hash_table_insert (exec_to_kind, (char *)name, GUINT_TO_POINTER(kind))
  ADD_MAPPING ("ssh", PROMPT_PROCESS_LEADER_KIND_REMOTE);
  ADD_MAPPING ("scp", PROMPT_PROCESS_LEADER_KIND_REMOTE);
  ADD_MAPPING ("sftp", PROMPT_PROCESS_LEADER_KIND_REMOTE);
  ADD_MAPPING ("telnet", PROMPT_PROCESS_LEADER_KIND_REMOTE);
  ADD_MAPPING ("toolbox", PROMPT_PROCESS_LEADER_KIND_CONTAINER);
  ADD_MAPPING ("flatpak", PROMPT_PROCESS_LEADER_KIND_CONTAINER);
  ADD_MAPPING ("podman", PROMPT_PROCESS_LEADER_KIND_CONTAINER);
  ADD_MAPPING ("docker", PROMPT_PROCESS_LEADER_KIND_CONTAINER);
#undef ADD_MAPPING
}

static void
prompt_process_init (PromptProcess *self)
{
}

PromptProcess *
prompt_process_new (GSubprocess *subprocess,
                    VtePty      *pty)
{
  g_return_val_if_fail (G_IS_SUBPROCESS (subprocess), NULL);
  g_return_val_if_fail (VTE_IS_PTY (pty), NULL);

  return g_object_new (PROMPT_TYPE_PROCESS,
                       "subprocess", subprocess,
                       "pty", pty,
                       NULL);
}

PromptProcessLeaderKind
prompt_process_get_leader_kind (PromptProcess *self)
{
  g_return_val_if_fail (PROMPT_IS_PROCESS (self), 0);

  return PROMPT_PROCESS_GET_CLASS (self)->get_leader_kind (self);
}

gboolean
prompt_process_get_if_exited (PromptProcess *self)
{
  g_return_val_if_fail (PROMPT_IS_PROCESS (self), FALSE);

  return PROMPT_PROCESS_GET_CLASS (self)->get_if_exited (self);
}

gboolean
prompt_process_get_if_signaled (PromptProcess *self)
{
  g_return_val_if_fail (PROMPT_IS_PROCESS (self), FALSE);

  return PROMPT_PROCESS_GET_CLASS (self)->get_if_signaled (self);
}

int
prompt_process_get_exit_status (PromptProcess *self)
{
  g_return_val_if_fail (PROMPT_IS_PROCESS (self), -1);

  return PROMPT_PROCESS_GET_CLASS (self)->get_exit_status (self);
}

int
prompt_process_get_term_sig (PromptProcess *self)
{
  g_return_val_if_fail (PROMPT_IS_PROCESS (self), -1);

  return PROMPT_PROCESS_GET_CLASS (self)->get_term_sig (self);
}

void
prompt_process_wait_check_async (PromptProcess       *self,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_return_if_fail (PROMPT_IS_PROCESS (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  PROMPT_PROCESS_GET_CLASS (self)->wait_check_async (self, cancellable, callback, user_data);
}

gboolean
prompt_process_wait_check_finish (PromptProcess  *self,
                                  GAsyncResult   *result,
                                  GError        **error)
{
  g_return_val_if_fail (PROMPT_IS_PROCESS (self), FALSE);

  return PROMPT_PROCESS_GET_CLASS (self)->wait_check_finish (self, result, error);
}

gboolean
prompt_process_has_leader (PromptProcess *self)
{
  g_return_val_if_fail (PROMPT_IS_PROCESS (self), FALSE);

  return PROMPT_PROCESS_GET_CLASS (self)->has_leader (self);
}

void
prompt_process_force_exit (PromptProcess *self)
{
  g_return_if_fail (PROMPT_IS_PROCESS (self));

  PROMPT_PROCESS_GET_CLASS (self)->force_exit (self);
}
