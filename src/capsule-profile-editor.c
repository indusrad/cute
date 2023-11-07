/*
 * capsule-profile-editor.c
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

#include <glib/gi18n.h>

#include "capsule-profile-editor.h"
#include "capsule-preferences-list-item.h"

struct _CapsuleProfileEditor
{
  AdwNavigationPage  parent_instance;

  CapsuleProfile    *profile;

  AdwEntryRow       *label;
  AdwSwitchRow      *bold_is_bright;
  AdwSwitchRow      *use_custom_commmand;
  AdwSwitchRow      *login_shell;
  AdwSpinRow        *scrollback_lines;
  AdwSwitchRow      *limit_scrollback;
  AdwSwitchRow      *scroll_on_keystroke;
  AdwSwitchRow      *scroll_on_output;
  AdwComboRow       *exit_action;
  GListModel        *exit_actions;
  AdwComboRow       *palette;
  AdwComboRow       *preserve_directory;
  GListModel        *preserve_directories;
  AdwEntryRow       *custom_commmand;
  GtkScale          *opacity;
  GtkAdjustment     *opacity_adjustment;
  AdwToastOverlay   *toasts;
  GtkLabel          *uuid;
  GListStore        *erase_bindings;
  AdwComboRow       *backspace_binding;
  AdwComboRow       *delete_binding;
  AdwComboRow       *cjk_ambiguous_width;
  GListStore        *cjk_ambiguous_widths;
};

enum {
  PROP_0,
  PROP_PROFILE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (CapsuleProfileEditor, capsule_profile_editor, ADW_TYPE_NAVIGATION_PAGE)

static GParamSpec *properties[N_PROPS];

static gboolean
string_to_index (GValue   *value,
                 GVariant *variant,
                 gpointer  user_data)
{
  GListModel *model = G_LIST_MODEL (user_data);
  guint n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(CapsulePreferencesListItem) item = CAPSULE_PREFERENCES_LIST_ITEM (g_list_model_get_item (model, i));
      GVariant *item_value = capsule_preferences_list_item_get_value (item);

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
  g_autoptr(CapsulePreferencesListItem) item = CAPSULE_PREFERENCES_LIST_ITEM (g_list_model_get_item (model, index));

  if (item != NULL)
    return g_variant_ref (capsule_preferences_list_item_get_value (item));

  return NULL;
}

static void
capsule_profile_editor_uuid_copy (GtkWidget  *widget,
                                  const char *action_name,
                                  GVariant   *param)
{
  CapsuleProfileEditor *self = (CapsuleProfileEditor *)widget;
  GdkClipboard *clipboard;
  AdwToast *toast;

  g_assert (CAPSULE_IS_PROFILE_EDITOR (self));

  clipboard = gtk_widget_get_clipboard (widget);
  gdk_clipboard_set_text (clipboard, capsule_profile_get_uuid (self->profile));

  toast = g_object_new (ADW_TYPE_TOAST,
                        "title", _("Copied to clipboard"),
                        "timeout", 3,
                        NULL);
  adw_toast_overlay_add_toast (self->toasts, toast);
}

static void
capsule_profile_editor_constructed (GObject *object)
{
  CapsuleProfileEditor *self = (CapsuleProfileEditor *)object;
  g_autoptr(GSettings) gsettings = NULL;

  G_OBJECT_CLASS (capsule_profile_editor_parent_class)->constructed (object);

  adw_combo_row_set_model (self->palette,
                           capsule_palette_list_model_get_default ());

  gsettings = capsule_profile_dup_settings (self->profile);

  g_object_bind_property (self->profile, "uuid",
                          self->uuid, "label",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (self->profile, "label",
                          self->label, "text",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (self->profile, "limit-scrollback",
                          self->limit_scrollback, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (self->profile, "scrollback-lines",
                          self->scrollback_lines, "value",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (self->profile, "scroll-on-keystroke",
                          self->scroll_on_keystroke, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (self->profile, "scroll-on-output",
                          self->scroll_on_output, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (self->profile, "bold-is-bright",
                          self->bold_is_bright, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (self->profile, "login-shell",
                          self->login_shell, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (self->profile, "use-custom-command",
                          self->use_custom_commmand, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (self->profile, "custom-command",
                          self->custom_commmand, "text",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (self->profile, "opacity",
                          self->opacity_adjustment, "value",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  g_settings_bind_with_mapping (gsettings,
                                CAPSULE_PROFILE_KEY_PALETTE,
                                self->palette,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                string_to_index,
                                index_to_string,
                                g_object_ref (capsule_palette_list_model_get_default ()),
                                g_object_unref);

  g_settings_bind_with_mapping (gsettings,
                                CAPSULE_PROFILE_KEY_BACKSPACE_BINDING,
                                self->backspace_binding,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                string_to_index,
                                index_to_string,
                                g_object_ref (self->erase_bindings),
                                g_object_unref);

  g_settings_bind_with_mapping (gsettings,
                                CAPSULE_PROFILE_KEY_DELETE_BINDING,
                                self->delete_binding,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                string_to_index,
                                index_to_string,
                                g_object_ref (self->erase_bindings),
                                g_object_unref);

  g_settings_bind_with_mapping (gsettings,
                                CAPSULE_PROFILE_KEY_CJK_AMBIGUOUS_WIDTH,
                                self->cjk_ambiguous_width,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                string_to_index,
                                index_to_string,
                                g_object_ref (self->cjk_ambiguous_widths),
                                g_object_unref);

  g_settings_bind_with_mapping (gsettings,
                                CAPSULE_PROFILE_KEY_PRESERVE_DIRECTORY,
                                self->preserve_directory,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                string_to_index,
                                index_to_string,
                                g_object_ref (self->preserve_directories),
                                g_object_unref);

  g_settings_bind_with_mapping (gsettings,
                                CAPSULE_PROFILE_KEY_EXIT_ACTION,
                                self->exit_action,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                string_to_index,
                                index_to_string,
                                g_object_ref (self->exit_actions),
                                g_object_unref);
}

static void
capsule_profile_editor_dispose (GObject *object)
{
  CapsuleProfileEditor *self = (CapsuleProfileEditor *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), CAPSULE_TYPE_PROFILE_EDITOR);

  g_clear_object (&self->profile);

  G_OBJECT_CLASS (capsule_profile_editor_parent_class)->dispose (object);
}

static void
capsule_profile_editor_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  CapsuleProfileEditor *self = CAPSULE_PROFILE_EDITOR (object);

  switch (prop_id)
    {
    case PROP_PROFILE:
      g_value_set_object (value, capsule_profile_editor_get_profile (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
capsule_profile_editor_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  CapsuleProfileEditor *self = CAPSULE_PROFILE_EDITOR (object);

  switch (prop_id)
    {
    case PROP_PROFILE:
      self->profile = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
capsule_profile_editor_class_init (CapsuleProfileEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = capsule_profile_editor_constructed;
  object_class->dispose = capsule_profile_editor_dispose;
  object_class->get_property = capsule_profile_editor_get_property;
  object_class->set_property = capsule_profile_editor_set_property;

  properties[PROP_PROFILE] =
    g_param_spec_object ("profile", NULL, NULL,
                         CAPSULE_TYPE_PROFILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Capsule/capsule-profile-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, CapsuleProfileEditor, backspace_binding);
  gtk_widget_class_bind_template_child (widget_class, CapsuleProfileEditor, bold_is_bright);
  gtk_widget_class_bind_template_child (widget_class, CapsuleProfileEditor, cjk_ambiguous_width);
  gtk_widget_class_bind_template_child (widget_class, CapsuleProfileEditor, cjk_ambiguous_widths);
  gtk_widget_class_bind_template_child (widget_class, CapsuleProfileEditor, custom_commmand);
  gtk_widget_class_bind_template_child (widget_class, CapsuleProfileEditor, delete_binding);
  gtk_widget_class_bind_template_child (widget_class, CapsuleProfileEditor, erase_bindings);
  gtk_widget_class_bind_template_child (widget_class, CapsuleProfileEditor, exit_action);
  gtk_widget_class_bind_template_child (widget_class, CapsuleProfileEditor, exit_actions);
  gtk_widget_class_bind_template_child (widget_class, CapsuleProfileEditor, label);
  gtk_widget_class_bind_template_child (widget_class, CapsuleProfileEditor, limit_scrollback);
  gtk_widget_class_bind_template_child (widget_class, CapsuleProfileEditor, login_shell);
  gtk_widget_class_bind_template_child (widget_class, CapsuleProfileEditor, opacity);
  gtk_widget_class_bind_template_child (widget_class, CapsuleProfileEditor, opacity_adjustment);
  gtk_widget_class_bind_template_child (widget_class, CapsuleProfileEditor, palette);
  gtk_widget_class_bind_template_child (widget_class, CapsuleProfileEditor, preserve_directories);
  gtk_widget_class_bind_template_child (widget_class, CapsuleProfileEditor, preserve_directory);
  gtk_widget_class_bind_template_child (widget_class, CapsuleProfileEditor, scroll_on_keystroke);
  gtk_widget_class_bind_template_child (widget_class, CapsuleProfileEditor, scroll_on_output);
  gtk_widget_class_bind_template_child (widget_class, CapsuleProfileEditor, scrollback_lines);
  gtk_widget_class_bind_template_child (widget_class, CapsuleProfileEditor, toasts);
  gtk_widget_class_bind_template_child (widget_class, CapsuleProfileEditor, use_custom_commmand);
  gtk_widget_class_bind_template_child (widget_class, CapsuleProfileEditor, uuid);

  gtk_widget_class_install_action (widget_class, "uuid.copy", NULL, capsule_profile_editor_uuid_copy);
}

static void
capsule_profile_editor_init (CapsuleProfileEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

CapsuleProfileEditor *
capsule_profile_editor_new (CapsuleProfile *profile)
{
  g_return_val_if_fail (CAPSULE_IS_PROFILE (profile), NULL);

  return g_object_new (CAPSULE_TYPE_PROFILE_EDITOR,
                       "profile", profile,
                       NULL);
}

CapsuleProfile *
capsule_profile_editor_get_profile (CapsuleProfileEditor *self)
{
  g_return_val_if_fail (CAPSULE_IS_PROFILE_EDITOR (self), NULL);

  return self->profile;
}
