/*
 * prompt-session.c
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

#include "prompt-application.h"
#include "prompt-session.h"
#include "prompt-settings.h"
#include "prompt-util.h"
#include "prompt-window.h"

GVariant *
prompt_session_save (PromptApplication *app)
{
  PromptSettings *settings;
  GVariantBuilder builder;
  gboolean restore_session;

  g_return_val_if_fail (PROMPT_IS_APPLICATION (app), NULL);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));
  g_variant_builder_add_parsed (&builder, "{'version', <%u>}", 1);
  g_variant_builder_open (&builder, G_VARIANT_TYPE ("{sv}"));
  g_variant_builder_add (&builder, "s", "windows");
  g_variant_builder_open (&builder, G_VARIANT_TYPE ("v"));
  g_variant_builder_open (&builder, G_VARIANT_TYPE ("aa{sv}"));

  settings = prompt_application_get_settings (PROMPT_APPLICATION_DEFAULT);
  restore_session = prompt_settings_get_restore_session (settings);

  for (const GList *list = gtk_application_get_windows (GTK_APPLICATION (app));
       list != NULL;
       list = list->next)
    {
      if (PROMPT_IS_WINDOW (list->data))
        {
          PromptWindow *window = PROMPT_WINDOW (list->data);
          g_autoptr(GListModel) pages = prompt_window_list_pages (window);
          guint n_items = g_list_model_get_n_items (pages);

          g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{sv}"));

          if (gtk_window_is_maximized (GTK_WINDOW (window)))
            g_variant_builder_add_parsed (&builder, "{'maximized', <%b>}", TRUE);

          g_variant_builder_open (&builder, G_VARIANT_TYPE ("{sv}"));
          g_variant_builder_add (&builder, "s", "tabs");
          g_variant_builder_open (&builder, G_VARIANT_TYPE ("v"));
          g_variant_builder_open (&builder, G_VARIANT_TYPE ("aa{sv}"));

          for (guint i = 0; i < n_items; i++)
            {
              g_autoptr(AdwTabPage) page = g_list_model_get_item (pages, i);
              gboolean pinned;

              g_assert (ADW_IS_TAB_PAGE (page));

              pinned = adw_tab_page_get_pinned (page);

              if (restore_session || pinned)
                {
                  PromptTab *tab = PROMPT_TAB (adw_tab_page_get_child (page));
                  g_autoptr(PromptIpcContainer) container = NULL;
                  g_autofree char *default_container = NULL;
                  PromptTerminal *terminal;
                  PromptProfile *profile;
                  const char *container_id = NULL;
                  const char *window_title;
                  const char *cwd;
                  const char *uuid;
                  gboolean is_active;
                  guint rows;
                  guint columns;

                  g_assert (PROMPT_IS_TAB (tab));

                  profile = prompt_tab_get_profile (tab);
                  uuid = prompt_profile_get_uuid (profile);
                  default_container = prompt_profile_dup_default_container (profile);
                  container = prompt_tab_dup_container (tab);
                  is_active = prompt_window_get_active_tab (window) == tab;

                  terminal = prompt_tab_get_terminal (tab);
                  columns = vte_terminal_get_column_count (VTE_TERMINAL (terminal));
                  rows = vte_terminal_get_row_count (VTE_TERMINAL (terminal));
                  cwd = vte_terminal_get_current_directory_uri (VTE_TERMINAL (terminal));
                  window_title = vte_terminal_get_window_title (VTE_TERMINAL (terminal));

                  if (container != NULL)
                    container_id = prompt_ipc_container_get_id (container);

                  g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{sv}"));
                  g_variant_builder_add_parsed (&builder, "{'profile', <%s>}", uuid);
                  g_variant_builder_add_parsed (&builder, "{'pinned', <%b>}", pinned);
                  g_variant_builder_add_parsed (&builder, "{'size', <(%u,%u)>}", columns, rows);
                  g_variant_builder_add_parsed (&builder, "{'active', <%b>}", is_active);
                  if (!prompt_str_empty0 (window_title))
                    g_variant_builder_add_parsed (&builder, "{'window-title', <%s>}", window_title);
                  if (!prompt_str_empty0 (cwd))
                    g_variant_builder_add_parsed (&builder, "{'cwd', <%s>}", cwd);
                  if (container_id != NULL &&
                      g_strcmp0 (default_container, container_id) != 0)
                    g_variant_builder_add_parsed (&builder, "{'container', <%s>}", container_id);
                  g_variant_builder_close (&builder);
                }
            }

          g_variant_builder_close (&builder);
          g_variant_builder_close (&builder);
          g_variant_builder_close (&builder);
          g_variant_builder_close (&builder);
        }
    }

  g_variant_builder_close (&builder);
  g_variant_builder_close (&builder);
  g_variant_builder_close (&builder);

  return g_variant_take_ref (g_variant_builder_end (&builder));
}

gboolean
prompt_session_restore (PromptApplication *app,
                        GVariant          *state)
{
  g_autoptr(GVariant) windows = NULL;
  PromptSettings *settings;
  GVariantIter iter;
  GVariant *window;
  gboolean restore_session;
  gboolean added_window = FALSE;
  guint32 version;

  g_return_val_if_fail (PROMPT_IS_APPLICATION (app), FALSE);
  g_return_val_if_fail (state != NULL, FALSE);
  g_return_val_if_fail (g_variant_is_of_type (state, G_VARIANT_TYPE ("a{sv}")), FALSE);

  if (!g_variant_lookup (state, "version", "u", &version))
    return FALSE;

  if (!(windows = g_variant_lookup_value (state, "windows", G_VARIANT_TYPE ("aa{sv}"))))
    return FALSE;

  settings = prompt_application_get_settings (PROMPT_APPLICATION_DEFAULT);
  restore_session = prompt_settings_get_restore_session (settings);

  g_variant_iter_init (&iter, windows);
  while (g_variant_iter_loop (&iter, "@a{sv}", &window))
    {
      g_autoptr(GVariant) tabs = NULL;
      g_autoptr(GVariant) tab = NULL;
      PromptWindow *the_window = NULL;
      PromptTab *active_tab = NULL;
      GVariantIter tab_iter;
      gboolean maximized;

      if (!(tabs = g_variant_lookup_value (window, "tabs", G_VARIANT_TYPE ("aa{sv}"))) ||
          g_variant_n_children (tabs) == 0)
        continue;

      if (!g_variant_lookup (window, "maximized", "b", &maximized))
        maximized = FALSE;

      g_variant_iter_init (&tab_iter, tabs);
      while (g_variant_iter_loop (&tab_iter, "@a{sv}", &tab))
        {
          g_autoptr(PromptIpcContainer) the_container = NULL;
          g_autoptr(PromptProfile) the_profile = NULL;
          PromptTerminal *terminal;
          const char *profile;
          const char *container;
          const char *cwd;
          const char *window_title;
          PromptTab *the_tab;
          gboolean is_active;
          gboolean pinned;
          guint32 columns;
          guint32 rows;

          if (!g_variant_lookup (tab, "profile", "&s", &profile))
            profile = NULL;

          if (!g_variant_lookup (tab, "container", "&s", &container))
            container = NULL;

          if (!g_variant_lookup (tab, "pinned", "b", &pinned))
            pinned = FALSE;

          if (!pinned && !restore_session)
            continue;

          if (!g_variant_lookup (tab, "size", "(uu)", &columns, &rows))
            columns = 80, rows = 24;

          if (!g_variant_lookup (tab, "cwd", "&s", &cwd))
            cwd = NULL;

          if (!g_variant_lookup (tab, "window-title", "&s", &window_title))
            window_title = NULL;

          if (!g_variant_lookup (tab, "active", "b", &is_active))
            is_active = FALSE;

          if (!prompt_str_empty0 (container))
            the_container = prompt_application_lookup_container (app, container);

          if (!prompt_str_empty0 (profile))
            the_profile = prompt_application_dup_profile (app, profile);

          if (the_profile == NULL)
            the_profile = prompt_application_dup_default_profile (app);

          if (the_window == NULL)
            the_window = prompt_window_new_empty ();

          the_tab = prompt_tab_new (the_profile);

          if (the_container != NULL)
            prompt_tab_set_container (the_tab, the_container);

          if (cwd != NULL)
            prompt_tab_set_previous_working_directory_uri (the_tab, cwd);

          if (window_title != NULL)
            prompt_tab_set_initial_title (the_tab, window_title);

          terminal = prompt_tab_get_terminal (the_tab);
          vte_terminal_set_size (VTE_TERMINAL (terminal), columns, rows);

          prompt_window_add_tab (the_window, the_tab);
          prompt_window_set_tab_pinned (the_window, the_tab, pinned);

          if (is_active)
            active_tab = the_tab;
        }

      if (the_window != NULL)
        {
          if (!restore_session)
            {
              g_autoptr(PromptProfile) the_profile = NULL;
              PromptTab *the_tab;

              /* Also add a default profile tab which will be our focused
               * tab for the new window since we're not restoring the
               * full tab session for the user.
               */
              the_profile = prompt_application_dup_default_profile (app);
              the_tab = prompt_tab_new (the_profile);
              prompt_window_add_tab (the_window, the_tab);

              if (!active_tab)
                active_tab = the_tab;
            }

          if (active_tab)
            {
              prompt_window_set_active_tab (the_window, active_tab);
              gtk_widget_grab_focus (GTK_WIDGET (active_tab));
            }

          if (maximized)
            gtk_window_maximize (GTK_WINDOW (the_window));

          gtk_window_present (GTK_WINDOW (the_window));
        }

      added_window |= the_window != NULL;
    }

  return added_window;
}
