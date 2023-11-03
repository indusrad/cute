/* capsule-run-context.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "capsule-run-context"

#include "config.h"

#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "capsule-run-context.h"
#include "capsule-util.h"

typedef struct
{
  GList                     qlink;
  char                     *cwd;
  GArray                   *argv;
  GArray                   *env;
  CapsuleUnixFDMap         *unix_fd_map;
  CapsuleRunContextHandler  handler;
  gpointer                  handler_data;
  GDestroyNotify            handler_data_destroy;
} CapsuleRunContextLayer;

struct _CapsuleRunContext
{
  GObject                parent_instance;
  GQueue                 layers;
  CapsuleRunContextLayer root;
  guint                  ended : 1;
  guint                  setup_tty : 1;
};

G_DEFINE_FINAL_TYPE (CapsuleRunContext, capsule_run_context, G_TYPE_OBJECT)

CapsuleRunContext *
capsule_run_context_new (void)
{
  return g_object_new (CAPSULE_TYPE_RUN_CONTEXT, NULL);
}

static void
copy_envvar_with_fallback (CapsuleRunContext  *run_context,
                           const char * const *environ,
                           const char         *key,
                           const char         *fallback)
{
  const char *val;

  if ((val = g_environ_getenv ((char **)environ, key)))
    capsule_run_context_setenv (run_context, key, val);
  else if (fallback != NULL)
    capsule_run_context_setenv (run_context, key, fallback);
}

/**
 * capsule_run_context_add_minimal_environment:
 * @self: a #CapsuleRunContext
 *
 * Adds a minimal set of environment variables.
 *
 * This is useful to get access to things like the display or other
 * expected variables.
 */
void
capsule_run_context_add_minimal_environment (CapsuleRunContext *self)
{
  const gchar * const *host_environ = capsule_host_environ ();
  static const char *copy_env[] = {
    "AT_SPI_BUS_ADDRESS",
    "DBUS_SESSION_BUS_ADDRESS",
    "DBUS_SYSTEM_BUS_ADDRESS",
    "DESKTOP_SESSION",
    "DISPLAY",
    "LANG",
    "HOME",
    "SHELL",
    "SSH_AUTH_SOCK",
    "USER",
    "WAYLAND_DISPLAY",
    "XAUTHORITY",
    "XDG_CURRENT_DESKTOP",
    "XDG_MENU_PREFIX",
    "XDG_SEAT",
    "XDG_SESSION_DESKTOP",
    "XDG_SESSION_ID",
    "XDG_SESSION_TYPE",
    "XDG_VTNR",
  };
  const char *val;

  g_return_if_fail (CAPSULE_IS_RUN_CONTEXT (self));

  for (guint i = 0; i < G_N_ELEMENTS (copy_env); i++)
    {
      const char *key = copy_env[i];

      if ((val = g_environ_getenv ((char **)host_environ, key)))
        capsule_run_context_setenv (self, key, val);
    }

  copy_envvar_with_fallback (self, host_environ, "TERM", "xterm-256color");
  copy_envvar_with_fallback (self, host_environ, "COLORTERM", "truecolor");
}

static void
capsule_run_context_layer_clear (CapsuleRunContextLayer *layer)
{
  g_assert (layer != NULL);
  g_assert (layer->qlink.data == layer);
  g_assert (layer->qlink.prev == NULL);
  g_assert (layer->qlink.next == NULL);

  if (layer->handler_data_destroy)
    g_clear_pointer (&layer->handler_data, layer->handler_data_destroy);

  g_clear_pointer (&layer->cwd, g_free);
  g_clear_pointer (&layer->argv, g_array_unref);
  g_clear_pointer (&layer->env, g_array_unref);
  g_clear_object (&layer->unix_fd_map);
}

static void
capsule_run_context_layer_free (CapsuleRunContextLayer *layer)
{
  capsule_run_context_layer_clear (layer);

  g_slice_free (CapsuleRunContextLayer, layer);
}

static void
strptr_free (gpointer data)
{
  char **strptr = data;
  g_clear_pointer (strptr, g_free);
}

static void
capsule_run_context_layer_init (CapsuleRunContextLayer *layer)
{
  g_assert (layer != NULL);

  layer->qlink.data = layer;
  layer->argv = g_array_new (TRUE, TRUE, sizeof (char *));
  layer->env = g_array_new (TRUE, TRUE, sizeof (char *));
  layer->unix_fd_map = capsule_unix_fd_map_new ();

  g_array_set_clear_func (layer->argv, strptr_free);
  g_array_set_clear_func (layer->env, strptr_free);
}

static CapsuleRunContextLayer *
capsule_run_context_current_layer (CapsuleRunContext *self)
{
  g_assert (CAPSULE_IS_RUN_CONTEXT (self));
  g_assert (self->layers.length > 0);

  return self->layers.head->data;
}

static void
capsule_run_context_dispose (GObject *object)
{
  CapsuleRunContext *self = (CapsuleRunContext *)object;
  CapsuleRunContextLayer *layer;

  while ((layer = g_queue_peek_head (&self->layers)))
    {
      g_queue_unlink (&self->layers, &layer->qlink);
      if (layer != &self->root)
        capsule_run_context_layer_free (layer);
    }

  capsule_run_context_layer_clear (&self->root);

  G_OBJECT_CLASS (capsule_run_context_parent_class)->dispose (object);
}

static void
capsule_run_context_class_init (CapsuleRunContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = capsule_run_context_dispose;
}

static void
capsule_run_context_init (CapsuleRunContext *self)
{
  capsule_run_context_layer_init (&self->root);

  g_queue_push_head_link (&self->layers, &self->root.qlink);

  self->setup_tty = TRUE;
}

void
capsule_run_context_push (CapsuleRunContext        *self,
                          CapsuleRunContextHandler  handler,
                          gpointer                  handler_data,
                          GDestroyNotify            handler_data_destroy)
{
  CapsuleRunContextLayer *layer;

  g_return_if_fail (CAPSULE_IS_RUN_CONTEXT (self));

  layer = g_slice_new0 (CapsuleRunContextLayer);

  capsule_run_context_layer_init (layer);

  layer->handler = handler;
  layer->handler_data = handler_data;
  layer->handler_data_destroy = handler_data_destroy;

  g_queue_push_head_link (&self->layers, &layer->qlink);
}

void
capsule_run_context_push_at_base (CapsuleRunContext        *self,
                                  CapsuleRunContextHandler  handler,
                                  gpointer                  handler_data,
                                  GDestroyNotify            handler_data_destroy)
{
  CapsuleRunContextLayer *layer;

  g_return_if_fail (CAPSULE_IS_RUN_CONTEXT (self));

  layer = g_slice_new0 (CapsuleRunContextLayer);

  capsule_run_context_layer_init (layer);

  layer->handler = handler;
  layer->handler_data = handler_data;
  layer->handler_data_destroy = handler_data_destroy;

  g_queue_insert_before_link (&self->layers, &self->root.qlink, &layer->qlink);
}

static gboolean
capsule_run_context_host_handler (CapsuleRunContext   *self,
                                  const char * const  *argv,
                                  const char * const  *env,
                                  const char          *cwd,
                                  CapsuleUnixFDMap    *unix_fd_map,
                                  gpointer             user_data,
                                  GError             **error)
{
  guint length;

  g_assert (CAPSULE_IS_RUN_CONTEXT (self));
  g_assert (argv != NULL);
  g_assert (env != NULL);
  g_assert (CAPSULE_IS_UNIX_FD_MAP (unix_fd_map));
  g_assert (capsule_get_process_kind () == CAPSULE_PROCESS_KIND_FLATPAK);

  capsule_run_context_append_argv (self, "flatpak-spawn");
  capsule_run_context_append_argv (self, "--host");
  capsule_run_context_append_argv (self, "--watch-bus");

  if (env != NULL)
    {
      for (guint i = 0; env[i]; i++)
        capsule_run_context_append_formatted (self, "--env=%s", env[i]);
    }

  if (cwd != NULL)
    capsule_run_context_append_formatted (self, "--directory=%s", cwd);

  if ((length = capsule_unix_fd_map_get_length (unix_fd_map)))
    {
      for (guint i = 0; i < length; i++)
        {
          int source_fd;
          int dest_fd;

          source_fd = capsule_unix_fd_map_peek (unix_fd_map, i, &dest_fd);

          if (dest_fd < STDERR_FILENO)
            continue;

          g_debug ("Mapping Builder FD %d to target FD %d via flatpak-spawn",
                   source_fd, dest_fd);

          if (source_fd != -1 && dest_fd != -1)
            capsule_run_context_append_formatted (self, "--forward-fd=%d", dest_fd);
        }

      if (!capsule_run_context_merge_unix_fd_map (self, unix_fd_map, error))
        return FALSE;
    }

  /* Now append the arguments */
  capsule_run_context_append_args (self, argv);

  return TRUE;
}

/**
 * capsule_run_context_push_host:
 * @self: a #CapsuleRunContext
 *
 * Pushes handler to transform command to run on host.
 *
 * If necessary, a layer is pushed to ensure the command is run on the
 * host instead of the application container.
 *
 * If Builder is running on the host already, this function does nothing.
 */
void
capsule_run_context_push_host (CapsuleRunContext *self)
{
  g_return_if_fail (CAPSULE_IS_RUN_CONTEXT (self));

  if (capsule_get_process_kind () == CAPSULE_PROCESS_KIND_FLATPAK)
    {
      self->setup_tty = FALSE;
      capsule_run_context_push (self,
                            capsule_run_context_host_handler,
                            NULL,
                            NULL);
    }
}

typedef struct
{
  char *shell;
  CapsuleRunContextShell kind : 2;
} Shell;

static void
shell_free (gpointer data)
{
  Shell *shell = data;
  g_clear_pointer (&shell->shell, g_free);
  g_slice_free (Shell, shell);
}

static gboolean
capsule_run_context_shell_handler (CapsuleRunContext   *self,
                                   const char * const  *argv,
                                   const char * const  *env,
                                   const char          *cwd,
                                   CapsuleUnixFDMap    *unix_fd_map,
                                   gpointer             user_data,
                                   GError             **error)
{
  Shell *shell = user_data;
  g_autoptr(GString) str = NULL;

  g_assert (CAPSULE_IS_RUN_CONTEXT (self));
  g_assert (argv != NULL);
  g_assert (env != NULL);
  g_assert (CAPSULE_IS_UNIX_FD_MAP (unix_fd_map));
  g_assert (shell != NULL);
  g_assert (shell->shell != NULL);

  if (!capsule_run_context_merge_unix_fd_map (self, unix_fd_map, error))
    return FALSE;

  if (cwd != NULL)
    capsule_run_context_set_cwd (self, cwd);

  capsule_run_context_append_argv (self, shell->shell);
  if (shell->kind == CAPSULE_RUN_CONTEXT_SHELL_LOGIN)
    capsule_run_context_append_argv (self, "-l");
  else if (shell->kind == CAPSULE_RUN_CONTEXT_SHELL_INTERACTIVE)
    capsule_run_context_append_argv (self, "-i");
  capsule_run_context_append_argv (self, "-c");

  str = g_string_new (NULL);

  if (env[0] != NULL)
    {
      g_string_append (str, "env");

      for (guint i = 0; env[i]; i++)
        {
          g_autofree char *quoted = g_shell_quote (env[i]);

          g_string_append_c (str, ' ');
          g_string_append (str, quoted);
        }

      g_string_append_c (str, ' ');
    }

  for (guint i = 0; argv[i]; i++)
    {
      g_autofree char *quoted = g_shell_quote (argv[i]);

      if (i > 0)
        g_string_append_c (str, ' ');
      g_string_append (str, quoted);
    }

  capsule_run_context_append_argv (self, str->str);

  return TRUE;
}

/**
 * capsule_run_context_push_shell:
 * @self: a #CapsuleRunContext
 * @shell: the kind of shell to be used
 *
 * Pushes a shell which can run the upper layer command with -c.
 */
void
capsule_run_context_push_shell (CapsuleRunContext      *self,
                                CapsuleRunContextShell  shell)
{
  Shell *state;

  g_return_if_fail (CAPSULE_IS_RUN_CONTEXT (self));

  state = g_slice_new0 (Shell);
  state->shell = g_strdup ("/bin/sh");
  state->kind = shell;

  capsule_run_context_push (self, capsule_run_context_shell_handler, state, shell_free);
}

static gboolean
capsule_run_context_error_handler (CapsuleRunContext   *self,
                                   const char * const  *argv,
                                   const char * const  *env,
                                   const char          *cwd,
                                   CapsuleUnixFDMap    *unix_fd_map,
                                   gpointer             user_data,
                                   GError             **error)
{
  const GError *local_error = user_data;

  g_assert (CAPSULE_IS_RUN_CONTEXT (self));
  g_assert (CAPSULE_IS_UNIX_FD_MAP (unix_fd_map));
  g_assert (local_error != NULL);

  if (error != NULL)
    *error = g_error_copy (local_error);

  return FALSE;
}

/**
 * capsule_run_context_push_error:
 * @self: a #CapsuleRunContext
 * @error: (transfer full) (in): a #GError
 *
 * Pushes a new layer that will always fail with @error.
 *
 * This is useful if you have an error when attempting to build
 * a run command, but need it to deliver the error when attempting
 * to create a subprocess launcher.
 */
void
capsule_run_context_push_error (CapsuleRunContext *self,
                                GError            *error)
{
  g_return_if_fail (CAPSULE_IS_RUN_CONTEXT (self));
  g_return_if_fail (error != NULL);

  capsule_run_context_push (self,
                        capsule_run_context_error_handler,
                        error,
                        (GDestroyNotify)g_error_free);
}

static gboolean
next_variable (const char *str,
               guint      *cursor,
               guint      *begin)
{
  for (guint i = *cursor; str[i]; i++)
    {
      /* Skip past escaped $ */
      if (str[i] == '\\' && str[i+1] == '$')
        {
          i++;
          continue;
        }

      if (str[i] == '$')
        {
          *begin = i;
          *cursor = i;

          for (guint j = i+1; str[j]; j++)
            {
              if (!g_ascii_isalnum (str[j]) && str[j] != '_')
                {
                  *cursor = j;
                  break;
                }
            }

          if (*cursor > ((*begin) + 1))
            return TRUE;
        }
    }

  return FALSE;
}

static char *
wordexp_with_environ (const char         *input,
                      const char * const *environ)
{
  g_autoptr(GString) str = NULL;
  guint cursor = 0;
  guint begin;

  g_assert (input != NULL);
  g_assert (environ != NULL);

  str = g_string_new (input);

  while (next_variable (str->str, &cursor, &begin))
    {
      g_autofree char *key = NULL;
      guint key_len = cursor - begin;
      const char *value;
      guint value_len;

      g_assert (str->str[begin] == '$');

      key = g_strndup (str->str + begin, key_len);
      value = g_environ_getenv ((char **)environ, key+1);
      value_len = value ? strlen (value) : 0;

      if (value != NULL)
        {
          g_string_erase (str, begin, key_len);
          g_string_insert_len (str, begin, value, value_len);

          if (value_len > key_len)
            cursor += (value_len - key_len);
          else if (value_len < key_len)
            cursor -= (key_len - value_len);
        }
    }

  return g_string_free (g_steal_pointer (&str), FALSE);
}

static gboolean
capsule_run_context_expansion_handler (CapsuleRunContext   *self,
                                       const char * const  *argv,
                                       const char * const  *env,
                                       const char          *cwd,
                                       CapsuleUnixFDMap    *unix_fd_map,
                                       gpointer             user_data,
                                       GError             **error)
{
  const char * const *environ = user_data;

  g_assert (CAPSULE_IS_RUN_CONTEXT (self));
  g_assert (argv != NULL);
  g_assert (environ != NULL);
  g_assert (CAPSULE_IS_UNIX_FD_MAP (unix_fd_map));

  if (!capsule_run_context_merge_unix_fd_map (self, unix_fd_map, error))
    return FALSE;

  if (cwd != NULL)
    {
      g_autofree char *newcwd = wordexp_with_environ (cwd, environ);
      g_autofree char *expanded = capsule_path_expand (newcwd);

      capsule_run_context_set_cwd (self, expanded);
    }

  if (env != NULL)
    {
      g_autoptr(GArray) newenv = g_array_new (TRUE, TRUE, sizeof (char *));

      for (guint i = 0; env[i]; i++)
        {
          char *expanded = wordexp_with_environ (env[i], environ);
          g_array_append_val (newenv, expanded);
        }

      capsule_run_context_add_environ (self, (const char * const *)(gpointer)newenv->data);
    }

  if (argv != NULL)
    {
      g_autoptr(GArray) newargv = g_array_new (TRUE, TRUE, sizeof (char *));

      for (guint i = 0; argv[i]; i++)
        {
          char *expanded = wordexp_with_environ (argv[i], environ);
          g_array_append_val (newargv, expanded);
        }

      capsule_run_context_append_args (self, (const char * const *)(gpointer)newargv->data);
    }

  return TRUE;
}

/**
 * capsule_run_context_push_expansion:
 * @self: a #CapsuleRunContext
 *
 * Pushes a layer to expand known environment variables.
 *
 * The command argv and cwd will have `$FOO` style environment
 * variables expanded that are known. This can be useful to allow
 * things like `$BUILDDIR` be expanded at this layer.
 */
void
capsule_run_context_push_expansion (CapsuleRunContext  *self,
                                    const char * const *environ)
{
  g_return_if_fail (CAPSULE_IS_RUN_CONTEXT (self));

  if (environ != NULL)
    capsule_run_context_push (self,
                          capsule_run_context_expansion_handler,
                          g_strdupv ((char **)environ),
                          (GDestroyNotify)g_strfreev);
}

const char * const *
capsule_run_context_get_argv (CapsuleRunContext *self)
{
  CapsuleRunContextLayer *layer;

  g_return_val_if_fail (CAPSULE_IS_RUN_CONTEXT (self), NULL);

  layer = capsule_run_context_current_layer (self);

  return (const char * const *)(gpointer)layer->argv->data;
}

void
capsule_run_context_set_argv (CapsuleRunContext  *self,
                              const char * const *argv)
{
  CapsuleRunContextLayer *layer;

  g_return_if_fail (CAPSULE_IS_RUN_CONTEXT (self));

  layer = capsule_run_context_current_layer (self);

  g_array_set_size (layer->argv, 0);

  if (argv != NULL)
    {
      char **copy = g_strdupv ((char **)argv);
      g_array_append_vals (layer->argv, copy, g_strv_length (copy));
      g_free (copy);
    }
}

const char * const *
capsule_run_context_get_environ (CapsuleRunContext *self)
{
  CapsuleRunContextLayer *layer;

  g_return_val_if_fail (CAPSULE_IS_RUN_CONTEXT (self), NULL);

  layer = capsule_run_context_current_layer (self);

  return (const char * const *)(gpointer)layer->env->data;
}

void
capsule_run_context_set_environ (CapsuleRunContext  *self,
                                 const char * const *environ)
{
  CapsuleRunContextLayer *layer;

  g_return_if_fail (CAPSULE_IS_RUN_CONTEXT (self));

  layer = capsule_run_context_current_layer (self);

  g_array_set_size (layer->env, 0);

  if (environ != NULL && environ[0] != NULL)
    {
      char **copy = g_strdupv ((char **)environ);
      g_array_append_vals (layer->env, copy, g_strv_length (copy));
      g_free (copy);
    }
}

void
capsule_run_context_add_environ (CapsuleRunContext  *self,
                                 const char * const *environ)
{
  CapsuleRunContextLayer *layer;

  g_return_if_fail (CAPSULE_IS_RUN_CONTEXT (self));

  if (environ == NULL || environ[0] == NULL)
    return;

  layer = capsule_run_context_current_layer (self);

  for (guint i = 0; environ[i]; i++)
    {
      const char *pair = environ[i];
      const char *eq = strchr (pair, '=');
      char **dest = NULL;
      gsize keylen;

      if (eq == NULL)
        continue;

      keylen = eq - pair;

      for (guint j = 0; j < layer->env->len; j++)
        {
          const char *ele = g_array_index (layer->env, const char *, j);

          if (strncmp (pair, ele, keylen) == 0 && ele[keylen] == '=')
            {
              dest = &g_array_index (layer->env, char *, j);
              break;
            }
        }

      if (dest == NULL)
        {
          g_array_set_size (layer->env, layer->env->len + 1);
          dest = &g_array_index (layer->env, char *, layer->env->len - 1);
        }

      g_clear_pointer (dest, g_free);
      *dest = g_strdup (pair);
    }
}

const char *
capsule_run_context_get_cwd (CapsuleRunContext *self)
{
  CapsuleRunContextLayer *layer;

  g_return_val_if_fail (CAPSULE_IS_RUN_CONTEXT (self), NULL);

  layer = capsule_run_context_current_layer (self);

  return layer->cwd;
}

void
capsule_run_context_set_cwd (CapsuleRunContext *self,
                             const char        *cwd)
{
  CapsuleRunContextLayer *layer;

  g_return_if_fail (CAPSULE_IS_RUN_CONTEXT (self));

  layer = capsule_run_context_current_layer (self);

  g_set_str (&layer->cwd, cwd);
}

void
capsule_run_context_prepend_argv (CapsuleRunContext *self,
                                  const char        *arg)
{
  CapsuleRunContextLayer *layer;
  char *copy;

  g_return_if_fail (CAPSULE_IS_RUN_CONTEXT (self));
  g_return_if_fail (arg != NULL);

  layer = capsule_run_context_current_layer (self);

  copy = g_strdup (arg);
  g_array_insert_val (layer->argv, 0, copy);
}

void
capsule_run_context_prepend_args (CapsuleRunContext  *self,
                                  const char * const *args)
{
  CapsuleRunContextLayer *layer;
  char **copy;

  g_return_if_fail (CAPSULE_IS_RUN_CONTEXT (self));

  if (args == NULL || args[0] == NULL)
    return;

  layer = capsule_run_context_current_layer (self);

  copy = g_strdupv ((char **)args);
  g_array_insert_vals (layer->argv, 0, copy, g_strv_length (copy));
  g_free (copy);
}

void
capsule_run_context_append_argv (CapsuleRunContext *self,
                                 const char        *arg)
{
  CapsuleRunContextLayer *layer;
  char *copy;

  g_return_if_fail (CAPSULE_IS_RUN_CONTEXT (self));
  g_return_if_fail (arg != NULL);

  layer = capsule_run_context_current_layer (self);

  copy = g_strdup (arg);
  g_array_append_val (layer->argv, copy);
}

void
capsule_run_context_append_formatted (CapsuleRunContext *self,
                                      const char        *format,
                                      ...)
{
  g_autofree char *arg = NULL;
  va_list args;

  g_return_if_fail (CAPSULE_IS_RUN_CONTEXT (self));
  g_return_if_fail (format != NULL);

  va_start (args, format);
  arg = g_strdup_vprintf (format, args);
  va_end (args);

  capsule_run_context_append_argv (self, arg);
}

void
capsule_run_context_append_args (CapsuleRunContext  *self,
                                 const char * const *args)
{
  CapsuleRunContextLayer *layer;
  char **copy;

  g_return_if_fail (CAPSULE_IS_RUN_CONTEXT (self));

  if (args == NULL || args[0] == NULL)
    return;

  layer = capsule_run_context_current_layer (self);

  copy = g_strdupv ((char **)args);
  g_array_append_vals (layer->argv, copy, g_strv_length (copy));
  g_free (copy);
}

gboolean
capsule_run_context_append_args_parsed (CapsuleRunContext  *self,
                                        const char         *args,
                                        GError            **error)
{
  CapsuleRunContextLayer *layer;
  char **argv = NULL;
  int argc;

  g_return_val_if_fail (CAPSULE_IS_RUN_CONTEXT (self), FALSE);
  g_return_val_if_fail (args != NULL, FALSE);

  layer = capsule_run_context_current_layer (self);

  if (!g_shell_parse_argv (args, &argc, &argv, error))
    return FALSE;

  g_array_append_vals (layer->argv, argv, argc);
  g_free (argv);

  return TRUE;
}

void
capsule_run_context_take_fd (CapsuleRunContext *self,
                             int                source_fd,
                             int                dest_fd)
{
  CapsuleRunContextLayer *layer;

  g_return_if_fail (CAPSULE_IS_RUN_CONTEXT (self));
  g_return_if_fail (source_fd >= -1);
  g_return_if_fail (dest_fd > -1);

  layer = capsule_run_context_current_layer (self);

  capsule_unix_fd_map_take (layer->unix_fd_map, source_fd, dest_fd);
}

const char *
capsule_run_context_getenv (CapsuleRunContext *self,
                            const char        *key)
{
  CapsuleRunContextLayer *layer;
  gsize keylen;

  g_return_val_if_fail (CAPSULE_IS_RUN_CONTEXT (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  layer = capsule_run_context_current_layer (self);

  keylen = strlen (key);

  for (guint i = 0; i < layer->env->len; i++)
    {
      const char *envvar = g_array_index (layer->env, const char *, i);

      if (strncmp (key, envvar, keylen) == 0 && envvar[keylen] == '=')
        return &envvar[keylen+1];
    }

  return NULL;
}

void
capsule_run_context_setenv (CapsuleRunContext *self,
                            const char        *key,
                            const char        *value)
{
  CapsuleRunContextLayer *layer;
  char *element;
  gsize keylen;

  g_return_if_fail (CAPSULE_IS_RUN_CONTEXT (self));
  g_return_if_fail (key != NULL);

  if (value == NULL)
    {
      capsule_run_context_unsetenv (self, key);
      return;
    }

  layer = capsule_run_context_current_layer (self);

  keylen = strlen (key);
  element = g_strconcat (key, "=", value, NULL);

  g_array_append_val (layer->env, element);

  for (guint i = 0; i < layer->env->len-1; i++)
    {
      const char *envvar = g_array_index (layer->env, const char *, i);

      if (strncmp (key, envvar, keylen) == 0 && envvar[keylen] == '=')
        {
          g_array_remove_index_fast (layer->env, i);
          break;
        }
    }
}

void
capsule_run_context_unsetenv (CapsuleRunContext *self,
                              const char        *key)
{
  CapsuleRunContextLayer *layer;
  gsize len;

  g_return_if_fail (CAPSULE_IS_RUN_CONTEXT (self));
  g_return_if_fail (key != NULL);

  layer = capsule_run_context_current_layer (self);

  len = strlen (key);

  for (guint i = 0; i < layer->env->len; i++)
    {
      const char *envvar = g_array_index (layer->env, const char *, i);

      if (strncmp (key, envvar, len) == 0 && envvar[len] == '=')
        {
          g_array_remove_index_fast (layer->env, i);
          return;
        }
    }
}

void
capsule_run_context_environ_to_argv (CapsuleRunContext *self)
{
  CapsuleRunContextLayer *layer;
  const char **copy;

  g_assert (CAPSULE_IS_RUN_CONTEXT (self));

  layer = capsule_run_context_current_layer (self);

  if (layer->env->len == 0)
    return;

  copy = (const char **)g_new0 (char *, layer->env->len + 2);
  copy[0] = "env";
  for (guint i = 0; i < layer->env->len; i++)
    copy[1+i] = g_array_index (layer->env, const char *, i);
  capsule_run_context_prepend_args (self, (const char * const *)copy);
  g_free (copy);

  g_array_set_size (layer->env, 0);
}

static gboolean
capsule_run_context_default_handler (CapsuleRunContext   *self,
                                     const char * const  *argv,
                                     const char * const  *env,
                                     const char          *cwd,
                                     CapsuleUnixFDMap    *unix_fd_map,
                                     gpointer             user_data,
                                     GError             **error)
{
  CapsuleRunContextLayer *layer;

  g_assert (CAPSULE_IS_RUN_CONTEXT (self));
  g_assert (argv != NULL);
  g_assert (env != NULL);
  g_assert (CAPSULE_IS_UNIX_FD_MAP (unix_fd_map));

  layer = capsule_run_context_current_layer (self);

  if (cwd != NULL)
    {
      /* If the working directories do not match, we can't satisfy this and
       * need to error out.
       */
      if (layer->cwd != NULL && !g_str_equal (cwd, layer->cwd))
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Cannot resolve differently requested cwd: %s and %s",
                       cwd, layer->cwd);
          return FALSE;
        }

      capsule_run_context_set_cwd (self, cwd);
    }

  /* Merge all the FDs unless there are collisions */
  if (!capsule_unix_fd_map_steal_from (layer->unix_fd_map, unix_fd_map, error))
    return FALSE;

  if (env[0] != NULL)
    {
      if (argv[0] == NULL)
        {
          capsule_run_context_add_environ (self, env);
        }
      else
        {
          capsule_run_context_append_argv (self, "env");
          capsule_run_context_append_args (self, env);
        }
    }

  if (argv[0] != NULL)
    capsule_run_context_append_args (self, argv);

  return TRUE;
}

static int
sort_strptr (gconstpointer a,
             gconstpointer b)
{
  const char * const *astr = a;
  const char * const *bstr = b;

  return g_strcmp0 (*astr, *bstr);
}

static gboolean
capsule_run_context_callback_layer (CapsuleRunContext       *self,
                                    CapsuleRunContextLayer  *layer,
                                    GError                 **error)
{
  CapsuleRunContextHandler handler;
  gpointer handler_data;
  gboolean ret;

  g_assert (CAPSULE_IS_RUN_CONTEXT (self));
  g_assert (layer != NULL);
  g_assert (layer != &self->root);

  handler = layer->handler ? layer->handler : capsule_run_context_default_handler;
  handler_data = layer->handler ? layer->handler_data : NULL;

  /* Sort environment variables first so that we have an easier time
   * finding them by eye in tooling which translates them.
   */
  g_array_sort (layer->env, sort_strptr);

  ret = handler (self,
                 (const char * const *)(gpointer)layer->argv->data,
                 (const char * const *)(gpointer)layer->env->data,
                 layer->cwd,
                 layer->unix_fd_map,
                 handler_data,
                 error);

  capsule_run_context_layer_free (layer);

  return ret;
}

static void
capsule_run_context_child_setup_cb (gpointer data)
{
#ifdef G_OS_UNIX
  setsid ();
  setpgid (0, 0);
#endif
}

static void
capsule_run_context_child_setup_with_tty_cb (gpointer data)
{
  capsule_run_context_child_setup_cb (data);

  if (isatty (STDIN_FILENO))
    ioctl (STDIN_FILENO, TIOCSCTTY, 0);
}

/**
 * capsule_run_context_spawn:
 * @self: a #CapsuleRunContext
 *
 * Returns: (transfer full): an #GSubprocessLauncher if successful; otherwise
 *   %NULL and @error is set.
 */
GSubprocess *
capsule_run_context_spawn (CapsuleRunContext  *self,
                           GError            **error)
{
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  const char * const *argv;
  GSubprocessFlags flags = 0;
  guint length;

  g_return_val_if_fail (CAPSULE_IS_RUN_CONTEXT (self), NULL);
  g_return_val_if_fail (self->ended == FALSE, NULL);

  self->ended = TRUE;

  while (self->layers.length > 1)
    {
      CapsuleRunContextLayer *layer = capsule_run_context_current_layer (self);

      g_queue_unlink (&self->layers, &layer->qlink);

      if (!capsule_run_context_callback_layer (self, layer, error))
        return FALSE;
    }

  argv = capsule_run_context_get_argv (self);

  launcher = g_subprocess_launcher_new (0);

  g_subprocess_launcher_set_environ (launcher, (char **)capsule_run_context_get_environ (self));
  g_subprocess_launcher_set_cwd (launcher, capsule_run_context_get_cwd (self));

  length = capsule_unix_fd_map_get_length (self->root.unix_fd_map);

  for (guint i = 0; i < length; i++)
    {
      int source_fd;
      int dest_fd;

      source_fd = capsule_unix_fd_map_steal (self->root.unix_fd_map, i, &dest_fd);

      if (dest_fd == STDOUT_FILENO && source_fd == -1)
        flags |= G_SUBPROCESS_FLAGS_STDOUT_SILENCE;

      if (dest_fd == STDERR_FILENO && source_fd == -1)
        flags |= G_SUBPROCESS_FLAGS_STDERR_SILENCE;

      if (source_fd != -1 && dest_fd != -1)
        {
          if (dest_fd == STDIN_FILENO)
            g_subprocess_launcher_take_stdin_fd (launcher, source_fd);
          else if (dest_fd == STDOUT_FILENO)
            g_subprocess_launcher_take_stdout_fd (launcher, source_fd);
          else if (dest_fd == STDERR_FILENO)
            g_subprocess_launcher_take_stderr_fd (launcher, source_fd);
          else
            g_subprocess_launcher_take_fd (launcher, source_fd, dest_fd);
        }
    }

  g_subprocess_launcher_set_flags (launcher, flags);

  if (self->setup_tty)
    g_subprocess_launcher_set_child_setup (launcher,
                                           capsule_run_context_child_setup_with_tty_cb,
                                           NULL, NULL);
  else
    g_subprocess_launcher_set_child_setup (launcher,
                                           capsule_run_context_child_setup_cb,
                                           NULL, NULL);

  return g_subprocess_launcher_spawnv (launcher, argv, error);
}

/**
 * capsule_run_context_merge_unix_fd_map:
 * @self: a #CapsuleRunContext
 * @unix_fd_map: a #CapsuleUnixFDMap
 * @error: a #GError, or %NULL
 *
 * Merges the #CapsuleUnixFDMap into the current layer.
 *
 * If there are collisions in destination FDs, then that may cause an
 * error and %FALSE is returned.
 *
 * @unix_fd_map will have the FDs stolen using capsule_unix_fd_map_steal_from()
 * which means that if successful, @unix_fd_map will not have any open
 * file-descriptors after calling this function.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
capsule_run_context_merge_unix_fd_map (CapsuleRunContext  *self,
                                       CapsuleUnixFDMap   *unix_fd_map,
                                       GError            **error)
{
  CapsuleRunContextLayer *layer;

  g_return_val_if_fail (CAPSULE_IS_RUN_CONTEXT (self), FALSE);
  g_return_val_if_fail (CAPSULE_IS_UNIX_FD_MAP (unix_fd_map), FALSE);

  layer = capsule_run_context_current_layer (self);

  return capsule_unix_fd_map_steal_from (layer->unix_fd_map, unix_fd_map, error);
}

/**
 * capsule_run_context_set_pty_fd:
 * @self: an #CapsuleRunContext
 * @consumer_fd: the FD of the PTY consumer
 *
 * Sets up a PTY for the run context that will communicate with the
 * consumer. The consumer is the generally the widget that is rendering
 * the PTY contents and the producer is the FD that is connected to the
 * subprocess.
 */
void
capsule_run_context_set_pty_fd (CapsuleRunContext *self,
                                int                consumer_fd)
{
  int stdin_fd = -1;
  int stdout_fd = -1;
  int stderr_fd = -1;

  g_return_if_fail (CAPSULE_IS_RUN_CONTEXT (self));

  if (consumer_fd < 0)
    return;

  if (-1 == (stdin_fd = capsule_pty_create_producer (consumer_fd, TRUE)))
    {
      int errsv = errno;
      g_critical ("Failed to create PTY device: %s", g_strerror (errsv));
      return;
    }

  if (-1 == (stdout_fd = dup (stdin_fd)))
    {
      int errsv = errno;
      g_critical ("Failed to dup stdout FD: %s", g_strerror (errsv));
    }

  if (-1 == (stderr_fd = dup (stdin_fd)))
    {
      int errsv = errno;
      g_critical ("Failed to dup stderr FD: %s", g_strerror (errsv));
    }

  g_assert (stdin_fd > -1);
  g_assert (stdout_fd > -1);
  g_assert (stderr_fd > -1);

  capsule_run_context_take_fd (self, stdin_fd, STDIN_FILENO);
  capsule_run_context_take_fd (self, stdout_fd, STDOUT_FILENO);
  capsule_run_context_take_fd (self, stderr_fd, STDERR_FILENO);
}

/**
 * capsule_run_context_set_pty:
 * @self: a #CapsuleRunContext
 *
 * Sets the PTY for a run context.
 */
void
capsule_run_context_set_pty (CapsuleRunContext *self,
                             VtePty            *pty)
{
  int consumer_fd;

  g_return_if_fail (CAPSULE_IS_RUN_CONTEXT (self));
  g_return_if_fail (VTE_IS_PTY (pty));

  consumer_fd = vte_pty_get_fd (pty);

  if (consumer_fd != -1)
    capsule_run_context_set_pty_fd (self, consumer_fd);
}

/**
 * capsule_run_context_create_stdio_stream:
 * @self: a #CapsuleRunContext
 * @error: a location for a #GError
 *
 * Creates a stream to communicate with the subprocess using stdin/stdout.
 *
 * The stream is created using UNIX pipes which are attached to the
 * stdin/stdout of the child process.
 *
 * Returns: (transfer full): a #GIOStream if successful; otherwise
 *   %NULL and @error is set.
 */
GIOStream *
capsule_run_context_create_stdio_stream (CapsuleRunContext  *self,
                                         GError            **error)
{
  CapsuleRunContextLayer *layer;

  g_return_val_if_fail (CAPSULE_IS_RUN_CONTEXT (self), NULL);

  layer = capsule_run_context_current_layer (self);

  return capsule_unix_fd_map_create_stream (layer->unix_fd_map,
                                            STDIN_FILENO,
                                            STDOUT_FILENO,
                                            error);
}


