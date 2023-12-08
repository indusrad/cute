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

#include "prompt-session.h"
#include "prompt-window.h"

GVariant *
prompt_session_save (PromptApplication *app)
{
  GVariantBuilder builder;

  g_return_val_if_fail (PROMPT_IS_APPLICATION (app), NULL);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));
  g_variant_builder_add_parsed (&builder, "{'version', <%u>}", 1);
  g_variant_builder_open (&builder, G_VARIANT_TYPE ("{sv}"));
  g_variant_builder_add (&builder, "s", "windows");
  g_variant_builder_open (&builder, G_VARIANT_TYPE ("v"));
  g_variant_builder_open (&builder, G_VARIANT_TYPE ("aa{sv}"));

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
          g_variant_builder_open (&builder, G_VARIANT_TYPE ("{sv}"));
          g_variant_builder_add (&builder, "s", "tabs");
          g_variant_builder_open (&builder, G_VARIANT_TYPE ("v"));
          g_variant_builder_open (&builder, G_VARIANT_TYPE ("aa{sv}"));

          for (guint i = 0; i < n_items; i++)
            {
              g_autoptr(AdwTabPage) page = g_list_model_get_item (pages, i);

              g_assert (ADW_IS_TAB_PAGE (page));

              if (adw_tab_page_get_pinned (page))
                {
                  PromptTab *tab = PROMPT_TAB (adw_tab_page_get_child (page));
                  g_autoptr(PromptIpcContainer) container = NULL;
                  g_autofree char *default_container = NULL;
                  PromptProfile *profile;
                  const char *container_id = NULL;
                  const char *uuid;

                  g_assert (PROMPT_IS_TAB (tab));

                  profile = prompt_tab_get_profile (tab);
                  uuid = prompt_profile_get_uuid (profile);
                  default_container = prompt_profile_dup_default_container (profile);
                  container = prompt_tab_dup_container (tab);

                  if (container != NULL)
                    container_id = prompt_ipc_container_get_id (container);

                  g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{sv}"));
                  g_variant_builder_add_parsed (&builder, "{'profile', <%s>}", uuid);
                  g_variant_builder_add_parsed (&builder, "{'pinned', <%b>}", TRUE);
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

void
prompt_session_restore (PromptApplication *app,
                        GVariant          *state)
{
  g_autoptr(GVariant) windows = NULL;
  GVariantIter iter;
  GVariant *window;
  gint32 version;

  g_return_if_fail (PROMPT_IS_APPLICATION (app));
  g_return_if_fail (state != NULL);
  g_return_if_fail (g_variant_is_of_type (state, G_VARIANT_TYPE ("a{sv}")));

  if (!g_variant_lookup (state, "version", "i", &version))
    return;

  if (!(windows = g_variant_lookup_value (state, "windows", G_VARIANT_TYPE ("aa{sv}"))))
    return;

  g_variant_iter_init (&iter, windows);
  while (g_variant_iter_loop (&iter, "@a{sv}", &window))
    {
      g_autoptr(GVariant) tabs = NULL;
      g_autoptr(GVariant) tab = NULL;
      PromptWindow *the_window = NULL;
      GVariantIter tab_iter;

      if (!(tabs = g_variant_lookup_value (window, "tabs", G_VARIANT_TYPE ("aa{sv}"))) ||
          g_variant_n_children (tabs) == 0)
        continue;

      /* TODO: Restore tabs into the_window */
      g_variant_iter_init (&tab_iter, tabs);
      while (g_variant_iter_loop (&tab_iter, "@a{sv}", &tab))
        {
          const char *profile = NULL;
          const char *container = NULL;

          g_variant_lookup (tab, "profile", "&s", &profile);
          g_variant_lookup (tab, "container", "&s", &container);
        }

      if (the_window != NULL)
        gtk_window_present (GTK_WINDOW (the_window));
    }
}
