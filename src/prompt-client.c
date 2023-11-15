/*
 * prompt-client.c
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
#include <sys/ioctl.h>
#ifdef __linux__
# include <sys/prctl.h>
#endif
#include <sys/socket.h>

#include <glib/gstdio.h>
#include <glib-unix.h>

#include "prompt-agent-ipc.h"
#include "prompt-client.h"
#include "prompt-util.h"

struct _PromptClient
{
  GObject          parent_instance;
  GPtrArray       *containers;
  GSubprocess     *subprocess;
  GDBusConnection *bus;
  PromptIpcAgent  *proxy;
};

enum {
  PROP_0,
  PROP_N_ITEMS,
  N_PROPS
};

static GType
prompt_client_get_item_type (GListModel *model)
{
  return PROMPT_IPC_TYPE_CONTAINER;
}

static guint
prompt_client_get_n_items (GListModel *model)
{
  return PROMPT_CLIENT (model)->containers->len;
}

static gpointer
prompt_client_get_item (GListModel *model,
                        guint       position)
{
  PromptClient *self = PROMPT_CLIENT (model);

  if (position < self->containers->len)
    return g_object_ref (g_ptr_array_index (self->containers, position));

  return NULL;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = prompt_client_get_item_type;
  iface->get_n_items = prompt_client_get_n_items;
  iface->get_item = prompt_client_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (PromptClient, prompt_client, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GParamSpec *properties [N_PROPS];

static void
prompt_client_dispose (GObject *object)
{
  PromptClient *self = (PromptClient *)object;

  if (self->containers->len > 0)
    g_ptr_array_remove_range (self->containers, 0, self->containers->len);

  g_clear_object (&self->bus);
  g_clear_object (&self->proxy);
  g_clear_object (&self->subprocess);

  G_OBJECT_CLASS (prompt_client_parent_class)->dispose (object);
}

static void
prompt_client_finalize (GObject *object)
{
  PromptClient *self = (PromptClient *)object;

  g_clear_pointer (&self->containers, g_ptr_array_unref);

  G_OBJECT_CLASS (prompt_client_parent_class)->finalize (object);
}

static void
prompt_client_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  PromptClient *self = PROMPT_CLIENT (object);

  switch (prop_id)
    {
    case PROP_N_ITEMS:
      g_value_set_uint (value, self->containers->len);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_client_class_init (PromptClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = prompt_client_dispose;
  object_class->finalize = prompt_client_finalize;
  object_class->get_property = prompt_client_get_property;

  properties[PROP_N_ITEMS] =
    g_param_spec_uint ("n-items", NULL, NULL,
                       0, G_MAXUINT-1, 0,
                       (G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
prompt_client_init (PromptClient *self)
{
}

static void
prompt_client_containers_changed_cb (PromptClient       *self,
                                     guint               position,
                                     guint               removed,
                                     const char * const *added,
                                     PromptIpcAgent     *agent)
{
  guint added_len;

  g_assert (PROMPT_IS_CLIENT (self));
  g_assert (PROMPT_IPC_IS_AGENT (agent));

  if (removed > 0)
    g_ptr_array_remove_range (self->containers, position, removed);

  added_len = g_strv_length ((char **)added);

  for (guint i = 0; i < added_len; i++)
    {
      g_autoptr(PromptIpcContainer) container = NULL;
      g_autoptr(GError) error = NULL;

      container = prompt_ipc_container_proxy_new_sync (self->bus,
                                                       G_DBUS_PROXY_FLAGS_NONE,
                                                       NULL,
                                                       added[i],
                                                       NULL,
                                                       &error);

      g_ptr_array_insert (self->containers, position + i, g_steal_pointer (&container));
    }

  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added_len);

  if (removed != added_len)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_ITEMS]);
}

static char *
find_prompt_agent_path (void)
{
  if (prompt_get_process_kind () == PROMPT_PROCESS_KIND_FLATPAK)
    {
      g_autofree char *contents = NULL;
      gsize len;

      if (g_file_get_contents ("/.flatpak-info", &contents, &len, NULL))
        {
          g_autoptr(GKeyFile) key_file = g_key_file_new ();
          g_autofree char *str = NULL;

          if (g_key_file_load_from_data (key_file, contents, len, 0, NULL) &&
              (str = g_key_file_get_string (key_file, "Instance", "app-path", NULL)))
            return g_build_filename (str, "libexec", "prompt-agent", NULL);
        }
    }

  return g_build_filename (LIBEXECDIR, "prompt-agent", NULL);
}

static void
prompt_client_wait_cb (GObject      *object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  GSubprocess *subprocess = (GSubprocess *)object;
  g_autoptr(PromptClient) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (PROMPT_IS_CLIENT (self));

  /* TODO:
   *
   * There isn't much we can do to recover here because without the peer we
   * can't monitor when processes exit/etc. Depending on how things go, we may
   * want to allow still interacting locally with whatever tabs are open, but
   * notify the user that there is pretty much no ability to continue.
   *
   * Another option could be a "terminal crashed" view in the PromptTab.
   */

  if (!g_subprocess_wait_check_finish (subprocess, result, &error))
    g_error ("%s", error->message);
  else
    g_error ("prompt-agent exited, cannot continue");
}

static void
prompt_client_child_setup_func (gpointer data)
{
  setsid ();
  setpgid (0, 0);

#ifdef __linux__
  prctl (PR_SET_PDEATHSIG, SIGKILL);
#endif
}

PromptClient *
prompt_client_new (GError **error)
{
  g_autofree char *prompt_agent_path = find_prompt_agent_path ();
  PromptClient *self = g_object_new (PROMPT_TYPE_CLIENT, NULL);
  g_autoptr(GSubprocessLauncher) launcher = g_subprocess_launcher_new (0);
  g_autoptr(GPtrArray) argv = g_ptr_array_new_with_free_func (g_free);
  g_autoptr(GDBusConnection) bus = NULL;
  g_autoptr(GSocketConnection) stream = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GSocket) socket = NULL;
  int pair[2];
  int res;

  if (prompt_get_process_kind () == PROMPT_PROCESS_KIND_FLATPAK)
    {
      g_ptr_array_add (argv, g_strdup ("flatpak-spawn"));
      g_ptr_array_add (argv, g_strdup ("--host"));
      g_ptr_array_add (argv, g_strdup ("--watch-bus"));
      g_ptr_array_add (argv, g_strdup_printf ("--forward-fd=3"));
    }

  g_ptr_array_add (argv, g_strdup (prompt_agent_path));
  g_ptr_array_add (argv, g_strdup ("--socket-fd=3"));
  g_ptr_array_add (argv, NULL);

  res = socketpair (AF_UNIX, SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC, 0, pair);

  if (res != 0)
    {
      int errsv = errno;
      g_set_error_literal (error,
                           G_IO_ERROR,
                           g_io_error_from_errno (errsv),
                           g_strerror (errsv));
      return NULL;
    }

  if (!(socket = g_socket_new_from_fd (pair[0], error)))
    {
      close (pair[0]);
      close (pair[1]);
      return NULL;
    }

  g_subprocess_launcher_take_fd (launcher, pair[1], 3);

  pair[0] = -1;
  pair[1] = -1;

  stream = g_socket_connection_factory_create_connection (socket);
  if (!(bus = g_dbus_connection_new_sync (G_IO_STREAM (stream), NULL,
                                          G_DBUS_CONNECTION_FLAGS_DELAY_MESSAGE_PROCESSING,
                                          NULL, NULL, error)))
    return NULL;

  g_set_object (&self->bus, bus);

  if (!(self->proxy = prompt_ipc_agent_proxy_new_sync (bus,
                                                       G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                                       NULL,
                                                       "/org/gnome/Prompt/Agent/",
                                                       NULL,
                                                       error)))
    return NULL;

  g_signal_connect_object (self->proxy,
                           "containers-changed",
                           G_CALLBACK (prompt_client_containers_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_subprocess_launcher_set_child_setup (launcher,
                                         prompt_client_child_setup_func,
                                         NULL, NULL);

  if (!(subprocess = g_subprocess_launcher_spawnv (launcher, (const char * const *)argv->pdata, error)))
    return NULL;

  g_set_object (&self->subprocess, subprocess);

  g_subprocess_wait_check_async (subprocess,
                                 NULL,
                                 prompt_client_wait_cb,
                                 g_object_ref (self));

  g_dbus_connection_start_message_processing (bus);

  return g_steal_pointer (&self);
}

void
prompt_client_force_exit (PromptClient *self)
{
  g_return_if_fail (PROMPT_IS_CLIENT (self));

  g_subprocess_force_exit (self->subprocess);
}
