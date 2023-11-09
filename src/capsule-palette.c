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
#include "capsule-preferences-list-item.h"
#include "capsule-util.h"

/* If you're here, you might be wondering if there is support for
 * custom installation of palettes. Currently, the answer is no. But
 * if you were going to venture on that journey, here is how you
 * should implement it.
 *
 *  0) Add a deserialize from GFile/GKeyFile constructor
 *  1) Add a GListModel to CapsuleApplication to hold dyanmically
 *     loaded palettes.
 *  2) Drop palettes in something like .local/share/appname/palettes/
 *  3) The format for palettes could probably just be GKeyFile with
 *     key/value pairs for everything we have in static data. I'm
 *     sure there is another GTK based terminal which already has a
 *     reasonable palette definition like this you can borrow.
 *  4) Use a GtkFlattenListModel to join our internal and dynamic
 *     palettes together.
 *  5) Add loader to CapsuleApplication at startup. It's fine to
 *     just require reloading of the app to pick them up, but a
 *     GFileMonitor might be nice.
 */

typedef struct _CapsulePaletteData
{
  const char *id;
  const char *name;
  struct {
    GdkRGBA background;
    GdkRGBA foreground;
  } light;
  struct {
    GdkRGBA background;
    GdkRGBA foreground;
  } dark;
  GdkRGBA indexed[16];
} CapsulePaletteData;

struct _CapsulePalette
{
  GObject parent_instance;
  const CapsulePaletteData *palette;
};

G_DEFINE_FINAL_TYPE (CapsulePalette, capsule_palette, G_TYPE_OBJECT)

static const CapsulePaletteData palettes[] = {
  {
    .id = "gnome",
    .name = N_("GNOME"),
    .light = {
      .foreground = GDK_RGBA ("1e1e1e"),
      .background = GDK_RGBA ("ffffff"),
    },
    .dark = {
      .foreground = GDK_RGBA ("ffffff"),
      .background = GDK_RGBA ("1e1e1e"),
    },
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
  },

  {
    .id = "solarized",
    .name = N_("Solarized"),
    .light = {
      .foreground = GDK_RGBA ("657b83"),
      .background = GDK_RGBA ("fdf6e3"),
    },
    .dark = {
      .foreground = GDK_RGBA ("839496"),
      .background = GDK_RGBA ("002b36"),
    },
    .indexed = {
      GDK_RGBA ("073642"),
      GDK_RGBA ("dc322f"),
      GDK_RGBA ("859900"),
      GDK_RGBA ("b58900"),
      GDK_RGBA ("268bd2"),
      GDK_RGBA ("d33682"),
      GDK_RGBA ("2aa198"),
      GDK_RGBA ("eee8d5"),
      GDK_RGBA ("002b36"),
      GDK_RGBA ("cb4b16"),
      GDK_RGBA ("586e75"),
      GDK_RGBA ("657b83"),
      GDK_RGBA ("839496"),
      GDK_RGBA ("6c71c4"),
      GDK_RGBA ("93a1a1"),
      GDK_RGBA ("fdf6e3"),
    }
  },

  {
    .id = "tango",
    .name = N_("Tango"),
    .light = {
      .foreground = GDK_RGBA ("2e3436"),
      .background = GDK_RGBA ("eeeeec"),
    },
    .dark = {
      .foreground = GDK_RGBA ("d3d7cf"),
      .background = GDK_RGBA ("2e3436"),
    },
    .indexed = {
      GDK_RGBA ("2e3436"),
      GDK_RGBA ("cc0000"),
      GDK_RGBA ("4e9a06"),
      GDK_RGBA ("c4a000"),
      GDK_RGBA ("3465a4"),
      GDK_RGBA ("75507b"),
      GDK_RGBA ("06989a"),
      GDK_RGBA ("d3d7cf"),
      GDK_RGBA ("555753"),
      GDK_RGBA ("ef2929"),
      GDK_RGBA ("8ae234"),
      GDK_RGBA ("fce94f"),
      GDK_RGBA ("729fcf"),
      GDK_RGBA ("ad7fa8"),
      GDK_RGBA ("34e2e2"),
      GDK_RGBA ("eeeeec"),
    }
  },

  {
    .id = "dracula",
    .name = N_("Dracula"),
    .light = {
      .foreground = GDK_RGBA ("F8F8F2"),
      .background = GDK_RGBA ("282A36"),
    },
    .dark = {
      .foreground = GDK_RGBA ("F8F8F2"),
      .background = GDK_RGBA ("282A36"),
    },
    .indexed = {
      GDK_RGBA ("21222c"),
      GDK_RGBA ("ff5555"),
      GDK_RGBA ("50fa7b"),
      GDK_RGBA ("f1fa8c"),
      GDK_RGBA ("bd93f9"),
      GDK_RGBA ("ff79c6"),
      GDK_RGBA ("8be9fd"),
      GDK_RGBA ("f8f8f2"),
      GDK_RGBA ("6272a4"),
      GDK_RGBA ("ff6e6e"),
      GDK_RGBA ("69ff94"),
      GDK_RGBA ("ffffa5"),
      GDK_RGBA ("d6acff"),
      GDK_RGBA ("ff92df"),
      GDK_RGBA ("a4ffff"),
      GDK_RGBA ("ffffff"),
    }
  },

  {
    .id = "nord",
    .name = N_("Nord"),
    .light = {
      .foreground = GDK_RGBA ("d8dee9"),
      .background = GDK_RGBA ("2e3440"),
    },
    .dark = {
      .foreground = GDK_RGBA ("d8dee9"),
      .background = GDK_RGBA ("2e3440"),
    },
    .indexed = {
      GDK_RGBA ("3b4252"),
      GDK_RGBA ("bf616a"),
      GDK_RGBA ("a3be8c"),
      GDK_RGBA ("ebcb8b"),
      GDK_RGBA ("81a1c1"),
      GDK_RGBA ("b48ead"),
      GDK_RGBA ("88c0d0"),
      GDK_RGBA ("e5e9f0"),
      GDK_RGBA ("4c566a"),
      GDK_RGBA ("bf616a"),
      GDK_RGBA ("a3be8c"),
      GDK_RGBA ("ebcb8b"),
      GDK_RGBA ("81a1c1"),
      GDK_RGBA ("b48ead"),
      GDK_RGBA ("8fbcbb"),
      GDK_RGBA ("eceff4"),
    }
  },

  {
    .id = "linux",
    .name = N_("Linux"),
    .light = {
      .foreground = GDK_RGBA ("000000"),
      .background = GDK_RGBA ("ffffff"),
    },
    .dark = {
      .foreground = GDK_RGBA ("ffffff"),
      .background = GDK_RGBA ("000000"),
    },
    .indexed = {
      GDK_RGBA ("000000"),
      GDK_RGBA ("aa0000"),
      GDK_RGBA ("00aa00"),
      GDK_RGBA ("aa5500"),
      GDK_RGBA ("0000aa"),
      GDK_RGBA ("aa00aa"),
      GDK_RGBA ("00aaaa"),
      GDK_RGBA ("aaaaaa"),
      GDK_RGBA ("555555"),
      GDK_RGBA ("ff5555"),
      GDK_RGBA ("55ff55"),
      GDK_RGBA ("ffff55"),
      GDK_RGBA ("5555ff"),
      GDK_RGBA ("ff55ff"),
      GDK_RGBA ("55ffff"),
      GDK_RGBA ("ffffff")
    }
  },

  {
    .id = "xterm",
    .name = N_("XTerm"),
    .light = {
      .foreground = GDK_RGBA ("000000"),
      .background = GDK_RGBA ("ffffff"),
    },
    .dark = {
      .foreground = GDK_RGBA ("ffffff"),
      .background = GDK_RGBA ("000000"),
    },
    .indexed = {
      GDK_RGBA ("000000"),
      GDK_RGBA ("cd0000"),
      GDK_RGBA ("00cd00"),
      GDK_RGBA ("cdcd00"),
      GDK_RGBA ("0000ee"),
      GDK_RGBA ("cd00cd"),
      GDK_RGBA ("00cdcd"),
      GDK_RGBA ("e5e5e5"),
      GDK_RGBA ("7f7f7f"),
      GDK_RGBA ("ff0000"),
      GDK_RGBA ("00ff00"),
      GDK_RGBA ("ffff00"),
      GDK_RGBA ("5c5cff"),
      GDK_RGBA ("ff00ff"),
      GDK_RGBA ("00ffff"),
      GDK_RGBA ("ffffff")
    }
  },

  {
    .id = "rxvt",
    .name = N_("RXVT"),
    .light = {
      .foreground = GDK_RGBA ("000000"),
      .background = GDK_RGBA ("ffffff"),
    },
    .dark = {
      .foreground = GDK_RGBA ("ffffff"),
      .background = GDK_RGBA ("000000"),
    },
    .indexed = {
      GDK_RGBA ("000000"),
      GDK_RGBA ("cd0000"),
      GDK_RGBA ("00cd00"),
      GDK_RGBA ("cdcd00"),
      GDK_RGBA ("0000cd"),
      GDK_RGBA ("cd00cd"),
      GDK_RGBA ("00cdcd"),
      GDK_RGBA ("faebd7"),
      GDK_RGBA ("404040"),
      GDK_RGBA ("ff0000"),
      GDK_RGBA ("00ff00"),
      GDK_RGBA ("ffff00"),
      GDK_RGBA ("0000ff"),
      GDK_RGBA ("ff00ff"),
      GDK_RGBA ("00ffff"),
      GDK_RGBA ("ffffff")
    }
  },
};

static void
capsule_palette_class_init (CapsulePaletteClass *klass)
{
}

static void
capsule_palette_init (CapsulePalette *self)
{
}

const GdkRGBA *
capsule_palette_get_background (CapsulePalette *self,
                                gboolean        dark)
{
  g_return_val_if_fail (CAPSULE_IS_PALETTE (self), NULL);

  if (dark)
    return &self->palette->dark.background;
  else
    return &self->palette->light.background;
}

const GdkRGBA *
capsule_palette_get_foreground (CapsulePalette *self,
                                gboolean        dark)
{
  g_return_val_if_fail (CAPSULE_IS_PALETTE (self), NULL);

  if (dark)
    return &self->palette->dark.foreground;
  else
    return &self->palette->light.foreground;
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

GListModel *
capsule_palette_list_model_get_default (void)
{
  static GListStore *instance;

  if (instance == NULL)
    {
      instance = g_list_store_new (CAPSULE_TYPE_PREFERENCES_LIST_ITEM);
      g_object_add_weak_pointer (G_OBJECT (instance), (gpointer *)&instance);

      for (guint i = 0; i < G_N_ELEMENTS (palettes); i++)
        {
          g_autoptr(GVariant) value = g_variant_take_ref (g_variant_new_string (palettes[i].id));
          g_autoptr(CapsulePreferencesListItem) item = g_object_new (CAPSULE_TYPE_PREFERENCES_LIST_ITEM,
                                                                     "title", g_dgettext (GETTEXT_PACKAGE, palettes[i].name),
                                                                     "value", value,
                                                                     NULL);
          g_list_store_append (instance, item);
        }
    }

  return G_LIST_MODEL (instance);
}
