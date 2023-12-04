/* main.c
 *
 * Copyright 2023 Christian Hergert
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

#include <glib/gi18n.h>

#include "prompt-application.h"

static void
check_early_opts (int        *argc,
                  char     ***argv,
                  gboolean   *standalone)
{
  g_autoptr(GOptionContext) context = NULL;
  gboolean version = FALSE;
  GOptionEntry entries[] = {
    { "standalone", 's', 0, G_OPTION_ARG_NONE, standalone },
    { "version", 0, 0, G_OPTION_ARG_NONE, &version },
    { NULL }
  };

  /* If we see a -- then take all of the arguments after that and
   * replace it into an escaped string suitable to pass as the value
   * for `-c 'command ...'.
   */
  for (int i = 0; i < *argc; i++)
    {
      if (g_str_equal ((*argv)[i], "--") && (*argv)[i+1] != NULL)
        {
          GString *str = g_string_new (NULL);

          for (int j = i+1; j < *argc; j++)
            {
              g_autofree char *quoted = g_shell_quote ((*argv)[j]);

              g_string_append (str, quoted);
              g_string_append_c (str, ' ');
              (*argv)[j] = NULL;
            }

          *argc = i + 1 + 1;
          (*argv)[i] = g_strdup ("-x");
          (*argv)[i+1] = g_string_free (str, FALSE);

          break;
        }
    }

  context = g_option_context_new (NULL);
  g_option_context_set_ignore_unknown_options (context, TRUE);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_parse (context, argc, argv, NULL);

  if (version)
    {
      g_print ("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
      g_print ("\n");
      g_print ("  GTK: %d.%d.%d (Compiled against %d.%d.%d)\n",
                  gtk_get_major_version (), gtk_get_minor_version (), gtk_get_micro_version (),
                  GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION);
      g_print ("  VTE: %d.%d.%d (Compiled against %d.%d.%d)\n",
                  vte_get_major_version (), vte_get_minor_version (), vte_get_micro_version (),
                  VTE_MAJOR_VERSION, VTE_MINOR_VERSION, VTE_MICRO_VERSION);

      g_print ("\n\
Copyright 2020-2023 Christian Hergert, et al.\n\
This is free software; see the source for copying conditions. There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
");

      exit (EXIT_SUCCESS);
    }
}

int
main (int   argc,
      char *argv[])
{
  g_autoptr(PromptApplication) app = NULL;
  GApplicationFlags flags = G_APPLICATION_HANDLES_COMMAND_LINE;
  gboolean standalone = FALSE;
  int ret;

  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  g_set_prgname ("prompt");
  g_set_application_name (_("Prompt"));

  check_early_opts (&argc, &argv, &standalone);

  if (standalone)
    flags |= G_APPLICATION_NON_UNIQUE;

  app = prompt_application_new (APP_ID, flags);
  ret = g_application_run (G_APPLICATION (app), argc, argv);

  return ret;
}
