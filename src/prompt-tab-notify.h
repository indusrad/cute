/*
 * prompt-tab-notify.h
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

#pragma once

#include <gtk/gtk.h>

#ifdef GDK_WINDOWING_X11
# include <gdk/x11/gdkx.h>
#endif

#include <glib/gi18n.h>

#include "prompt-window.h"

G_BEGIN_DECLS

typedef struct _PromptTabNotify
{
  PromptTab *tab;

  char *current_cmdline;

  guint contents_changed_source;
  guint shell_preexec_source;

  gulong shell_precmd_handler;
  gulong shell_preexec_handler;

  guint between_preexec_and_precmd : 1;
} PromptTabNotify;

static inline void
prompt_tab_notify_show_notification (PromptTabNotify *notify,
                                     const char      *cmdline)
{
  GtkRoot *window;

  g_assert (notify != NULL);
  g_assert (PROMPT_IS_TAB (notify->tab));

  window = gtk_widget_get_root (GTK_WIDGET (notify->tab));

  if (!PROMPT_IS_WINDOW (window))
    return;

  if (gtk_window_is_active (GTK_WINDOW (window)))
    {
      if (prompt_window_get_active_tab (PROMPT_WINDOW (window)) == notify->tab)
        return;
    }
  else
    {
      g_autoptr(GNotification) notification = NULL;
      g_autoptr(GIcon) icon = NULL;
      g_autofree char *cmdline_sanitized = NULL;
      const char *uuid = prompt_tab_get_uuid (notify->tab);

#ifdef GDK_WINDOWING_X11
      {
        GdkSurface *surface = gtk_native_get_surface (GTK_NATIVE (window));

        if (GDK_IS_X11_SURFACE (surface))
          gdk_x11_surface_set_urgency_hint (surface, TRUE);
      }
#endif

      icon = g_themed_icon_new (APP_ID "-symbolic");
      cmdline_sanitized = g_utf8_make_valid (cmdline, -1);

      notification = g_notification_new (_("Command completed"));
      g_notification_set_body (notification, cmdline_sanitized);
      g_notification_set_icon (notification, icon);
      g_notification_set_default_action_and_target (notification,
                                                    "app.focus-tab-by-uuid",
                                                    "s", uuid);
      g_application_send_notification (G_APPLICATION (PROMPT_APPLICATION_DEFAULT),
                                       uuid, notification);
    }

  prompt_tab_set_needs_attention (notify->tab, TRUE);
}

static inline void
prompt_tab_notify_shell_precmd_cb (PromptTerminal  *terminal,
                                   PromptTabNotify *notify)
{
  g_assert (PROMPT_IS_TERMINAL (terminal));
  g_assert (PROMPT_IS_TAB (notify->tab));

  notify->between_preexec_and_precmd = FALSE;

  g_clear_handle_id (&notify->contents_changed_source, g_source_remove);
  g_clear_handle_id (&notify->shell_preexec_source, g_source_remove);

  if (notify->current_cmdline != NULL)
    {
      prompt_tab_notify_show_notification (notify, notify->current_cmdline);
      g_clear_pointer (&notify->current_cmdline, g_free);
    }
}

static void
prompt_tab_notify_shell_preexec_poll_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  PromptTab *tab = (PromptTab *)object;
  PromptTabNotify *notify = user_data;

  g_assert (PROMPT_IS_TAB (tab));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (notify != NULL);

  prompt_tab_poll_agent_finish (tab, result, NULL);

  g_set_str (&notify->current_cmdline,
             prompt_tab_get_command_line (tab));
}

static inline void
prompt_tab_notify_shell_preexec_cb (PromptTerminal  *terminal,
                                    PromptTabNotify *notify)
{
  g_autofree char *cmdline = NULL;

  g_assert (PROMPT_IS_TERMINAL (terminal));
  g_assert (PROMPT_IS_TAB (notify->tab));

  notify->between_preexec_and_precmd = TRUE;

  g_set_str (&notify->current_cmdline, NULL);

  /* We will poll the agent for the updated command line. If we get
   * precmd back in before this completes, we will ignore showing
   * any notification as it's so quick it shouldn't matter. We
   * can pass a borrowed notify as it's owned by @tab.
   */
  prompt_tab_poll_agent_async (notify->tab,
                               NULL,
                               prompt_tab_notify_shell_preexec_poll_cb,
                               notify);
}

static inline void
prompt_tab_notify_init (PromptTabNotify *notify,
                        PromptTab       *tab)
{
  PromptTerminal *terminal = prompt_tab_get_terminal (tab);

  notify->tab = tab;

  notify->contents_changed_source = 0;
  notify->shell_preexec_source = 0;
  notify->between_preexec_and_precmd = FALSE;
  notify->current_cmdline = NULL;

  notify->shell_precmd_handler =
    g_signal_connect (terminal,
                      "shell-precmd",
                      G_CALLBACK (prompt_tab_notify_shell_precmd_cb),
                      notify);

  notify->shell_preexec_handler =
    g_signal_connect (terminal,
                      "shell-preexec",
                      G_CALLBACK (prompt_tab_notify_shell_preexec_cb),
                      notify);
}

static inline void
prompt_tab_notify_destroy (PromptTabNotify *notify)
{
  PromptTerminal *terminal = prompt_tab_get_terminal (notify->tab);

  if (notify->tab == NULL)
    return;

  g_clear_handle_id (&notify->contents_changed_source, g_source_remove);
  g_clear_handle_id (&notify->shell_preexec_source, g_source_remove);

  g_clear_signal_handler (&notify->shell_precmd_handler, terminal);
  g_clear_signal_handler (&notify->shell_preexec_handler, terminal);

  g_clear_pointer (&notify->current_cmdline, g_free);

  notify->tab = NULL;
}

G_END_DECLS
