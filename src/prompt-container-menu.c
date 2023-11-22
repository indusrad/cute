/*
 * prompt-container-menu.c
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

#include "prompt-agent-ipc.h"

#include "prompt-container-menu.h"
#include "prompt-util.h"

struct _PromptContainerMenu
{
  GMenuModel  parent_instance;
  GListModel *containers;
};

enum {
  PROP_0,
  PROP_CONTAINERS,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PromptContainerMenu, prompt_container_menu, G_TYPE_MENU_MODEL)

static GParamSpec *properties [N_PROPS];

static int
prompt_container_menu_get_n_items (GMenuModel *model)
{
  PromptContainerMenu *self = PROMPT_CONTAINER_MENU (model);

  return g_list_model_get_n_items (self->containers);
}

static gboolean
prompt_container_menu_is_mutable (GMenuModel *model)
{
  return TRUE;
}

static GMenuModel *
prompt_container_menu_get_item_link (GMenuModel *model,
                                    int         position,
                                    const char *link)
{
  return NULL;
}

static void
prompt_container_menu_get_item_links (GMenuModel  *model,
                                     int          position,
                                     GHashTable **links)
{
  *links = NULL;
}

static void
prompt_container_menu_get_item_attributes (GMenuModel  *model,
                                          int          position,
                                          GHashTable **attributes)
{
  PromptContainerMenu *self = PROMPT_CONTAINER_MENU (model);
  g_autoptr(PromptIpcContainer) container = NULL;
  const char *label;
  const char *id;
  GHashTable *ht;

  *attributes = NULL;

  if (position < 0 || position >= g_list_model_get_n_items (self->containers))
    return;

  container = g_list_model_get_item (self->containers, position);
  label = prompt_ipc_container_get_display_name (container);
  id = prompt_ipc_container_get_id (container);

  if (prompt_str_empty0 (label))
    label = _("User Session");

  ht = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_variant_unref);
  g_hash_table_insert (ht, g_strdup (G_MENU_ATTRIBUTE_ACTION), g_variant_ref_sink (g_variant_new_string ("win.new-terminal-with-container")));
  g_hash_table_insert (ht, g_strdup (G_MENU_ATTRIBUTE_TARGET), g_variant_ref_sink (g_variant_new_string (id)));
  g_hash_table_insert (ht, g_strdup (G_MENU_ATTRIBUTE_LABEL), g_variant_ref_sink (g_variant_new_string (label)));

  *attributes = ht;
}

static void
prompt_container_menu_items_changed_cb (PromptContainerMenu *self,
                                        guint                position,
                                        guint                removed,
                                        guint                added,
                                        GListModel          *model)
{
  g_assert (PROMPT_IS_CONTAINER_MENU (self));
  g_assert (G_IS_LIST_MODEL (model));

  g_menu_model_items_changed (G_MENU_MODEL (self), position, removed, added);
}

static void
prompt_container_menu_constructed (GObject *object)
{
  PromptContainerMenu *self = (PromptContainerMenu *)object;

  G_OBJECT_CLASS (prompt_container_menu_parent_class)->constructed (object);

  g_signal_connect_object (self->containers,
                           "items-changed",
                           G_CALLBACK (prompt_container_menu_items_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
prompt_container_menu_dispose (GObject *object)
{
  PromptContainerMenu *self = (PromptContainerMenu *)object;

  g_clear_object (&self->containers);

  G_OBJECT_CLASS (prompt_container_menu_parent_class)->dispose (object);
}

static void
prompt_container_menu_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  PromptContainerMenu *self = PROMPT_CONTAINER_MENU (object);

  switch (prop_id)
    {
    case PROP_CONTAINERS:
      g_value_set_object (value, self->containers);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_container_menu_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  PromptContainerMenu *self = PROMPT_CONTAINER_MENU (object);

  switch (prop_id)
    {
    case PROP_CONTAINERS:
      self->containers = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_container_menu_class_init (PromptContainerMenuClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GMenuModelClass *menu_model_class = G_MENU_MODEL_CLASS (klass);

  object_class->constructed = prompt_container_menu_constructed;
  object_class->dispose = prompt_container_menu_dispose;
  object_class->get_property = prompt_container_menu_get_property;
  object_class->set_property = prompt_container_menu_set_property;

  menu_model_class->get_n_items = prompt_container_menu_get_n_items;
  menu_model_class->is_mutable = prompt_container_menu_is_mutable;
  menu_model_class->get_item_link = prompt_container_menu_get_item_link;
  menu_model_class->get_item_links = prompt_container_menu_get_item_links;
  menu_model_class->get_item_attributes = prompt_container_menu_get_item_attributes;

  properties[PROP_CONTAINERS] =
    g_param_spec_object ("containers", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
prompt_container_menu_init (PromptContainerMenu *self)
{
}

PromptContainerMenu *
prompt_container_menu_new (GListModel *containers)
{
  g_return_val_if_fail (G_IS_LIST_MODEL (containers), NULL);
  g_return_val_if_fail (g_type_is_a (g_list_model_get_item_type (containers),
                                     PROMPT_IPC_TYPE_CONTAINER),
                        NULL);

  return g_object_new (PROMPT_TYPE_CONTAINER_MENU,
                       "containers", containers,
                       NULL);
}
