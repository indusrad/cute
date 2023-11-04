/*
 * capsule-palette.c
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

#include "capsule-palette.h"
#include "capsule-util.h"

typedef struct _CapsulePaletteData
{
  const char *id;
  const char *name;
  GdkRGBA background;
  GdkRGBA foreground;
  GdkRGBA indexed[16];
} CapsulePaletteData;

struct _CapsulePalette
{
  GObject parent_instance;
  const CapsulePaletteData *palette;
};

enum {
  PROP_0,
  PROP_BACKGROUND,
  PROP_FOREGROUND,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (CapsulePalette, capsule_palette, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static const CapsulePaletteData palettes[] = {
  {
    .id = "gnome",
    .name = N_("GNOME"),
    .foreground = GDK_RGBA ("1e1e1e"),
    .background = GDK_RGBA ("ffffff"),
    .indexed = {
      GDK_RGBA ("1e1e1e"),
      GDK_RGBA ("c01c28"),
      GDK_RGBA ("26a269"),
      GDK_RGBA ("a2734c"),
      GDK_RGBA ("12488b"),
      GDK_RGBA ("a347ba"),
      GDK_RGBA ("2aa1b3"),
      GDK_RGBA ("ffffff"),
      GDK_RGBA ("5e5c64"),
      GDK_RGBA ("f66151"),
      GDK_RGBA ("33d17a"),
      GDK_RGBA ("e9ad0c"),
      GDK_RGBA ("2a7bde"),
      GDK_RGBA ("c061cb"),
      GDK_RGBA ("33c7de"),
      GDK_RGBA ("d0cfcc"),
    }
  }
};

static void
capsule_palette_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  CapsulePalette *self = CAPSULE_PALETTE (object);

  switch (prop_id)
    {
    case PROP_BACKGROUND:
      g_value_set_boxed (value, capsule_palette_get_background (self));
      break;

    case PROP_FOREGROUND:
      g_value_set_boxed (value, capsule_palette_get_foreground (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
capsule_palette_class_init (CapsulePaletteClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = capsule_palette_get_property;

  properties[PROP_BACKGROUND] =
    g_param_spec_boxed ("background", NULL, NULL,
                        GDK_TYPE_RGBA,
                        (G_PARAM_READABLE |
                         G_PARAM_STATIC_STRINGS));

  properties[PROP_FOREGROUND] =
    g_param_spec_boxed ("foreground", NULL, NULL,
                        GDK_TYPE_RGBA,
                        (G_PARAM_READABLE |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
capsule_palette_init (CapsulePalette *self)
{
}

const GdkRGBA *
capsule_palette_get_background (CapsulePalette *self)
{
  g_return_val_if_fail (CAPSULE_IS_PALETTE (self), NULL);

  return &self->palette->background;
}

const GdkRGBA *
capsule_palette_get_foreground (CapsulePalette *self)
{
  g_return_val_if_fail (CAPSULE_IS_PALETTE (self), NULL);

  return &self->palette->foreground;
}

const GdkRGBA *
capsule_palette_get_indexed_color (CapsulePalette *self,
                                   guint           index)
{
  g_return_val_if_fail (CAPSULE_IS_PALETTE (self), NULL);

  if (index >= 16)
    return NULL;

  return &self->palette->indexed[index];
}

CapsulePalette *
capsule_palette_new_from_name (const char *name)
{
  const CapsulePaletteData *data = NULL;
  CapsulePalette *self;

  for (guint i = 0; i < G_N_ELEMENTS (palettes); i++)
    {
      if (g_strcmp0 (name, palettes[i].id) == 0)
        {
          data = &palettes[i];
          break;
        }
    }

  if (data == NULL)
    data = &palettes[0];

  self = g_object_new (CAPSULE_TYPE_PALETTE, NULL);
  self->palette = data;

  return self;
}

const char *
capsule_palette_get_id (CapsulePalette *self)
{
  g_return_val_if_fail (CAPSULE_IS_PALETTE (self), NULL);

  return self->palette->id;
}

const char *
capsule_palette_get_name (CapsulePalette *self)
{
  g_return_val_if_fail (CAPSULE_IS_PALETTE (self), NULL);

  return self->palette->name;
}

const GdkRGBA *
capsule_palette_get_indexed_colors (CapsulePalette *self,
                                    guint          *n_colors)
{
  g_return_val_if_fail (CAPSULE_IS_PALETTE (self), NULL);
  g_return_val_if_fail (n_colors != NULL, NULL);

  *n_colors = 16;
  return self->palette->indexed;
}
