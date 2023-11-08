/*
 * capsule-terminal.c
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

#include <adwaita.h>

#include <glib/gi18n.h>

#include "capsule-application.h"
#include "capsule-shortcuts.h"
#include "capsule-tab.h"
#include "capsule-terminal.h"
#include "capsule-window.h"

#define SIZE_DISMISS_TIMEOUT_MSEC 1000
#define BUILDER_PCRE2_UCP 0x00020000u
#define BUILDER_PCRE2_MULTILINE 0x00000400u
#define URL_MATCH_CURSOR_NAME "pointer"

#define DROP_REQUEST_PRIORITY               G_PRIORITY_DEFAULT
#define APPLICATION_VND_PORTAL_FILETRANSFER "application/vnd.portal.filetransfer"
#define APPLICATION_VND_PORTAL_FILES        "application/vnd.portal.files"
#define TEXT_X_MOZ_URL                      "text/x-moz-url"
#define TEXT_URI_LIST                       "text/uri-list"

struct _CapsuleTerminal
{
  VteTerminal         parent_instance;

  CapsuleShortcuts   *shortcuts;
  CapsulePalette     *palette;
  char               *url;

  GtkPopover         *popover;
  GMenu              *terminal_menu;
  GtkWidget          *drop_highlight;
  GtkDropTargetAsync *drop_target;
  GtkRevealer        *size_revealer;
  GtkLabel           *size_label;

  guint               size_dismiss_source;
};

enum {
  PROP_0,
  PROP_PALETTE,
  PROP_SHORTCUTS,
  N_PROPS
};

enum {
  MATCH_CLICKED,
  N_SIGNALS
};

G_DEFINE_FINAL_TYPE (CapsuleTerminal, capsule_terminal, VTE_TYPE_TERMINAL)

static GParamSpec *properties [N_PROPS];
static guint signals[N_SIGNALS];
static VteRegex *builtin_dingus_regex[2];
static const char * const builtin_dingus[] = {
  "(((gopher|news|telnet|nntp|file|http|ftp|https)://)|(www|ftp)[-A-Za-z0-9]*\\.)[-A-Za-z0-9\\.]+(:[0-9]*)?",
  "(((gopher|news|telnet|nntp|file|http|ftp|https)://)|(www|ftp)[-A-Za-z0-9]*\\.)[-A-Za-z0-9\\.]+(:[0-9]*)?/[-A-Za-z0-9_\\$\\.\\+\\!\\*\\(\\),;:@&=\\?/~\\#\\%]*[^]'\\.}>\\) ,\\\"]",
};

static void
capsule_terminal_toast (CapsuleTerminal *self,
                        int              timeout,
                        const char      *title)
{
  GtkWidget *overlay = gtk_widget_get_ancestor (GTK_WIDGET (self), ADW_TYPE_TOAST_OVERLAY);
  AdwToast *toast;

  if (overlay == NULL)
    return;

  toast = g_object_new (ADW_TYPE_TOAST,
                        "title", title,
                        "timeout", timeout,
                        NULL);
  adw_toast_overlay_add_toast (ADW_TOAST_OVERLAY (overlay), toast);
}

static gboolean
capsule_terminal_is_active (CapsuleTerminal *self)
{
  CapsuleTerminal *active_terminal = NULL;
  CapsuleWindow *window;
  CapsuleTab *active_tab;

  g_assert (CAPSULE_IS_TERMINAL (self));

  if ((window = CAPSULE_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (self), CAPSULE_TYPE_WINDOW))) &&
      (active_tab = capsule_window_get_active_tab (window)))
    active_terminal = capsule_tab_get_terminal (active_tab);

  return active_terminal == self;
}

static gboolean
clear_url_actions_cb (gpointer data)
{
  CapsuleTerminal *self = data;

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "clipboard.copy-link", FALSE);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "terminal.open-link", FALSE);

  return G_SOURCE_REMOVE;
}

static void
capsule_terminal_update_clipboard_actions (CapsuleTerminal *self)
{
  GdkClipboard *clipboard;
  gboolean can_paste;
  gboolean has_selection;

  g_assert (CAPSULE_IS_TERMINAL (self));

  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (self));
  can_paste = gdk_content_formats_contain_gtype (gdk_clipboard_get_formats (clipboard), G_TYPE_STRING);
  has_selection = vte_terminal_get_has_selection (VTE_TERMINAL (self));

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "clipboard.copy", has_selection);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "clipboard.paste", can_paste);
}

static void
capsule_terminal_update_url_actions (CapsuleTerminal *self,
                                     double           x,
                                     double           y)
{
  g_autofree char *pattern = NULL;
  int tag = 0;

  g_assert (CAPSULE_IS_TERMINAL (self));

  pattern = vte_terminal_check_match_at (VTE_TERMINAL (self), x, y, &tag);

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "clipboard.copy-link", pattern != NULL);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "terminal.open-link", pattern != NULL);

  g_set_str (&self->url, pattern);
}

static void
capsule_terminal_popover_closed_cb (CapsuleTerminal *self,
                                    GtkPopover      *popover)
{
  g_assert (CAPSULE_IS_TERMINAL (self));
  g_assert (GTK_IS_POPOVER (popover));

  g_idle_add_full (G_PRIORITY_LOW,
                   clear_url_actions_cb,
                   g_object_ref (self),
                   g_object_unref);
}

static gboolean
capsule_terminal_match_clicked (CapsuleTerminal *self,
                                double           x,
                                double           y,
                                int              button,
                                GdkModifierType  state,
                                const char      *match)
{
  gboolean ret = FALSE;

  g_assert (CAPSULE_IS_TERMINAL (self));
  g_assert (match != NULL);

  g_signal_emit (self, signals[MATCH_CLICKED], 0, x, y, button, state, match, &ret);

  return ret;
}

static void
capsule_terminal_popup (CapsuleTerminal *self,
                        double           x,
                        double           y)
{
  g_assert (CAPSULE_IS_TERMINAL (self));

  capsule_terminal_update_clipboard_actions (self);
  capsule_terminal_update_url_actions (self, x, y);

  if (self->popover == NULL)
    {
      self->popover = GTK_POPOVER (gtk_popover_menu_new_from_model (G_MENU_MODEL (self->terminal_menu)));

      gtk_popover_set_has_arrow (self->popover, FALSE);

      if (gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL)
        gtk_widget_set_halign (GTK_WIDGET (self->popover), GTK_ALIGN_END);
      else
        gtk_widget_set_halign (GTK_WIDGET (self->popover), GTK_ALIGN_START);

      gtk_widget_set_parent (GTK_WIDGET (self->popover), GTK_WIDGET (self));

      g_signal_connect_object (self->popover,
                               "closed",
                               G_CALLBACK (capsule_terminal_popover_closed_cb),
                               self,
                               G_CONNECT_SWAPPED);
    }

  gtk_popover_set_pointing_to (self->popover,
                               &(GdkRectangle) { x, y, 1, 1 });

  gtk_popover_popup (self->popover);
}

static void
capsule_terminal_bubble_click_pressed_cb (CapsuleTerminal *self,
                                          int              n_press,
                                          double           x,
                                          double           y,
                                          GtkGestureClick *click)
{
  g_assert (CAPSULE_IS_TERMINAL (self));
  g_assert (GTK_IS_GESTURE_CLICK (click));

  if (n_press == 1)
    {
      GdkModifierType state = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (click));
      int button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (click));

      if (button == 3)
        {
          if (!(state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_ALT_MASK)) ||
              !(state & (GDK_CONTROL_MASK | GDK_ALT_MASK)))
            {
              capsule_terminal_popup (self, x, y);
              gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_CLAIMED);
              return;
            }
        }
    }

  gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_DENIED);
}

static void
capsule_terminal_capture_click_pressed_cb (CapsuleTerminal *self,
                                           int              n_press,
                                           double           x,
                                           double           y,
                                           GtkGestureClick *click)
{
  g_autofree char *hyperlink = NULL;
  g_autofree char *match = NULL;
  GdkModifierType state;
  gboolean handled = FALSE;
  GdkEvent *event;
  int button;
  int tag = 0;

  g_assert (CAPSULE_IS_TERMINAL (self));
  g_assert (GTK_IS_GESTURE_CLICK (click));

  event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (click));
  state = gdk_event_get_modifier_state (event) & gtk_accelerator_get_default_mod_mask ();
  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (click));

  hyperlink = vte_terminal_check_hyperlink_at (VTE_TERMINAL (self), x, y);
  match = vte_terminal_check_match_at (VTE_TERMINAL (self), x, y, &tag);

  if (n_press == 1 &&
      !handled &&
      (button == 1 || button == 2) &&
      (state & GDK_CONTROL_MASK))
    {
      if (hyperlink != NULL)
        handled = capsule_terminal_match_clicked (self, x, y, button, state, hyperlink);
      else if (match != NULL)
        handled = capsule_terminal_match_clicked (self, x, y, button, state, match);
    }

  if (handled)
    gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_CLAIMED);
  else
    gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_DENIED);
}

static void
select_all_action (GtkWidget  *widget,
                   const char *action_name,
                   GVariant   *param)
{
  g_assert (CAPSULE_IS_TERMINAL (widget));
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_BOOLEAN));

  if (g_variant_get_boolean (param))
    vte_terminal_select_all (VTE_TERMINAL (widget));
  else
    vte_terminal_unselect_all (VTE_TERMINAL (widget));
}

static void
copy_clipboard_action (GtkWidget  *widget,
                       const char *action_name,
                       GVariant   *param)
{
  CapsuleTerminal *self = CAPSULE_TERMINAL (widget);
  GdkClipboard *clipboard = gtk_widget_get_clipboard (widget);
  g_autofree char *text = vte_terminal_get_text_selected (VTE_TERMINAL (widget), VTE_FORMAT_TEXT);

  if (text && text[0] != 0)
    {
      gdk_clipboard_set_text (clipboard, text);
      capsule_terminal_toast (self, 1, _("Copied to clipboard"));
    }
}

static void
paste_clipboard_action (GtkWidget  *widget,
                        const char *action_name,
                        GVariant   *param)
{
  g_assert (VTE_IS_TERMINAL (widget));

  vte_terminal_paste_clipboard (VTE_TERMINAL (widget));
}

static void
capsule_terminal_selection_changed (VteTerminal *terminal)
{
  capsule_terminal_update_clipboard_actions (CAPSULE_TERMINAL (terminal));
}

static void
copy_link_address_action (GtkWidget  *widget,
                          const char *action_name,
                          GVariant   *param)
{
  CapsuleTerminal *self = (CapsuleTerminal *)widget;

  g_assert (CAPSULE_IS_TERMINAL (self));

  if (self->url && self->url[0])
    {
      gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (self)), self->url);
      capsule_terminal_toast (self, 1, _("Copied to clipboard"));
    }
}

static void
open_link_action (GtkWidget  *widget,
                  const char *action_name,
                  GVariant   *param)
{
  CapsuleTerminal *self = (CapsuleTerminal *)widget;
  g_autoptr(GtkUriLauncher) launcher = NULL;

  g_assert (CAPSULE_IS_TERMINAL (self));

  if (self->url == NULL || self->url[0] == 0)
    return;

  launcher = gtk_uri_launcher_new (self->url);
  gtk_uri_launcher_launch (launcher,
                           GTK_WINDOW (gtk_widget_get_root (widget)),
                           NULL, NULL, NULL);
}

typedef struct {
  CapsuleTerminal *terminal;
  GdkDrop *drop;
  GList *files;
  const char *mime_type;
} TextUriList;

static void
text_uri_list_free (TextUriList *uri_list)
{
  g_clear_object (&uri_list->terminal);
  g_clear_object (&uri_list->drop);
  g_clear_list (&uri_list->files, g_object_unref);
  uri_list->mime_type = NULL;
  g_free (uri_list);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (TextUriList, text_uri_list_free)

static void
capsule_terminal_drop_file_list (CapsuleTerminal *self,
                                 const GList     *files)
{
  g_autoptr(GString) string = NULL;

  g_assert (CAPSULE_IS_TERMINAL (self));
  g_assert (files == NULL || G_IS_FILE (files->data));

  string = g_string_new (NULL);

  for (const GList *iter = files; iter; iter = iter->next)
    {
      GFile *file = G_FILE (iter->data);

      if (g_file_is_native (file))
        {
          g_autofree char *quoted = g_shell_quote (g_file_peek_path (file));

          g_string_append (string, quoted);
          g_string_append_c (string, ' ');
        }
      else
        {
          g_autofree char *uri = g_file_get_uri (file);
          g_autofree char *quoted = g_shell_quote (uri);

          g_string_append (string, quoted);
          g_string_append_c (string, ' ');
        }
    }

  if (string->len > 0)
    vte_terminal_paste_text (VTE_TERMINAL (self), string->str);
}

static void
capsule_terminal_drop_uri_list_line_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  GDataInputStream *line_reader = G_DATA_INPUT_STREAM (object);
  g_autoptr(TextUriList) state = (TextUriList*)user_data;
  g_autoptr(GError) error = NULL;
  g_autofree char *line = NULL;
  gsize len = 0;

  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (state != NULL);
  g_assert (CAPSULE_IS_TERMINAL (state->terminal));
  g_assert (GDK_IS_DROP (state->drop));

  line = g_data_input_stream_read_line_finish_utf8 (line_reader, result, &len, &error);

  if (error != NULL)
    {
      g_debug ("Failed to receive '%s': %s", state->mime_type, error->message);
      gdk_drop_finish (state->drop, 0);
      return;
    }

  if (line != NULL && line[0] != 0 && line[0] != '#')
    {
      GFile *file = g_file_new_for_uri (line);

      if (file != NULL)
        state->files = g_list_append (state->files, file);
    }

  if (line == NULL || g_strcmp0 (state->mime_type, TEXT_X_MOZ_URL) == 0)
    {
      capsule_terminal_drop_file_list (state->terminal, state->files);
      gdk_drop_finish (state->drop, GDK_ACTION_COPY);
      return;
    }

  g_data_input_stream_read_line_async (line_reader,
                                       DROP_REQUEST_PRIORITY,
                                       NULL,
                                       capsule_terminal_drop_uri_list_line_cb,
                                       g_steal_pointer (&state));
}

static void
capsule_terminal_drop_uri_list_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  GdkDrop *drop = (GdkDrop *)object;
  g_autoptr(CapsuleTerminal) self = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GInputStream) stream = NULL;
  g_autoptr(GDataInputStream) line_reader = NULL;
  g_autoptr(TextUriList) state = NULL;
  const char *mime_type = NULL;

  g_assert (GDK_IS_DROP (drop));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (CAPSULE_IS_TERMINAL (self));

  if (!(stream = gdk_drop_read_finish (drop, result, &mime_type, &error)))
    {
      g_debug ("Failed to receive text/uri-list offer: %s", error->message);
      gdk_drop_finish (drop, 0);
      return;
    }

  g_assert (g_strcmp0 (mime_type, TEXT_URI_LIST) == 0);
  g_assert (G_IS_INPUT_STREAM (stream));

  line_reader = g_data_input_stream_new (stream);
  g_data_input_stream_set_newline_type (line_reader,
                                        G_DATA_STREAM_NEWLINE_TYPE_CR_LF);

  state = g_new0 (TextUriList, 1);
  state->terminal = g_object_ref (self);
  state->drop = g_object_ref (drop);
  state->mime_type = g_intern_string (mime_type);

  g_data_input_stream_read_line_async (line_reader,
                                       DROP_REQUEST_PRIORITY,
                                       NULL,
                                       capsule_terminal_drop_uri_list_line_cb,
                                       g_steal_pointer (&state));
}

static void
capsule_terminal_drop_file_list_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  GdkDrop *drop = (GdkDrop *)object;
  g_autoptr(CapsuleTerminal) self = user_data;
  g_autoptr(GError) error = NULL;
  const GValue *value;
  const GList *file_list;

  g_assert (GDK_IS_DROP (drop));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (CAPSULE_IS_TERMINAL (self));

  if (!(value = gdk_drop_read_value_finish (drop, result, &error)))
    {
      g_debug ("Failed to receive file-list offer: %s", error->message);

      /* If the user dragged a directory from Nautilus or another
       * new-style application, a portal request would be made. But
       * GTK won't be able to open the directory so the request for
       * APPLICATION_VND_PORTAL_FILETRANSFER will fail. Fallback to
       * opening the request via TEXT_URI_LIST gracefully.
       */
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND) ||
          g_error_matches (error, G_IO_ERROR, G_DBUS_ERROR_ACCESS_DENIED))
        gdk_drop_read_async (drop,
                             (const char **)(const char * const[]){TEXT_URI_LIST, NULL},
                             DROP_REQUEST_PRIORITY,
                             NULL,
                             capsule_terminal_drop_uri_list_cb,
                             g_object_ref (self));
      else
        gdk_drop_finish (drop, 0);

      return;
    }

  g_assert (value != NULL);
  g_assert (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST));

  file_list = (const GList *)g_value_get_boxed (value);
  capsule_terminal_drop_file_list (self, file_list);
  gdk_drop_finish (drop, GDK_ACTION_COPY);
}

static void
capsule_terminal_drop_string_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  GdkDrop *drop = (GdkDrop *)object;
  g_autoptr(CapsuleTerminal) self = user_data;
  g_autoptr(GError) error = NULL;
  const GValue *value;
  const char *string;

  g_assert (GDK_IS_DROP (drop));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (CAPSULE_IS_TERMINAL (self));

  if (!(value = gdk_drop_read_value_finish (drop, result, &error)))
    {
      gdk_drop_finish (drop, 0);
      return;
    }

  g_assert (value != NULL);
  g_assert (G_VALUE_HOLDS_STRING (value));

  string = g_value_get_string (value);

  if (string != NULL && string[0] != 0)
    vte_terminal_paste_text (VTE_TERMINAL (self), string);

  gdk_drop_finish (drop, GDK_ACTION_COPY);
}

static void
capsule_terminal_drop_moz_url_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  GdkDrop *drop = (GdkDrop *)object;
  g_autoptr(CapsuleTerminal) self = user_data;
  g_autoptr(GCharsetConverter) converter = NULL;
  g_autoptr(GDataInputStream) line_reader = NULL;
  g_autoptr(GInputStream) converter_stream = NULL;
  g_autoptr(GInputStream) stream = NULL;
  g_autoptr(GError) error = NULL;
  const char *mime_type = NULL;
  TextUriList *state;

  g_assert (GDK_IS_DROP (drop));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (CAPSULE_IS_TERMINAL (self));

  if (!(stream = gdk_drop_read_finish (drop, result, &mime_type, &error))) {
    gdk_drop_finish (drop, 0);
    return;
  }

  g_assert (G_IS_INPUT_STREAM (stream));

  if (!(converter = g_charset_converter_new ("UTF-8", "UCS-2", &error))) {
    g_debug ("Failed to create UTF-8 decoder: %s", error->message);
    gdk_drop_finish (drop, 0);
    return;
  }

  /* TEXT_X_MOZ_URL is in UCS-2 so convert it to UTF-8.
   *
   * The data is expected to be URL, a \n, then the title of the web page.
   *
   * However, some applications (e.g. dolphin) delimit with a \r\n (see
   * issue#293) so handle that generically with the line reader.
   */
  converter_stream = g_converter_input_stream_new (stream, G_CONVERTER (converter));
  line_reader = g_data_input_stream_new (converter_stream);
  g_data_input_stream_set_newline_type (line_reader,
                                        G_DATA_STREAM_NEWLINE_TYPE_ANY);

  state = g_new0 (TextUriList, 1);
  state->terminal = g_object_ref (self);
  state->drop = g_object_ref (drop);
  state->mime_type = g_intern_string (TEXT_X_MOZ_URL);

  g_data_input_stream_read_line_async (line_reader,
                                       DROP_REQUEST_PRIORITY,
                                       NULL,
                                       capsule_terminal_drop_uri_list_line_cb,
                                       g_steal_pointer (&state));
}

static gboolean
capsule_terminal_drop_target_drop (CapsuleTerminal    *self,
                                   GdkDrop            *drop,
                                   double              x,
                                   double              y,
                                   GtkDropTargetAsync *drop_target)
{
  GdkContentFormats *formats;

  g_assert (CAPSULE_IS_TERMINAL (self));
  g_assert (GDK_IS_DROP (drop));
  g_assert (GTK_IS_DROP_TARGET_ASYNC (drop_target));

  formats = gdk_drop_get_formats (drop);

  if (gdk_content_formats_contain_gtype (formats, GDK_TYPE_FILE_LIST) ||
      gdk_content_formats_contain_gtype (formats, G_TYPE_FILE) ||
      gdk_content_formats_contain_mime_type (formats, TEXT_URI_LIST) ||
      gdk_content_formats_contain_mime_type (formats, APPLICATION_VND_PORTAL_FILETRANSFER) ||
      gdk_content_formats_contain_mime_type (formats, APPLICATION_VND_PORTAL_FILES)) {
    gdk_drop_read_value_async (drop,
                               GDK_TYPE_FILE_LIST,
                               DROP_REQUEST_PRIORITY,
                               NULL,
                               capsule_terminal_drop_file_list_cb,
                               g_object_ref (self));
    return TRUE;
  } else if (gdk_content_formats_contain_mime_type (formats, TEXT_X_MOZ_URL)) {
    gdk_drop_read_async (drop,
                         (const char **)(const char * const []){TEXT_X_MOZ_URL, NULL},
                         DROP_REQUEST_PRIORITY,
                         NULL,
                         capsule_terminal_drop_moz_url_cb,
                         g_object_ref (self));
    return TRUE;
  } else if (gdk_content_formats_contain_gtype (formats, G_TYPE_STRING)) {
    gdk_drop_read_value_async (drop,
                               G_TYPE_STRING,
                               DROP_REQUEST_PRIORITY,
                               NULL,
                               capsule_terminal_drop_string_cb,
                               g_object_ref (self));
    return TRUE;
  }

  return FALSE;
}

static GdkDragAction
capsule_terminal_drop_target_drag_enter (CapsuleTerminal    *self,
                                         GdkDrop            *drop,
                                         double              x,
                                         double              y,
                                         GtkDropTargetAsync *drop_target)
{
  g_assert (CAPSULE_IS_TERMINAL (self));
  g_assert (GTK_IS_DROP_TARGET_ASYNC (drop_target));

  gtk_widget_set_visible (self->drop_highlight, TRUE);

  return GDK_ACTION_COPY;
}

static void
capsule_terminal_drop_target_drag_leave (CapsuleTerminal    *self,
                                         GdkDrop            *drop,
                                         GtkDropTargetAsync *drop_target)
{
  g_assert (CAPSULE_IS_TERMINAL (self));
  g_assert (GTK_IS_DROP_TARGET_ASYNC (drop_target));

  gtk_widget_set_visible (self->drop_highlight, FALSE);
}

static void
capsule_terminal_measure (GtkWidget      *widget,
                          GtkOrientation  orientation,
                          int             for_size,
                          int            *minimum,
                          int            *natural,
                          int            *minimum_baseline,
                          int            *natural_baseline)
{
  CapsuleTerminal *self = CAPSULE_TERMINAL (widget);
  int min_revealer;
  int nat_revealer;

  GTK_WIDGET_CLASS (capsule_terminal_parent_class)->measure (widget,
                                                             orientation,
                                                             for_size,
                                                             minimum,
                                                             natural,
                                                             minimum_baseline,
                                                             natural_baseline);

  gtk_widget_measure (GTK_WIDGET (self->size_revealer),
                      orientation, for_size,
                      &min_revealer, &nat_revealer, NULL, NULL);

  *minimum = MAX (*minimum, min_revealer);
  *natural = MAX (*natural, nat_revealer);
}

static gboolean
dismiss_size_label_cb (gpointer user_data)
{
  CapsuleTerminal *self = user_data;

  gtk_revealer_set_reveal_child (self->size_revealer, FALSE);
  self->size_dismiss_source = 0;

  return G_SOURCE_REMOVE;
}

static void
capsule_terminal_size_allocate (GtkWidget *widget,
                                int        width,
                                int        height,
                                int        baseline)
{
  CapsuleTerminal *self = CAPSULE_TERMINAL (widget);
  GtkRequisition min;
  GtkAllocation revealer_alloc, dnd_alloc;
  GtkBorder padding;
  GtkRoot *root;
  int prev_column_count, column_count;
  int prev_row_count, row_count;

  g_assert (CAPSULE_IS_TERMINAL (self));

  prev_column_count = vte_terminal_get_column_count (VTE_TERMINAL (self));
  prev_row_count = vte_terminal_get_row_count (VTE_TERMINAL (self));

  GTK_WIDGET_CLASS (capsule_terminal_parent_class)->size_allocate (widget, width, height, baseline);

  column_count = vte_terminal_get_column_count (VTE_TERMINAL (self));
  row_count = vte_terminal_get_row_count (VTE_TERMINAL (self));

  root = gtk_widget_get_root (widget);

  if (capsule_terminal_is_active (self) &&
      GTK_IS_WINDOW (root) &&
      !gtk_window_is_maximized (GTK_WINDOW (root)) &&
      !gtk_window_is_fullscreen (GTK_WINDOW (root)) &&
      (prev_column_count != column_count || prev_row_count != row_count))
    {
      char format[32];

      g_snprintf (format, sizeof format, "%ld × %ld",
                  vte_terminal_get_column_count (VTE_TERMINAL (self)),
                  vte_terminal_get_row_count (VTE_TERMINAL (self)));
      gtk_label_set_label (self->size_label, format);

      gtk_revealer_set_reveal_child (self->size_revealer, TRUE);

      g_clear_handle_id (&self->size_dismiss_source, g_source_remove);
      self->size_dismiss_source = g_timeout_add (SIZE_DISMISS_TIMEOUT_MSEC,
                                                 dismiss_size_label_cb,
                                                 self);
    }
  else if (gtk_window_is_maximized (GTK_WINDOW (root)) ||
           gtk_window_is_fullscreen (GTK_WINDOW (root)))
    {
      g_clear_handle_id (&self->size_dismiss_source, g_source_remove);
      gtk_revealer_set_reveal_child (self->size_revealer, FALSE);
    }

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  gtk_style_context_get_padding (gtk_widget_get_style_context (widget), &padding);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  gtk_widget_get_preferred_size (GTK_WIDGET (self->size_revealer), &min, NULL);
  revealer_alloc.x = width + padding.right - min.width;
  revealer_alloc.y = height - min.height;
  revealer_alloc.width = min.width;
  revealer_alloc.height = min.height;
  gtk_widget_size_allocate (GTK_WIDGET (self->size_revealer), &revealer_alloc, -1);

  gtk_widget_get_preferred_size (GTK_WIDGET (self->drop_highlight), &min, NULL);
  dnd_alloc.x = -padding.left + 1;
  dnd_alloc.y = 1;
  dnd_alloc.width = padding.left - 1 + width + padding.right - 1;
  dnd_alloc.height = height - 2;
  gtk_widget_size_allocate (GTK_WIDGET (self->drop_highlight), &dnd_alloc, -1);

  if (self->popover)
    gtk_popover_present (self->popover);
}

static void
capsule_terminal_snapshot (GtkWidget   *widget,
                           GtkSnapshot *snapshot)
{
  CapsuleTerminal *self = CAPSULE_TERMINAL (widget);

  GTK_WIDGET_CLASS (capsule_terminal_parent_class)->snapshot (widget, snapshot);

  gtk_widget_snapshot_child (widget, GTK_WIDGET (self->size_revealer), snapshot);
  gtk_widget_snapshot_child (widget, GTK_WIDGET (self->drop_highlight), snapshot);
}

static void
capsule_terminal_shortcuts_notify_cb (CapsuleTerminal  *self,
                                      GParamSpec       *pspec,
                                      CapsuleShortcuts *shortcuts)
{
  g_assert (CAPSULE_IS_TERMINAL (self));
  g_assert (CAPSULE_IS_SHORTCUTS (shortcuts));

  capsule_shortcuts_update_menu (shortcuts, self->terminal_menu);
}

static void
capsule_terminal_dispose (GObject *object)
{
  CapsuleTerminal *self = (CapsuleTerminal *)object;

  g_clear_pointer ((GtkWidget **)&self->popover, gtk_widget_unparent);

  gtk_widget_dispose_template (GTK_WIDGET (self), CAPSULE_TYPE_TERMINAL);

  g_clear_object (&self->palette);
  g_clear_object (&self->shortcuts);
  g_clear_handle_id (&self->size_dismiss_source, g_source_remove);
  g_clear_pointer (&self->url, g_free);

  G_OBJECT_CLASS (capsule_terminal_parent_class)->dispose (object);
}

static void
capsule_terminal_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  CapsuleTerminal *self = CAPSULE_TERMINAL (object);

  switch (prop_id)
    {
    case PROP_PALETTE:
      g_value_set_object (value, capsule_terminal_get_palette (self));
      break;

    case PROP_SHORTCUTS:
      g_value_set_object (value, self->shortcuts);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
capsule_terminal_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  CapsuleTerminal *self = CAPSULE_TERMINAL (object);

  switch (prop_id)
    {
    case PROP_PALETTE:
      capsule_terminal_set_palette (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
capsule_terminal_class_init (CapsuleTerminalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  VteTerminalClass *terminal_class = VTE_TERMINAL_CLASS (klass);

  object_class->dispose = capsule_terminal_dispose;
  object_class->get_property = capsule_terminal_get_property;
  object_class->set_property = capsule_terminal_set_property;

  widget_class->measure = capsule_terminal_measure;
  widget_class->size_allocate = capsule_terminal_size_allocate;
  widget_class->snapshot = capsule_terminal_snapshot;

  terminal_class->selection_changed = capsule_terminal_selection_changed;

  properties[PROP_PALETTE] =
    g_param_spec_object ("palette", NULL, NULL,
                         CAPSULE_TYPE_PALETTE,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_SHORTCUTS] =
    g_param_spec_object ("shortcuts", NULL, NULL,
                         CAPSULE_TYPE_SHORTCUTS,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals[MATCH_CLICKED] =
    g_signal_new ("match-clicked",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  g_signal_accumulator_true_handled, NULL,
                  NULL,
                  G_TYPE_BOOLEAN,
                  5,
                  G_TYPE_DOUBLE,
                  G_TYPE_DOUBLE,
                  G_TYPE_INT,
                  GDK_TYPE_MODIFIER_TYPE,
                  G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Capsule/capsule-terminal.ui");

  gtk_widget_class_bind_template_child (widget_class, CapsuleTerminal, drop_highlight);
  gtk_widget_class_bind_template_child (widget_class, CapsuleTerminal, drop_target);
  gtk_widget_class_bind_template_child (widget_class, CapsuleTerminal, size_label);
  gtk_widget_class_bind_template_child (widget_class, CapsuleTerminal, size_revealer);
  gtk_widget_class_bind_template_child (widget_class, CapsuleTerminal, terminal_menu);

  gtk_widget_class_bind_template_callback (widget_class, capsule_terminal_bubble_click_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, capsule_terminal_capture_click_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, capsule_terminal_drop_target_drag_enter);
  gtk_widget_class_bind_template_callback (widget_class, capsule_terminal_drop_target_drag_leave);
  gtk_widget_class_bind_template_callback (widget_class, capsule_terminal_drop_target_drop);

  gtk_widget_class_install_action (widget_class, "clipboard.copy", NULL, copy_clipboard_action);
  gtk_widget_class_install_action (widget_class, "clipboard.copy-link", NULL, copy_link_address_action);
  gtk_widget_class_install_action (widget_class, "clipboard.paste", NULL, paste_clipboard_action);
  gtk_widget_class_install_action (widget_class, "terminal.open-link", NULL, open_link_action);
  gtk_widget_class_install_action (widget_class, "terminal.select-all", "b", select_all_action);

  for (guint i = 0; i < G_N_ELEMENTS (builtin_dingus); i++)
    {
      g_autoptr(GError) error = NULL;

      builtin_dingus_regex[i] = vte_regex_new_for_match (builtin_dingus[i],
                                                         strlen (builtin_dingus[i]),
                                                         VTE_REGEX_FLAGS_DEFAULT | BUILDER_PCRE2_MULTILINE | BUILDER_PCRE2_UCP,
                                                         NULL);

      if (!vte_regex_jit (builtin_dingus_regex[i], 0, &error))
        g_warning ("Failed to JIT regex: %s: Regex was: %s",
                   error->message,
                   builtin_dingus[i]);
    }
}

static void
capsule_terminal_init (CapsuleTerminal *self)
{
  g_autoptr(GdkContentFormats) formats = NULL;
  GdkContentFormatsBuilder *builder;
  CapsuleApplication *app = CAPSULE_APPLICATION_DEFAULT;
  CapsuleShortcuts *shortcuts = capsule_application_get_shortcuts (app);

  g_set_object (&self->shortcuts, shortcuts);

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (shortcuts,
                           "notify",
                           G_CALLBACK (capsule_terminal_shortcuts_notify_cb),
                           self,
                           G_CONNECT_SWAPPED);
  capsule_terminal_shortcuts_notify_cb (self, NULL, shortcuts);

  for (guint i = 0; i < G_N_ELEMENTS (builtin_dingus_regex); i++)
    {
      int tag = vte_terminal_match_add_regex (VTE_TERMINAL (self),
                                              builtin_dingus_regex[i],
                                              0);
      vte_terminal_match_set_cursor_name (VTE_TERMINAL (self),
                                          tag,
                                          URL_MATCH_CURSOR_NAME);
    }

  builder = gdk_content_formats_builder_new ();
  gdk_content_formats_builder_add_gtype (builder, G_TYPE_STRING);
  gdk_content_formats_builder_add_gtype (builder, GDK_TYPE_FILE_LIST);
  gdk_content_formats_builder_add_mime_type (builder, APPLICATION_VND_PORTAL_FILES);
  gdk_content_formats_builder_add_mime_type (builder, APPLICATION_VND_PORTAL_FILETRANSFER);
  gdk_content_formats_builder_add_mime_type (builder, TEXT_URI_LIST);
  gdk_content_formats_builder_add_mime_type (builder, TEXT_X_MOZ_URL);
  formats = gdk_content_formats_builder_free_to_formats (builder);

  gtk_drop_target_async_set_actions (self->drop_target,
                                     (GDK_ACTION_COPY |
                                      GDK_ACTION_MOVE));
  gtk_drop_target_async_set_formats (self->drop_target, formats);
}

CapsulePalette *
capsule_terminal_get_palette (CapsuleTerminal *self)
{
  g_return_val_if_fail (CAPSULE_IS_TERMINAL (self), NULL);

  return self->palette;
}

void
capsule_terminal_set_palette (CapsuleTerminal *self,
                              CapsulePalette  *palette)
{
  g_return_if_fail (CAPSULE_IS_TERMINAL (self));

  if (g_set_object (&self->palette, palette))
    {
      AdwStyleManager *style_manager = adw_style_manager_get_default ();
      g_autoptr(CapsulePalette) fallback = NULL;
      const GdkRGBA *background;
      const GdkRGBA *foreground;
      const GdkRGBA *colors;
      gboolean dark;
      guint n_colors;

      if (palette == NULL)
        palette = fallback = capsule_palette_new_from_name ("gnome");

      dark = adw_style_manager_get_dark (style_manager);
      colors = capsule_palette_get_indexed_colors (palette, &n_colors);
      background = capsule_palette_get_background (palette, dark);
      foreground = capsule_palette_get_foreground (palette, dark);

      g_assert (foreground != NULL);
      g_assert (background != NULL);
      g_assert (colors != NULL);
      g_assert (n_colors == 16);

      vte_terminal_set_colors (VTE_TERMINAL (self),
                               foreground,
                               background,
                               colors,
                               n_colors);

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PALETTE]);
    }
}
