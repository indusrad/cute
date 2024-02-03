/* prompt-close-dialog.c
 *
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "prompt-close-dialog.h"

#include "config.h"

#include <glib/gi18n.h>

#include "prompt-tab.h"
#include "prompt-close-dialog.h"
#include "prompt-util.h"
#include "prompt-window.h"

typedef struct
{
  PromptTab       *tab;
  AdwMessageDialog *dialog;
} SaveRequest;

static void
save_request_clear (gpointer data)
{
  SaveRequest *sr = data;

  g_clear_object (&sr->tab);
  g_clear_object (&sr->dialog);
}

static void
prompt_close_dialog_confirm (GtkMessageDialog *dialog,
                             GArray           *requests,
                             gboolean          confirm)
{
  g_assert (ADW_IS_MESSAGE_DIALOG (dialog));
  g_assert (requests != NULL);
  g_assert (requests->len > 0);

  gtk_widget_set_sensitive (GTK_WIDGET (dialog), FALSE);

  for (guint i = 0; i < requests->len; i++)
    {
      const SaveRequest *sr = &g_array_index (requests, SaveRequest , i);

      prompt_tab_force_quit (sr->tab);
    }
}

static void
prompt_close_dialog_response (GtkMessageDialog *dialog,
                              const char       *response,
                              GArray           *requests)
{
  GTask *task;
  g_assert (ADW_IS_MESSAGE_DIALOG (dialog));

  task = g_object_get_data (G_OBJECT (dialog), "G_TASK");

  if (!g_strcmp0 (response, "discard"))
    {
      prompt_close_dialog_confirm (dialog, requests, TRUE);
      g_task_return_boolean (task, TRUE);
    }
  else
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_CANCELLED,
                               "The user cancelled the request");
    }
}

static GtkWidget *
_prompt_close_dialog_new (GtkWindow *parent,
                          GPtrArray *tabs)
{
  g_autoptr(GArray) requests = NULL;
  const char *discard_label;
  PangoAttrList *smaller;
  GtkWidget *dialog;
  GtkWidget *group;
  GtkWidget *prefs_page;

  g_return_val_if_fail (!parent || GTK_IS_WINDOW (parent), NULL);
  g_return_val_if_fail (tabs != NULL, NULL);
  g_return_val_if_fail (tabs->len > 0, NULL);
  g_return_val_if_fail (PROMPT_IS_TAB (g_ptr_array_index (tabs, 0)), NULL);

  if (tabs->len == 1)
    prompt_tab_raise (g_ptr_array_index (tabs, 0));

  requests = g_array_new (FALSE, FALSE, sizeof (SaveRequest));
  g_array_set_clear_func (requests, save_request_clear);

  discard_label = g_dngettext (GETTEXT_PACKAGE, _("_Close"), _("_Close All"), tabs->len);

  dialog = adw_message_dialog_new (parent,
                                   _("Close Window?"),
                                   _("Some processes are still running."));
  gtk_widget_set_size_request (GTK_WIDGET (dialog), 400, -1);

  adw_message_dialog_add_responses (ADW_MESSAGE_DIALOG (dialog),
                                    "cancel", _("_Cancel"),
                                    "discard", discard_label,
                                    NULL);
  adw_message_dialog_set_response_appearance (ADW_MESSAGE_DIALOG (dialog),
                                              "discard", ADW_RESPONSE_DESTRUCTIVE);

  prefs_page = adw_preferences_page_new ();
  adw_message_dialog_set_extra_child (ADW_MESSAGE_DIALOG (dialog), prefs_page);

  group = adw_preferences_group_new ();
  adw_preferences_page_add (ADW_PREFERENCES_PAGE (prefs_page),
                            ADW_PREFERENCES_GROUP (group));

  smaller = pango_attr_list_new ();
  pango_attr_list_insert (smaller, pango_attr_scale_new (0.8333));

  for (guint i = 0; i < tabs->len; i++)
    {
      PromptTab *tab = g_ptr_array_index (tabs, i);
      g_autofree char *cmdline = NULL;
      g_autofree char *title = NULL;
      g_autofree char *subtitle = NULL;
      GtkWidget *row;
      SaveRequest sr;
      GPid pid;

      if (prompt_tab_has_foreground_process (tab, &pid, &cmdline))
        {
          g_utf8_make_valid (cmdline, -1);
          title = g_steal_pointer (&cmdline);
          subtitle = g_strdup_printf (_("Process %d"), pid);
        }
      else
        {
          title = prompt_tab_dup_title (tab);
          subtitle = prompt_tab_dup_subtitle (tab);
        }

      if (g_utf8_strlen (title, -1) > 200)
        g_utf8_offset_to_pointer (title, 200)[0] = 0;

      row = adw_action_row_new ();

      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), title);
      adw_action_row_set_subtitle (ADW_ACTION_ROW (row), subtitle);
      adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

      sr.tab = g_object_ref (tab);
      sr.dialog = g_object_ref (ADW_MESSAGE_DIALOG (dialog));

      g_array_append_val (requests, sr);
    }

  pango_attr_list_unref (smaller);

  g_signal_connect_data (dialog,
                         "response",
                         G_CALLBACK (prompt_close_dialog_response),
                         g_steal_pointer (&requests),
                         (GClosureNotify) g_array_unref,
                         0);

  return dialog;
}

void
_prompt_close_dialog_run_async (GtkWindow           *parent,
                                GPtrArray           *tabs,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  GtkWidget *dialog;

  g_return_if_fail (!parent || GTK_IS_WINDOW (parent));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  dialog = _prompt_close_dialog_new (parent, tabs);
  task = g_task_new (dialog, cancellable, callback, user_data);
  g_task_set_source_tag (task, _prompt_close_dialog_run_async);

  if (tabs == NULL || tabs->len == 0)
    {
      g_task_return_boolean (task, TRUE);
      return;
    }

  g_object_set_data_full (G_OBJECT (dialog),
                          "G_TASK",
                          g_steal_pointer (&task),
                          g_object_unref);
  gtk_window_present (GTK_WINDOW (dialog));
}

gboolean
_prompt_close_dialog_run_finish (GAsyncResult  *result,
                                 GError       **error)
{
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
