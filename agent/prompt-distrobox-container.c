/* prompt-distrobox-container.c
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

#include "prompt-distrobox-container.h"

struct _PromptDistroboxContainer
{
  PromptPodmanContainer parent_instance;
};

G_DEFINE_TYPE (PromptDistroboxContainer, prompt_distrobox_container, PROMPT_TYPE_PODMAN_CONTAINER)

static gboolean
prompt_distrobox_container_run_context_cb (PromptRunContext    *run_context,
                                           const char * const  *argv,
                                           const char * const  *env,
                                           const char          *cwd,
                                           PromptUnixFDMap     *unix_fd_map,
                                           gpointer             user_data,
                                           GError             **error)
{
  PromptDistroboxContainer *self = user_data;
  g_autoptr(GString) additional_flags = NULL;
  const char *name;
  int max_dest_fd;

  g_assert (PROMPT_IS_DISTROBOX_CONTAINER (self));
  g_assert (PROMPT_IS_RUN_CONTEXT (run_context));
  g_assert (argv != NULL);
  g_assert (env != NULL);
  g_assert (PROMPT_IS_UNIX_FD_MAP (unix_fd_map));

  name = prompt_ipc_container_get_display_name (PROMPT_IPC_CONTAINER (self));

  prompt_run_context_append_argv (run_context, "distrobox");
  prompt_run_context_append_argv (run_context, "enter");
  prompt_run_context_append_argv (run_context, "--no-tty");
  prompt_run_context_append_argv (run_context, name);

  additional_flags = g_string_new ("--tty ");

  /* From podman-exec(1):
   *
   * Pass down to the process N additional file descriptors (in addition to
   * 0, 1, 2).  The total FDs will be 3+N.
   */
  if ((max_dest_fd = prompt_unix_fd_map_get_max_dest_fd (unix_fd_map)) >= 2)
    g_string_append_printf (additional_flags, "--preserve-fds=%d ", max_dest_fd-2);

  /* Make sure we can pass the FDs down */
  if (!prompt_run_context_merge_unix_fd_map (run_context, unix_fd_map, error))
    return FALSE;

  if (additional_flags->len)
    {
      prompt_run_context_append_argv (run_context, "--additional-flags");
      prompt_run_context_append_argv (run_context, additional_flags->str);
    }

  prompt_run_context_append_argv (run_context, "--");
  prompt_run_context_append_argv (run_context, "env");

  /* TODO: We need to find a way to propagate directory safely.
   *       env --chrdir= is an option if we know it's already there.
   */
  if (cwd != NULL && cwd[0] && g_file_test (cwd, G_FILE_TEST_EXISTS))
    prompt_run_context_set_cwd (run_context, cwd);
  else
    prompt_run_context_append_formatted (run_context, "--chdir=%s", cwd);

  /* Append environment if we have it */
  if (env != NULL)
    prompt_run_context_append_args (run_context, env);

  /* Finally, propagate the upper layer's command arguments */
  prompt_run_context_append_args (run_context, argv);

  return TRUE;
}

static void
prompt_distrobox_container_prepare_run_context (PromptPodmanContainer *container,
                                                PromptRunContext      *run_context)
{
  g_assert (PROMPT_IS_DISTROBOX_CONTAINER (container));
  g_assert (PROMPT_IS_RUN_CONTEXT (run_context));

  /* These seem to be needed for distrobox-enter */
  prompt_run_context_setenv (run_context, "HOME", g_get_home_dir ());
  prompt_run_context_setenv (run_context, "USER", g_get_user_name ());

  prompt_run_context_push (run_context,
                           prompt_distrobox_container_run_context_cb,
                           g_object_ref (container),
                           g_object_unref);

  prompt_run_context_add_minimal_environment (run_context);

  /* But don't allow it to be overridden inside the environment, that
   * should be setup for us by the distrobox.
   */
  prompt_run_context_setenv (run_context, "HOME", NULL);
}

static void
prompt_distrobox_container_class_init (PromptDistroboxContainerClass *klass)
{
  PromptPodmanContainerClass *podman_container_class = PROMPT_PODMAN_CONTAINER_CLASS (klass);

  podman_container_class->prepare_run_context = prompt_distrobox_container_prepare_run_context;
}

static void
prompt_distrobox_container_init (PromptDistroboxContainer *self)
{
  prompt_ipc_container_set_provider (PROMPT_IPC_CONTAINER (self), "distrobox");
}
