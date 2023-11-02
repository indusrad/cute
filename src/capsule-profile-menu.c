/*
 * capsule-profile-menu.c
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

#include "capsule-profile.h"
#include "capsule-profile-menu.h"

struct _CapsuleProfileMenu
{
  GMenuModel        parent_instance;
  CapsuleSettings  *settings;
  char            **uuids;
};

enum {
  PROP_0,
  PROP_SETTINGS,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (CapsuleProfileMenu, capsule_profile_menu, G_TYPE_MENU_MODEL)

static GParamSpec *properties [N_PROPS];

static int
capsule_profile_menu_get_n_items (GMenuModel *model)
{
  CapsuleProfileMenu *self = CAPSULE_PROFILE_MENU (model);

  return self->uuids ? g_strv_length (self->uuids) : 0;
}

static gboolean
capsule_profile_menu_is_mutable (GMenuModel *model)
{
  return TRUE;
}

static GMenuModel *
capsule_profile_menu_get_item_link (GMenuModel *model,
                                    int         position,
                                    const char *link)
{
  return NULL;
}

static void
capsule_profile_menu_get_item_links (GMenuModel  *model,
                                     int          position,
                                     GHashTable **links)
{
  *links = NULL;
}

static void
capsule_profile_menu_get_item_attributes (GMenuModel  *model,
                                          int          position,
                                          GHashTable **attributes)
{
  CapsuleProfileMenu *self = CAPSULE_PROFILE_MENU (model);
  g_autoptr(CapsuleProfile) profile = NULL;
  g_autofree char *label = NULL;
  const char *uuid;
  GHashTable *ht;

  *attributes = NULL;

  if (position < 0 ||
      self->uuids == NULL ||
      position >= g_strv_length (self->uuids))
    return;

  uuid = self->uuids[position];
  profile = capsule_profile_new (uuid);
  label = capsule_profile_dup_label (profile);

  ht = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_variant_unref);
  g_hash_table_insert (ht, g_strdup (G_MENU_ATTRIBUTE_ACTION), g_variant_ref_sink (g_variant_new_string ("win.new-terminal")));
  g_hash_table_insert (ht, g_strdup (G_MENU_ATTRIBUTE_TARGET), g_variant_ref_sink (g_variant_new_string (uuid)));
  g_hash_table_insert (ht, g_strdup (G_MENU_ATTRIBUTE_LABEL), g_variant_ref_sink (g_variant_new_string (label)));

  *attributes = ht;
}

static void
capsule_profile_menu_notify_profile_uuids_cb (CapsuleProfileMenu *self,
                                              GParamSpec         *pspec,
                                              CapsuleSettings    *settings)
{
  char **profile_uuids;
  guint old_len;
  guint new_len;

  g_assert (CAPSULE_IS_PROFILE_MENU (self));
  g_assert (CAPSULE_IS_SETTINGS (settings));

  profile_uuids = capsule_settings_dup_profile_uuids (settings);

  old_len = g_strv_length (self->uuids);
  new_len = g_strv_length (profile_uuids);

  g_clear_pointer (&self->uuids, g_strfreev);
  self->uuids = profile_uuids;

  g_menu_model_items_changed (G_MENU_MODEL (self), 0, old_len, new_len);
}

static void
capsule_profile_menu_constructed (GObject *object)
{
  CapsuleProfileMenu *self = (CapsuleProfileMenu *)object;

  G_OBJECT_CLASS (capsule_profile_menu_parent_class)->constructed (object);

  self->uuids = capsule_settings_dup_profile_uuids (self->settings);

  g_signal_connect_object (self->settings,
                           "notify::profile-uuids",
                           G_CALLBACK (capsule_profile_menu_notify_profile_uuids_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
capsule_profile_menu_dispose (GObject *object)
{
  CapsuleProfileMenu *self = (CapsuleProfileMenu *)object;

  g_clear_object (&self->settings);
  g_clear_pointer (&self->uuids, g_strfreev);

  G_OBJECT_CLASS (capsule_profile_menu_parent_class)->dispose (object);
}

static void
capsule_profile_menu_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  CapsuleProfileMenu *self = CAPSULE_PROFILE_MENU (object);

  switch (prop_id)
    {
    case PROP_SETTINGS:
      g_value_set_object (value, self->settings);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
capsule_profile_menu_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  CapsuleProfileMenu *self = CAPSULE_PROFILE_MENU (object);

  switch (prop_id)
    {
    case PROP_SETTINGS:
      self->settings = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
capsule_profile_menu_class_init (CapsuleProfileMenuClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GMenuModelClass *menu_model_class = G_MENU_MODEL_CLASS (klass);

  object_class->constructed = capsule_profile_menu_constructed;
  object_class->dispose = capsule_profile_menu_dispose;
  object_class->get_property = capsule_profile_menu_get_property;
  object_class->set_property = capsule_profile_menu_set_property;

  menu_model_class->get_n_items = capsule_profile_menu_get_n_items;
  menu_model_class->is_mutable = capsule_profile_menu_is_mutable;
  menu_model_class->get_item_link = capsule_profile_menu_get_item_link;
  menu_model_class->get_item_links = capsule_profile_menu_get_item_links;
  menu_model_class->get_item_attributes = capsule_profile_menu_get_item_attributes;

  properties[PROP_SETTINGS] =
    g_param_spec_object ("settings", NULL, NULL,
                         CAPSULE_TYPE_SETTINGS,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
capsule_profile_menu_init (CapsuleProfileMenu *self)
{
}

CapsuleProfileMenu *
capsule_profile_menu_new (CapsuleSettings *settings)
{
  g_return_val_if_fail (CAPSULE_IS_SETTINGS (settings), NULL);

  return g_object_new (CAPSULE_TYPE_PROFILE_MENU,
                       "settings", settings,
                       NULL);
}
