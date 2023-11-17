/*
 * prompt-palette.c
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

#include "prompt-palette.h"
#include "prompt-palettes-inline.h"
#include "prompt-preferences-list-item.h"

/* If you're here, you might be wondering if there is support for
 * custom installation of palettes. Currently, the answer is no. But
 * if you were going to venture on that journey, here is how you
 * should implement it.
 *
 *  0) Add a deserialize from GFile/GKeyFile constructor
 *  1) Add a GListModel to PromptApplication to hold dyanmically
 *     loaded palettes.
 *  2) Drop palettes in something like .local/share/appname/palettes/
 *  3) The format for palettes could probably just be GKeyFile with
 *     key/value pairs for everything we have in static data. I'm
 *     sure there is another GTK based terminal which already has a
 *     reasonable palette definition like this you can borrow.
 *  4) Use a GtkFlattenListModel to join our internal and dynamic
 *     palettes together.
 *  5) Add loader to PromptApplication at startup. It's fine to
 *     just require reloading of the app to pick them up, but a
 *     GFileMonitor might be nice.
 */


struct _PromptPalette
{
  GObject parent_instance;
  const PromptPaletteData *palette;
  PromptPaletteData *allocated;
};

G_DEFINE_FINAL_TYPE (PromptPalette, prompt_palette, G_TYPE_OBJECT)

static void
prompt_palette_finalize (GObject *object)
{
  PromptPalette *self = PROMPT_PALETTE (object);

  g_clear_pointer (&self->allocated, g_free);

  G_OBJECT_CLASS (prompt_palette_parent_class)->finalize (object);
}

static void
prompt_palette_class_init (PromptPaletteClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = prompt_palette_finalize;
}

static void
prompt_palette_init (PromptPalette *self)
{
}

PromptPalette *
prompt_palette_lookup (const char *id)
{
  GListModel *model = prompt_palette_get_all ();
  guint n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(PromptPalette) palette = g_list_model_get_item (model, i);

      if (g_strcmp0 (id, prompt_palette_get_id (palette)) == 0)
        return g_steal_pointer (&palette);
    }

  return NULL;
}

PromptPalette *
prompt_palette_new_from_name (const char *name)
{
  const PromptPaletteData *data = NULL;
  PromptPalette *self;

  for (guint i = 0; i < G_N_ELEMENTS (prompt_palettes_inline); i++)
    {
      if (g_strcmp0 (name, prompt_palettes_inline[i].id) == 0)
        {
          data = &prompt_palettes_inline[i];
          break;
        }
    }

  if (data == NULL)
    data = &prompt_palettes_inline[0];

  self = g_object_new (PROMPT_TYPE_PALETTE, NULL);
  self->palette = data;

  return self;
}

const char *
prompt_palette_get_id (PromptPalette *self)
{
  g_return_val_if_fail (PROMPT_IS_PALETTE (self), NULL);

  return self->palette->id;
}

const char *
prompt_palette_get_name (PromptPalette *self)
{
  g_return_val_if_fail (PROMPT_IS_PALETTE (self), NULL);

  return self->palette->name;
}

const PromptPaletteFace *
prompt_palette_get_face (PromptPalette *self,
                         gboolean       dark)
{
  g_return_val_if_fail (PROMPT_IS_PALETTE (self), NULL);

  return &self->palette->faces[!!dark];
}

GListModel *
prompt_palette_get_all (void)
{
  static GListStore *instance;

  if (instance == NULL)
    {
      g_auto(GStrv) resources = NULL;

      instance = g_list_store_new (PROMPT_TYPE_PALETTE);
      g_object_add_weak_pointer (G_OBJECT (instance), (gpointer *)&instance);

      for (guint i = 0; i < G_N_ELEMENTS (prompt_palettes_inline); i++)
        {
          g_autoptr(PromptPalette) palette = prompt_palette_new_from_name (prompt_palettes_inline[i].id);
          g_list_store_append (instance, palette);
        }

      resources = g_resources_enumerate_children ("/org/gnome/Prompt/palettes/", 0, NULL);

      for (guint i = 0; resources[i]; i++)
        {
          g_autoptr(GError) error = NULL;
          g_autofree char *path = g_strdup_printf ("/org/gnome/Prompt/palettes/%s", resources[i]);
          g_autoptr(PromptPalette) palette = prompt_palette_new_from_resource (path, &error);

          g_assert_no_error (error);

          g_list_store_append (instance, palette);
        }
    }

  return G_LIST_MODEL (instance);
}

static gpointer
map_func (gpointer item,
          gpointer user_data)
{
  g_autoptr(PromptPalette) palette = item;
  g_autoptr(GVariant) value = g_variant_take_ref (g_variant_new_string (prompt_palette_get_id (palette)));

  return  g_object_new (PROMPT_TYPE_PREFERENCES_LIST_ITEM,
                        "title", prompt_palette_get_name (palette),
                        "value", value,
                        NULL);
}

GListModel *
prompt_palette_list_model_get_default (void)
{
  static GtkMapListModel *instance;

  if (instance == NULL)
    {
      instance = gtk_map_list_model_new (g_object_ref (prompt_palette_get_all ()),
                                         map_func, NULL, NULL);
      g_object_add_weak_pointer (G_OBJECT (instance), (gpointer *)&instance);
    }

  return G_LIST_MODEL (instance);
}

static gboolean
prompt_palette_load_color (const char  *path,
                           GdkRGBA     *color,
                           GKeyFile    *key_file,
                           const char  *scheme,
                           const char  *key,
                           GError     **error)
{
  g_autofree char *str = NULL;

  memset (color, 0, sizeof *color);

  if (!(str = g_key_file_get_string (key_file, scheme, key, NULL)))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "\"%s\" is missing %s key in %s section",
                   path, key, scheme);
      return FALSE;
    }

  if (!gdk_rgba_parse (color, str))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "\"%s\" is not a valid color for %s in section %s of \"%s\"",
                   str, key, scheme, path);
      return FALSE;
    }

  return TRUE;
}

static gboolean
prompt_palette_load_face (const char         *path,
                          PromptPaletteFace  *face,
                          GKeyFile           *key_file,
                          const char         *scheme,
                          GError            **error)
{
  if (!g_key_file_has_group (key_file, scheme))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "\"%s\" is missing %s section",
                   path, scheme);
      return FALSE;
    }

  prompt_palette_load_color (path, &face->cursor, key_file, scheme, "Cursor", NULL);

  if (FALSE ||
      !prompt_palette_load_color (path, &face->foreground, key_file, scheme, "Foreground", error) ||
      !prompt_palette_load_color (path, &face->background, key_file, scheme, "Background", error) ||
      !prompt_palette_load_color (path, &face->cursor, key_file, scheme, "Cursor", error) ||
      !prompt_palette_load_color (path, &face->indexed[0], key_file, scheme, "Color0", error) ||
      !prompt_palette_load_color (path, &face->indexed[1], key_file, scheme, "Color1", error) ||
      !prompt_palette_load_color (path, &face->indexed[2], key_file, scheme, "Color2", error) ||
      !prompt_palette_load_color (path, &face->indexed[3], key_file, scheme, "Color3", error) ||
      !prompt_palette_load_color (path, &face->indexed[4], key_file, scheme, "Color4", error) ||
      !prompt_palette_load_color (path, &face->indexed[5], key_file, scheme, "Color5", error) ||
      !prompt_palette_load_color (path, &face->indexed[6], key_file, scheme, "Color6", error) ||
      !prompt_palette_load_color (path, &face->indexed[7], key_file, scheme, "Color7", error) ||
      !prompt_palette_load_color (path, &face->indexed[8], key_file, scheme, "Color8", error) ||
      !prompt_palette_load_color (path, &face->indexed[9], key_file, scheme, "Color9", error) ||
      !prompt_palette_load_color (path, &face->indexed[10], key_file, scheme, "Color10", error) ||
      !prompt_palette_load_color (path, &face->indexed[11], key_file, scheme, "Color11", error) ||
      !prompt_palette_load_color (path, &face->indexed[12], key_file, scheme, "Color12", error) ||
      !prompt_palette_load_color (path, &face->indexed[13], key_file, scheme, "Color13", error) ||
      !prompt_palette_load_color (path, &face->indexed[14], key_file, scheme, "Color14", error) ||
      !prompt_palette_load_color (path, &face->indexed[15], key_file, scheme, "Color15", error) ||
      !prompt_palette_load_color (path, &face->visual_bell.foreground, key_file, scheme, "BellForeground", error) ||
      !prompt_palette_load_color (path, &face->visual_bell.background, key_file, scheme, "BellBackground", error) ||
      !prompt_palette_load_color (path, &face->superuser.foreground, key_file, scheme, "SuperuserForeground", error) ||
      !prompt_palette_load_color (path, &face->superuser.background, key_file, scheme, "SuperuserBackground", error) ||
      !prompt_palette_load_color (path, &face->remote.foreground, key_file, scheme, "RemoteForeground", error) ||
      !prompt_palette_load_color (path, &face->remote.background, key_file, scheme, "RemoteBackground", error) ||
      FALSE)
    return FALSE;

  return TRUE;
}

static gboolean
prompt_palette_load_info (const char         *path,
                          PromptPaletteData  *data,
                          GKeyFile           *key_file,
                          GError            **error)
{
  g_autofree char *name = NULL;
  g_autofree char *base = NULL;
  g_autofree char *id = NULL;

  if (!g_key_file_has_group (key_file, "Palette"))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "\"%s\" is missing Palette section",
                   path);
      return FALSE;
    }

  if (!(name = g_key_file_get_string (key_file, "Palette", "Name", NULL)))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "\"%s\" is missing Name key of Palette section",
                   path);
      return FALSE;
    }

  data->name = g_intern_string (name);

  base = g_path_get_basename (path);
  if (!g_str_has_suffix (base, ".palette"))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "\"%s\" does not have suffix .palette",
                   path);
      return FALSE;
    }

  id = g_strndup (base, strlen (base) - strlen (".palette"));

  data->id = g_intern_string (id);

  return TRUE;
}

PromptPalette *
prompt_palette_new_from_file (const char  *path,
                              GError     **error)
{
  g_autoptr(GKeyFile) key_file = NULL;
  PromptPaletteData data;
  PromptPalette *self;

  g_return_val_if_fail (path != NULL, NULL);

  key_file = g_key_file_new ();

  if (!g_key_file_load_from_file (key_file, path, 0, error))
    return NULL;

  memset (&data, 0, sizeof data);

  if (!prompt_palette_load_info (path, &data, key_file, error))
    return NULL;

  if (!prompt_palette_load_face (path, &data.faces[0], key_file, "Light", error))
    return NULL;

  if (!prompt_palette_load_face (path, &data.faces[1], key_file, "Dark", error))
    return NULL;

  self = g_object_new (PROMPT_TYPE_PALETTE, NULL);
  self->allocated = g_memdup2 (&data, sizeof data);
  self->palette = self->allocated;

  return self;
}

PromptPalette *
prompt_palette_new_from_resource (const char  *path,
                                  GError     **error)
{
  g_autoptr(GKeyFile) key_file = NULL;
  g_autoptr(GBytes) bytes = NULL;
  PromptPaletteData data;
  PromptPalette *self;

  g_return_val_if_fail (path != NULL, NULL);

  if (!(bytes = g_resources_lookup_data (path, 0, error)))
    return NULL;

  key_file = g_key_file_new ();

  if (!g_key_file_load_from_bytes (key_file, bytes, 0, error))
    return NULL;

  memset (&data, 0, sizeof data);

  if (!prompt_palette_load_info (path, &data, key_file, error))
    return NULL;

  if (!prompt_palette_load_face (path, &data.faces[0], key_file, "Light", error))
    return NULL;

  if (!prompt_palette_load_face (path, &data.faces[1], key_file, "Dark", error))
    return NULL;

  self = g_object_new (PROMPT_TYPE_PALETTE, NULL);
  self->allocated = g_memdup2 (&data, sizeof data);
  self->palette = self->allocated;

  return self;
}
