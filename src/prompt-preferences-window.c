/*
 * prompt-preferences-window.c
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

#include <math.h>

#include <glib/gi18n.h>

#include "prompt-application.h"
#include "prompt-palette-preview.h"
#include "prompt-preferences-list-item.h"
#include "prompt-preferences-window.h"
#include "prompt-profile-editor.h"
#include "prompt-profile-row.h"
#include "prompt-shortcut-row.h"
#include "prompt-util.h"

struct _PromptPreferencesWindow
{
  AdwPreferencesWindow  parent_instance;

  char                 *default_palette_id;
  GtkCustomFilter      *filter;
  GtkFilterListModel   *filter_palettes;

  GtkListBoxRow        *add_profile_row;
  AdwSwitchRow         *audible_bell;
  AdwComboRow          *backspace_binding;
  AdwSwitchRow         *bold_is_bright;
  AdwComboRow          *cjk_ambiguous_width;
  GListModel           *cjk_ambiguous_widths;
  AdwComboRow          *cursor_blink_mode;
  GListModel           *cursor_blink_modes;
  AdwComboRow          *cursor_shape;
  GListModel           *cursor_shapes;
  AdwComboRow          *delete_binding;
  GListModel           *erase_bindings;
  AdwComboRow          *exit_action;
  GListModel           *exit_actions;
  GtkLabel             *font_name;
  AdwActionRow         *font_name_row;
  AdwSwitchRow         *limit_scrollback;
  GtkAdjustment        *opacity_adjustment;
  GtkLabel             *opacity_label;
  GtkFlowBox           *palette_previews;
  AdwComboRow          *preserve_directory;
  GListModel           *preserve_directories;
  GtkListBox           *profiles_list_box;
  AdwSwitchRow         *restore_session;
  AdwSwitchRow         *restore_window_size;
  AdwSpinRow           *scrollback_lines;
  AdwSwitchRow         *scroll_on_output;
  AdwSwitchRow         *scroll_on_keystroke;
  AdwComboRow          *scrollbar_policy;
  GListModel           *scrollbar_policies;
  PromptShortcutRow    *shortcut_close_other_tabs;
  PromptShortcutRow    *shortcut_close_tab;
  PromptShortcutRow    *shortcut_copy_clipboard;
  PromptShortcutRow    *shortcut_detach_tab;
  PromptShortcutRow    *shortcut_focus_tab_10;
  PromptShortcutRow    *shortcut_focus_tab_1;
  PromptShortcutRow    *shortcut_focus_tab_2;
  PromptShortcutRow    *shortcut_focus_tab_3;
  PromptShortcutRow    *shortcut_focus_tab_4;
  PromptShortcutRow    *shortcut_focus_tab_5;
  PromptShortcutRow    *shortcut_focus_tab_6;
  PromptShortcutRow    *shortcut_focus_tab_7;
  PromptShortcutRow    *shortcut_focus_tab_8;
  PromptShortcutRow    *shortcut_focus_tab_9;
  PromptShortcutRow    *shortcut_move_next_tab;
  PromptShortcutRow    *shortcut_move_previous_tab;
  PromptShortcutRow    *shortcut_move_tab_left;
  PromptShortcutRow    *shortcut_move_tab_right;
  PromptShortcutRow    *shortcut_new_tab;
  PromptShortcutRow    *shortcut_new_window;
  PromptShortcutRow    *shortcut_paste_clipboard;
  PromptShortcutRow    *shortcut_popup_menu;
  PromptShortcutRow    *shortcut_preferences;
  PromptShortcutRow    *shortcut_reset;
  PromptShortcutRow    *shortcut_reset_and_clear;
  PromptShortcutRow    *shortcut_search;
  PromptShortcutRow    *shortcut_select_all;
  PromptShortcutRow    *shortcut_select_none;
  PromptShortcutRow    *shortcut_tab_overview;
  PromptShortcutRow    *shortcut_toggle_fullscreen;
  PromptShortcutRow    *shortcut_undo_close_tab;
  PromptShortcutRow    *shortcut_zoom_in;
  PromptShortcutRow    *shortcut_zoom_one;
  PromptShortcutRow    *shortcut_zoom_out;
  GtkLinkButton        *show_more;
  AdwComboRow          *tab_position;
  GListModel           *tab_positions;
  AdwComboRow          *text_blink_mode;
  GListModel           *text_blink_modes;
  AdwSwitchRow         *use_system_font;
  AdwSwitchRow         *visual_bell;
};

G_DEFINE_FINAL_TYPE (PromptPreferencesWindow, prompt_preferences_window, ADW_TYPE_PREFERENCES_WINDOW)

enum {
  PROP_0,
  PROP_DEFAULT_PALETTE_ID,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static gboolean
filter_primary (gpointer item,
                gpointer user_data)
{
  PromptPreferencesWindow *self = user_data;

  if (prompt_palette_is_primary (item))
    return TRUE;

  return g_strcmp0 (self->default_palette_id, prompt_palette_get_id (item)) == 0;
}

static gboolean
monospace_filter (gpointer item,
                  gpointer user_data)
{
  PangoFontFamily *family = NULL;

  if (PANGO_IS_FONT_FAMILY (item))
    family = PANGO_FONT_FAMILY (item);
  else if (PANGO_IS_FONT_FACE (item))
    family = pango_font_face_get_family (PANGO_FONT_FACE (item));

  return family ? pango_font_family_is_monospace (family) : FALSE;
}

static void
prompt_preferences_window_select_custom_font_cb (GObject      *object,
                                                 GAsyncResult *result,
                                                 gpointer      user_data)
{
  GtkFontDialog *dialog = GTK_FONT_DIALOG (object);
  g_autoptr(PangoFontDescription) font_desc = NULL;
  g_autoptr(PromptSettings) settings = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GTK_IS_FONT_DIALOG (dialog));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (PROMPT_IS_SETTINGS (settings));

  if ((font_desc = gtk_font_dialog_choose_font_finish (dialog, result, &error)))
    {
      g_autofree char *font_name = pango_font_description_to_string (font_desc);

      if (!prompt_str_empty0 (font_name))
        prompt_settings_set_font_name (settings, font_name);
    }
}

static void
prompt_preferences_window_select_custom_font (GtkWidget  *widget,
                                              const char *action_name,
                                              GVariant   *param)
{
  g_autoptr(PangoFontDescription) font_desc = NULL;
  g_autoptr(GtkCustomFilter) filter = NULL;
  g_autoptr(GtkFontDialog) dialog = NULL;
  g_autofree char *font_name = NULL;
  PromptApplication *app;
  PromptSettings *settings;

  g_assert (PROMPT_IS_PREFERENCES_WINDOW (widget));

  app = PROMPT_APPLICATION_DEFAULT;
  settings = prompt_application_get_settings (app);

  font_name = prompt_settings_dup_font_name (settings);
  if (prompt_str_empty0 (font_name))
    g_set_str (&font_name, prompt_application_get_system_font_name (app));

  font_desc = pango_font_description_from_string (font_name);

  filter = gtk_custom_filter_new (monospace_filter, NULL, NULL);
  dialog = (GtkFontDialog *)g_object_new (GTK_TYPE_FONT_DIALOG,
                                          "title", _("Select Font"),
                                          "filter", filter,
                                          NULL);

  gtk_font_dialog_choose_font (dialog,
                               GTK_WINDOW (gtk_widget_get_root (widget)),
                               font_desc,
                               NULL,
                               prompt_preferences_window_select_custom_font_cb,
                               g_object_ref (settings));
}

static void
prompt_preferences_window_add_profile (GtkWidget  *widget,
                                       const char *action_name,
                                       GVariant   *param)
{
  g_autoptr(PromptProfile) profile = NULL;

  g_assert (PROMPT_IS_PREFERENCES_WINDOW (widget));

  profile = prompt_profile_new (NULL);

  prompt_application_add_profile (PROMPT_APPLICATION_DEFAULT, profile);
}

static void
prompt_preferences_window_profile_row_activated_cb (PromptPreferencesWindow *self,
                                                    PromptProfileRow        *row)
{
  PromptProfile *profile;

  g_assert (PROMPT_IS_PREFERENCES_WINDOW (self));
  g_assert (PROMPT_IS_PROFILE_ROW (row));

  profile = prompt_profile_row_get_profile (row);

  prompt_preferences_window_edit_profile (self, profile);
}

static gboolean
prompt_preferences_window_show_all_cb (PromptPreferencesWindow *self,
                                       GtkLinkButton           *button)
{
  g_assert (PROMPT_IS_PREFERENCES_WINDOW (self));
  g_assert (GTK_IS_LINK_BUTTON (button));

  if (gtk_filter_list_model_get_filter (self->filter_palettes) != NULL)
    {
      gtk_button_set_label (GTK_BUTTON (self->show_more), _("Show Fewer…"));
      gtk_filter_list_model_set_filter (self->filter_palettes, NULL);
    }
  else
    {
      gtk_button_set_label (GTK_BUTTON (self->show_more), _("Show More…"));
      gtk_filter_list_model_set_filter (self->filter_palettes,
                                        GTK_FILTER (self->filter));
    }

  return TRUE;
}

static GtkWidget *
prompt_preferences_window_create_profile_row_cb (gpointer item,
                                                 gpointer user_data)
{
  return prompt_profile_row_new (PROMPT_PROFILE (item));
}

static gboolean
string_to_index (GValue   *value,
                 GVariant *variant,
                 gpointer  user_data)
{
  GListModel *model = G_LIST_MODEL (user_data);
  guint n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(PromptPreferencesListItem) item = PROMPT_PREFERENCES_LIST_ITEM (g_list_model_get_item (model, i));
      GVariant *item_value = prompt_preferences_list_item_get_value (item);

      if (g_variant_equal (variant, item_value))
        {
          g_value_set_uint (value, i);
          return TRUE;
        }
    }

  return FALSE;
}

static GVariant *
index_to_string (const GValue       *value,
                 const GVariantType *type,
                 gpointer            user_data)
{
  guint index = g_value_get_uint (value);
  GListModel *model = G_LIST_MODEL (user_data);
  g_autoptr(PromptPreferencesListItem) item = PROMPT_PREFERENCES_LIST_ITEM (g_list_model_get_item (model, index));

  if (item != NULL)
    return g_variant_ref (prompt_preferences_list_item_get_value (item));

  return NULL;
}

static gboolean
map_palette_to_selected (GBinding     *binding,
                         const GValue *from_value,
                         GValue       *to_value,
                         gpointer      user_data)
{
  PromptPalette *current;
  PromptPalette *palette;

  g_assert (G_IS_BINDING (binding));
  g_assert (from_value != NULL);
  g_assert (to_value != NULL);

  palette = g_value_get_object (from_value);
  current = prompt_palette_preview_get_palette (user_data);

  g_value_set_boolean (to_value,
                       0 == g_strcmp0 (prompt_palette_get_id (palette),
                                       prompt_palette_get_id (current)));
  return TRUE;
}

static gboolean
opacity_to_label (GBinding     *binding,
                  const GValue *from_value,
                  GValue       *to_value,
                  gpointer      user_data)
{
  double opacity;

  g_assert (G_IS_BINDING (binding));
  g_assert (G_VALUE_HOLDS_DOUBLE (from_value));
  g_assert (G_VALUE_HOLDS_STRING (to_value));

  opacity = g_value_get_double (from_value);
  g_value_take_string (to_value, g_strdup_printf ("%3.0lf%%", floor (100.*opacity)));

  return TRUE;
}

static void
prompt_preferences_window_notify_default_profile_cb (PromptPreferencesWindow *self,
                                                     GParamSpec              *pspec,
                                                     PromptApplication       *app)
{
  g_autoptr(PromptProfile) profile = NULL;
  g_autoptr(GPropertyAction) palette_action = NULL;
  g_autoptr(GSettings) gsettings = NULL;
  g_autoptr(GSimpleActionGroup) group = NULL;

  g_assert (PROMPT_IS_PREFERENCES_WINDOW (self));
  g_assert (PROMPT_IS_APPLICATION (app));

  profile = prompt_application_dup_default_profile (app);
  gsettings = prompt_profile_dup_settings (profile);

  g_object_bind_property (profile, "palette-id",
                          self, "default-palette-id",
                          G_BINDING_SYNC_CREATE);

  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->palette_previews));
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      GtkWidget *button = gtk_flow_box_child_get_child (GTK_FLOW_BOX_CHILD (child));
      GtkWidget *preview = gtk_button_get_child (GTK_BUTTON (button));

      g_object_bind_property_full (profile, "palette", preview, "selected",
                                   G_BINDING_SYNC_CREATE,
                                   map_palette_to_selected, NULL,
                                   preview, NULL);
    }

  group = g_simple_action_group_new ();
  palette_action = g_property_action_new ("palette", profile, "palette-id");
  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (palette_action));
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "default-profile",
                                  G_ACTION_GROUP (group));

  g_object_bind_property (profile, "opacity",
                          self->opacity_adjustment, "value",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property_full (profile, "opacity",
                               self->opacity_label, "label",
                               G_BINDING_SYNC_CREATE,
                               opacity_to_label, NULL, NULL, NULL);
  g_object_bind_property (profile, "limit-scrollback",
                          self->limit_scrollback, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (profile, "scroll-on-output",
                          self->scroll_on_output, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (profile, "scroll-on-keystroke",
                          self->scroll_on_keystroke, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (profile, "scrollback-lines",
                          self->scrollback_lines, "value",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (profile, "bold-is-bright",
                          self->bold_is_bright, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  g_settings_bind_with_mapping (gsettings,
                                PROMPT_PROFILE_KEY_BACKSPACE_BINDING,
                                self->backspace_binding,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                string_to_index,
                                index_to_string,
                                g_object_ref (self->erase_bindings),
                                g_object_unref);

  g_settings_bind_with_mapping (gsettings,
                                PROMPT_PROFILE_KEY_DELETE_BINDING,
                                self->delete_binding,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                string_to_index,
                                index_to_string,
                                g_object_ref (self->erase_bindings),
                                g_object_unref);

  g_settings_bind_with_mapping (gsettings,
                                PROMPT_PROFILE_KEY_CJK_AMBIGUOUS_WIDTH,
                                self->cjk_ambiguous_width,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                string_to_index,
                                index_to_string,
                                g_object_ref (self->cjk_ambiguous_widths),
                                g_object_unref);

  g_settings_bind_with_mapping (gsettings,
                                PROMPT_PROFILE_KEY_EXIT_ACTION,
                                self->exit_action,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                string_to_index,
                                index_to_string,
                                g_object_ref (self->exit_actions),
                                g_object_unref);

  g_settings_bind_with_mapping (gsettings,
                                PROMPT_PROFILE_KEY_PRESERVE_DIRECTORY,
                                self->preserve_directory,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                string_to_index,
                                index_to_string,
                                g_object_ref (self->preserve_directories),
                                g_object_unref);
}

static gboolean
prompt_preferences_window_drop_palette_cb (PromptPreferencesWindow *self,
                                           const GValue            *value,
                                           double                   x,
                                           double                   y,
                                           GtkDropTarget           *drop_target)
{
  g_assert (PROMPT_IS_PREFERENCES_WINDOW (self));
  g_assert (GTK_IS_DROP_TARGET (drop_target));

  if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST))
    {
      const GSList *files = g_value_get_boxed (value);

      for (const GSList *iter = files; iter; iter = iter->next)
        {
          GFile *file = iter->data;
          g_autofree char *name = g_file_get_basename (file);
          g_autoptr(GFile) dest = NULL;

          if (!g_str_has_suffix (name, ".palette"))
            return FALSE;

          dest = g_file_new_build_filename (g_get_user_data_dir (),
                                            APP_ID, "palettes", name,
                                            NULL);
          g_file_copy_async (file,
                             dest,
                             G_FILE_COPY_OVERWRITE,
                             G_PRIORITY_DEFAULT,
                             NULL, NULL, NULL, NULL, NULL);
        }

      return TRUE;
    }

  return FALSE;
}

static GtkWidget *
create_palette_preview (gpointer item,
                        gpointer user_data)
{
  AdwStyleManager *style_manager = user_data;
  g_autoptr(GVariant) action_target = NULL;
  PromptPalette *palette = item;
  PromptSettings *settings;
  GtkButton *button;
  GtkWidget *preview;
  GtkWidget *child;

  g_assert (PROMPT_IS_PALETTE (palette));
  g_assert (ADW_IS_STYLE_MANAGER (style_manager));

  settings = prompt_application_get_settings (PROMPT_APPLICATION_DEFAULT);
  action_target = g_variant_take_ref (g_variant_new_string (prompt_palette_get_id (palette)));
  preview = prompt_palette_preview_new (palette);
  g_object_bind_property (style_manager, "dark", preview, "dark", G_BINDING_SYNC_CREATE);
  g_object_bind_property (settings, "font-desc", preview, "font-desc", G_BINDING_SYNC_CREATE);
  button = g_object_new (GTK_TYPE_TOGGLE_BUTTON,
                         "css-classes", (const char * const[]) { "palette", NULL },
                         "action-name", "default-profile.palette",
                         "action-target", action_target,
                         "child", preview,
                         "focus-on-click", FALSE,
                         "can-focus", FALSE,
                         "overflow", GTK_OVERFLOW_HIDDEN,
                         NULL);
  child = g_object_new (GTK_TYPE_FLOW_BOX_CHILD,
                        "child", button,
                        NULL);

  return child;
}

static void
prompt_preferences_window_constructed (GObject *object)
{
  PromptPreferencesWindow *self = (PromptPreferencesWindow *)object;
  PromptApplication *app = PROMPT_APPLICATION_DEFAULT;
  PromptSettings *settings = prompt_application_get_settings (app);
  PromptShortcuts *shortcuts = prompt_application_get_shortcuts (app);
  GSettings *gsettings = prompt_settings_get_settings (settings);
  AdwStyleManager *style_manager = adw_style_manager_get_default ();
  g_autoptr(GListModel) profiles = NULL;

  G_OBJECT_CLASS (prompt_preferences_window_parent_class)->constructed (object);

  self->filter = gtk_custom_filter_new (filter_primary, self, NULL);
  self->filter_palettes = gtk_filter_list_model_new (g_object_ref (prompt_palette_get_all ()),
                                                     g_object_ref (GTK_FILTER (self->filter)));

  gtk_flow_box_bind_model (self->palette_previews,
                           G_LIST_MODEL (self->filter_palettes),
                           create_palette_preview,
                           g_object_ref (style_manager),
                           g_object_unref);

  g_signal_connect_object (app,
                           "notify::default-profile",
                           G_CALLBACK (prompt_preferences_window_notify_default_profile_cb),
                           self,
                           G_CONNECT_SWAPPED);
  prompt_preferences_window_notify_default_profile_cb (self, NULL, app);

  g_settings_bind_with_mapping (gsettings,
                                PROMPT_SETTING_KEY_NEW_TAB_POSITION,
                                self->tab_position,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                string_to_index,
                                index_to_string,
                                g_object_ref (self->tab_positions),
                                g_object_unref);

  g_settings_bind_with_mapping (gsettings,
                                PROMPT_SETTING_KEY_CURSOR_SHAPE,
                                self->cursor_shape,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                string_to_index,
                                index_to_string,
                                g_object_ref (self->cursor_shapes),
                                g_object_unref);

  g_settings_bind_with_mapping (gsettings,
                                PROMPT_SETTING_KEY_CURSOR_BLINK_MODE,
                                self->cursor_blink_mode,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                string_to_index,
                                index_to_string,
                                g_object_ref (self->cursor_blink_modes),
                                g_object_unref);

  g_settings_bind_with_mapping (gsettings,
                                PROMPT_SETTING_KEY_SCROLLBAR_POLICY,
                                self->scrollbar_policy,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                string_to_index,
                                index_to_string,
                                g_object_ref (self->scrollbar_policies),
                                g_object_unref);

  g_settings_bind_with_mapping (gsettings,
                                PROMPT_SETTING_KEY_TEXT_BLINK_MODE,
                                self->text_blink_mode,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                string_to_index,
                                index_to_string,
                                g_object_ref (self->text_blink_modes),
                                g_object_unref);

  profiles = prompt_application_list_profiles (app);

  gtk_list_box_bind_model (self->profiles_list_box,
                           profiles,
                           prompt_preferences_window_create_profile_row_cb,
                           NULL, NULL);

  g_object_bind_property (settings, "audible-bell",
                          self->audible_bell, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (settings, "visual-bell",
                          self->visual_bell, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (settings, "restore-session",
                          self->restore_session, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (settings, "restore-window-size",
                          self->restore_window_size, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (settings, "use-system-font",
                          self->use_system_font, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (settings, "font-name",
                          self->font_name, "label",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (settings, "use-system-font",
                          self->font_name, "sensitive",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
  g_object_bind_property (settings, "use-system-font",
                          self->font_name_row, "activatable",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

  g_object_bind_property (shortcuts, "new-tab",
                          self->shortcut_new_tab, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (shortcuts, "new-window",
                          self->shortcut_new_window, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (shortcuts, "tab-overview",
                          self->shortcut_tab_overview, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  g_object_bind_property (shortcuts, "focus-tab-1",
                          self->shortcut_focus_tab_1, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (shortcuts, "focus-tab-2",
                          self->shortcut_focus_tab_2, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (shortcuts, "focus-tab-3",
                          self->shortcut_focus_tab_3, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (shortcuts, "focus-tab-4",
                          self->shortcut_focus_tab_4, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (shortcuts, "focus-tab-5",
                          self->shortcut_focus_tab_5, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (shortcuts, "focus-tab-6",
                          self->shortcut_focus_tab_6, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (shortcuts, "focus-tab-7",
                          self->shortcut_focus_tab_7, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (shortcuts, "focus-tab-8",
                          self->shortcut_focus_tab_8, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (shortcuts, "focus-tab-9",
                          self->shortcut_focus_tab_9, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (shortcuts, "focus-tab-10",
                          self->shortcut_focus_tab_10, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  g_object_bind_property (shortcuts, "toggle-fullscreen",
                          self->shortcut_toggle_fullscreen, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (shortcuts, "preferences",
                          self->shortcut_preferences, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  g_object_bind_property (shortcuts, "copy-clipboard",
                          self->shortcut_copy_clipboard, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (shortcuts, "paste-clipboard",
                          self->shortcut_paste_clipboard, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  g_object_bind_property (shortcuts, "reset",
                          self->shortcut_reset, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (shortcuts, "reset-and-clear",
                          self->shortcut_reset_and_clear, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  g_object_bind_property (shortcuts, "search",
                          self->shortcut_search, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  g_object_bind_property (shortcuts, "select-all",
                          self->shortcut_select_all, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (shortcuts, "select-none",
                          self->shortcut_select_none, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  g_object_bind_property (shortcuts, "popup-menu",
                          self->shortcut_popup_menu, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  g_object_bind_property (shortcuts, "zoom-in",
                          self->shortcut_zoom_in, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (shortcuts, "zoom-one",
                          self->shortcut_zoom_one, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (shortcuts, "zoom-out",
                          self->shortcut_zoom_out, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  g_object_bind_property (shortcuts, "close-tab",
                          self->shortcut_close_tab, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (shortcuts, "close-other-tabs",
                          self->shortcut_close_other_tabs, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (shortcuts, "undo-close-tab",
                          self->shortcut_undo_close_tab, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  g_object_bind_property (shortcuts, "move-next-tab",
                          self->shortcut_move_next_tab, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (shortcuts, "move-previous-tab",
                          self->shortcut_move_previous_tab, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  g_object_bind_property (shortcuts, "move-tab-left",
                          self->shortcut_move_tab_left, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (shortcuts, "move-tab-right",
                          self->shortcut_move_tab_right, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (shortcuts, "detach-tab",
                          self->shortcut_detach_tab, "accelerator",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
}

static void
prompt_preferences_window_dispose (GObject *object)
{
  PromptPreferencesWindow *self = (PromptPreferencesWindow *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), PROMPT_TYPE_PREFERENCES_WINDOW);

  g_clear_object (&self->filter);
  g_clear_object (&self->filter_palettes);

  G_OBJECT_CLASS (prompt_preferences_window_parent_class)->dispose (object);
}

static void
prompt_preferences_window_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  PromptPreferencesWindow *self = PROMPT_PREFERENCES_WINDOW (object);

  switch (prop_id)
    {
    case PROP_DEFAULT_PALETTE_ID:
      g_value_set_string (value, self->default_palette_id);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_preferences_window_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  PromptPreferencesWindow *self = PROMPT_PREFERENCES_WINDOW (object);

  switch (prop_id)
    {
    case PROP_DEFAULT_PALETTE_ID:
      if (g_set_str (&self->default_palette_id, g_value_get_string (value)))
        {
          gtk_filter_changed (GTK_FILTER (self->filter), GTK_FILTER_CHANGE_DIFFERENT);
          g_object_notify_by_pspec (G_OBJECT (self), pspec);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_preferences_window_class_init (PromptPreferencesWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = prompt_preferences_window_constructed;
  object_class->dispose = prompt_preferences_window_dispose;
  object_class->get_property = prompt_preferences_window_get_property;
  object_class->set_property = prompt_preferences_window_set_property;

  properties[PROP_DEFAULT_PALETTE_ID] =
    g_param_spec_string ("default-palette-id", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Prompt/prompt-preferences-window.ui");

  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, add_profile_row);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, audible_bell);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, backspace_binding);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, bold_is_bright);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, cjk_ambiguous_width);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, cjk_ambiguous_widths);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, cursor_blink_mode);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, cursor_blink_modes);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, cursor_shape);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, cursor_shapes);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, delete_binding);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, erase_bindings);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, exit_action);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, exit_actions);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, font_name);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, font_name_row);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, limit_scrollback);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, opacity_adjustment);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, opacity_label);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, palette_previews);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, preserve_directories);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, preserve_directory);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, profiles_list_box);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, restore_session);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, restore_window_size);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, scroll_on_keystroke);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, scroll_on_output);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, scrollback_lines);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, scrollbar_policies);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, scrollbar_policy);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_close_other_tabs);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_close_tab);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_copy_clipboard);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_detach_tab);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_focus_tab_1);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_focus_tab_10);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_focus_tab_2);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_focus_tab_3);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_focus_tab_4);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_focus_tab_5);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_focus_tab_6);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_focus_tab_7);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_focus_tab_8);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_focus_tab_9);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_move_next_tab);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_move_previous_tab);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_move_tab_left);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_move_tab_right);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_new_tab);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_new_window);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_paste_clipboard);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_popup_menu);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_preferences);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_reset);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_reset_and_clear);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_search);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_select_all);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_select_none);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_tab_overview);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_toggle_fullscreen);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_undo_close_tab);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_zoom_in);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_zoom_one);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, shortcut_zoom_out);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, show_more);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, tab_position);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, tab_positions);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, text_blink_mode);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, text_blink_modes);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, use_system_font);
  gtk_widget_class_bind_template_child (widget_class, PromptPreferencesWindow, visual_bell);

  gtk_widget_class_bind_template_callback (widget_class, prompt_preferences_window_profile_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, prompt_preferences_window_show_all_cb);

  gtk_widget_class_install_action (widget_class,
                                   "profile.add",
                                   NULL,
                                   prompt_preferences_window_add_profile);
  gtk_widget_class_install_action (widget_class,
                                   "settings.select-custom-font",
                                   NULL,
                                   prompt_preferences_window_select_custom_font);

  g_type_ensure (PROMPT_TYPE_PREFERENCES_LIST_ITEM);
  g_type_ensure (PROMPT_TYPE_PROFILE_EDITOR);
  g_type_ensure (PROMPT_TYPE_PROFILE_ROW);
  g_type_ensure (PROMPT_TYPE_SHORTCUT_ROW);
}

static void
prompt_preferences_window_init (PromptPreferencesWindow *self)
{
  GtkDropTarget *drop_target;

  gtk_widget_init_template (GTK_WIDGET (self));

  drop_target = gtk_drop_target_new (GDK_TYPE_FILE_LIST, GDK_ACTION_COPY);
  g_signal_connect_object (drop_target,
                           "drop",
                           G_CALLBACK (prompt_preferences_window_drop_palette_cb),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_widget_add_controller (GTK_WIDGET (self->palette_previews),
                             GTK_EVENT_CONTROLLER (drop_target));
}

GtkWindow *
prompt_preferences_window_new (GtkApplication* application)
{
  return g_object_new (PROMPT_TYPE_PREFERENCES_WINDOW,
                       NULL);
}

void
prompt_preferences_window_edit_profile (PromptPreferencesWindow *self,
                                        PromptProfile           *profile)
{
  PromptProfileEditor *editor;

  g_return_if_fail (PROMPT_IS_PREFERENCES_WINDOW (self));
  g_return_if_fail (PROMPT_IS_PROFILE (profile));

  editor = prompt_profile_editor_new (profile);

  adw_preferences_window_pop_subpage (ADW_PREFERENCES_WINDOW (self));
  adw_preferences_window_push_subpage (ADW_PREFERENCES_WINDOW (self),
                                       ADW_NAVIGATION_PAGE (editor));
}

/**
 * prompt_preferences_window_get_default:
 *
 * Gets the default preferences window for the process.
 *
 * Returns: (transfer none): a #PromptPreferencesWindow
 */
PromptPreferencesWindow *
prompt_preferences_window_get_default (void)
{
  static PromptPreferencesWindow *instance;

  if (instance == NULL)
    {
      g_autoptr(GtkWindowGroup) sole_group = gtk_window_group_new ();

      instance = g_object_new (PROMPT_TYPE_PREFERENCES_WINDOW,
                               "modal", FALSE,
                               NULL);
      gtk_window_group_add_window (sole_group, GTK_WINDOW (instance));
      g_object_add_weak_pointer (G_OBJECT (instance), (gpointer *)&instance);
    }

  return instance;
}

void
prompt_preferences_window_edit_shortcuts (PromptPreferencesWindow *self)
{
  g_return_if_fail (PROMPT_IS_PREFERENCES_WINDOW (self));

  adw_preferences_window_pop_subpage (ADW_PREFERENCES_WINDOW (self));
  adw_preferences_window_set_visible_page_name (ADW_PREFERENCES_WINDOW (self), "shortcuts");
}
