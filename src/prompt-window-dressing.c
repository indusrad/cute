/*
 * prompt-window-dressing.c
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

#include "gdkhslaprivate.h"

#include "prompt-application.h"
#include "prompt-window-dressing.h"

struct _PromptWindowDressing
{
  GObject         parent_instance;
  GWeakRef        window_wr;
  PromptPalette  *palette;
  GtkCssProvider *css_provider;
  char           *css_class;
  double          opacity;
  guint           queued_update;
};

enum {
  PROP_0,
  PROP_OPACITY,
  PROP_PALETTE,
  PROP_WINDOW,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PromptWindowDressing, prompt_window_dressing, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];
static guint last_sequence;

static gboolean
rgba_is_dark (const GdkRGBA *rgba)
{
  /* http://alienryderflex.com/hsp.html */
  double r = rgba->red * 255.0;
  double g = rgba->green * 255.0;
  double b = rgba->blue * 255.0;
  double hsp = sqrt (0.299 * (r * r) +
                     0.587 * (g * g) +
                     0.114 * (b * b));

  return hsp <= 127.5;
}

static void
prompt_window_dressing_update (PromptWindowDressing *self)
{
  g_autoptr(GString) string = NULL;

  g_assert (PROMPT_IS_WINDOW_DRESSING (self));

  string = g_string_new (NULL);

  if (self->palette != NULL)
    {
      PromptSettings *settings = prompt_application_get_settings (PROMPT_APPLICATION_DEFAULT);
      AdwStyleManager *style_manager = adw_style_manager_get_default ();
      gboolean dark = adw_style_manager_get_dark (style_manager);
      const PromptPaletteFace *face = prompt_palette_get_face (self->palette, dark);
      g_autoptr(GString) gstring = g_string_new (NULL);
      g_autofree char *bg = NULL;
      g_autofree char *fg = NULL;
      g_autofree char *titlebar_bg = NULL;
      g_autofree char *titlebar_fg = NULL;
      g_autofree char *su_fg = NULL;
      g_autofree char *su_bg = NULL;
      g_autofree char *rm_fg = NULL;
      g_autofree char *rm_bg = NULL;
      g_autofree char *bell_fg = NULL;
      g_autofree char *bell_bg = NULL;
      g_autofree char *new_tab_bg_str = NULL;
      g_autofree char *new_tab_fg_str = NULL;
      char window_alpha_str[G_ASCII_DTOSTR_BUF_SIZE];
      char popover_alpha_str[G_ASCII_DTOSTR_BUF_SIZE];
      gboolean visual_process_leader;
      GdkRGBA new_tab_bg;
      GdkRGBA new_tab_fg;
      double window_alpha;
      double popover_alpha;

      bg = gdk_rgba_to_string (&face->background);
      fg = gdk_rgba_to_string (&face->foreground);
      titlebar_bg = gdk_rgba_to_string (&face->titlebar_background);
      titlebar_fg = gdk_rgba_to_string (&face->titlebar_foreground);
      rm_fg = gdk_rgba_to_string (&face->scarves[PROMPT_PALETTE_SCARF_REMOTE].foreground);
      rm_bg = gdk_rgba_to_string (&face->scarves[PROMPT_PALETTE_SCARF_REMOTE].background);
      su_fg = gdk_rgba_to_string (&face->scarves[PROMPT_PALETTE_SCARF_SUPERUSER].foreground);
      su_bg = gdk_rgba_to_string (&face->scarves[PROMPT_PALETTE_SCARF_SUPERUSER].background);
      bell_fg = gdk_rgba_to_string (&face->scarves[PROMPT_PALETTE_SCARF_VISUAL_BELL].foreground);
      bell_bg = gdk_rgba_to_string (&face->scarves[PROMPT_PALETTE_SCARF_VISUAL_BELL].background);

      window_alpha = self->opacity;
      popover_alpha = MAX (window_alpha, 0.85);

      g_ascii_dtostr (popover_alpha_str, sizeof popover_alpha_str, popover_alpha);
      g_ascii_dtostr (window_alpha_str, sizeof window_alpha_str, window_alpha);

      g_string_append_printf (string,
                              "window.%s { color: %s; background-color: alpha(%s, %s); }\n",
                              self->css_class, fg, bg, window_alpha_str);
      g_string_append_printf (string,
                              "window.%s popover > contents { color: %s; background-color: alpha(%s, %s); }\n",
                              self->css_class, titlebar_fg, titlebar_bg, popover_alpha_str);
      g_string_append_printf (string,
                              "window.%s popover > arrow { background-color: alpha(%s, %s); }\n",
                              self->css_class, titlebar_bg, popover_alpha_str);
      g_string_append_printf (string,
                              "window.%s vte-terminal > revealer.size label { color: %s; background-color: alpha(%s, %s); }\n",
                              self->css_class, titlebar_fg, titlebar_bg, popover_alpha_str);
      /* It would be super if we could make these match the color of the
       * actual tab contents rather than the active tab profile.
       */
      g_string_append_printf (string,
                              "window.%s toolbarview.overview overlay.card { background-color: %s; color: %s; }\n",
                              self->css_class, bg, fg);
      g_string_append_printf (string,
                              "window.%s toolbarview.overview tabthumbnail .icon-title-box { color: %s; }\n",
                              self->css_class, fg);
      g_string_append_printf (string,
                              "window.%s toolbarview.overview.background { background-color: shade(%s,%s); }\n",
                              self->css_class, bg,
                              rgba_is_dark (&face->background) ? "1.2" : ".95");
      g_string_append_printf (string,
                              "window.%s revealer.raised.top-bar { background-color: %s; color: %s; }\n",
                              self->css_class, titlebar_bg, titlebar_fg);
      g_string_append_printf (string,
                              "window.%s box.visual-bell headerbar { background-color: transparent; }\n"
                              "window.%s box.visual-bell { animation: visual-bell-%s-%s 0.3s ease-out; }\n"
                              "@keyframes visual-bell-%s-%s { 50%% { background-color: %s; color: %s; } }\n",
                              self->css_class,
                              self->css_class, self->css_class, dark ? "dark" : "light",
                              self->css_class, dark ? "dark" : "light", bell_bg, bell_fg);
      g_string_append_printf (string,
                              "window.%s banner > revealer > widget { background-color: %s; color: %s; }\n",
                              self->css_class, bell_bg, bell_fg);
      g_string_append_printf (string,
                              "window.%s headerbar { background-color: %s; color: %s; }\n",
                              self->css_class, titlebar_bg, titlebar_fg);
      g_string_append_printf (string,
                              "window.%s taboverview tabthumbnail button { background-color: alpha(%s,.15); color: %s; }\n"
                              "window.%s taboverview tabthumbnail button:hover { background-color: alpha(%s,.25); }\n"
                              "window.%s taboverview tabthumbnail button:active { background-color: alpha(%s,.55); }\n",
                              self->css_class, fg, fg,
                              self->css_class, fg,
                              self->css_class, fg);

      visual_process_leader = prompt_settings_get_visual_process_leader (settings);

      if (rgba_is_dark (&face->background))
        {
          g_string_append_printf (string,
                                  "window.%s toolbarview > revealer > windowhandle { color: %s; background-color: %s; }\n",
                                  self->css_class, titlebar_fg, titlebar_bg);

          if (visual_process_leader)
            {
              g_string_append_printf (string,
                                      "window.%s.remote headerbar { background-color: %s; color: %s; }\n"
                                      "window.%s.remote toolbarview > revealer > windowhandle { background-color: %s; color: %s; }\n",
                                      self->css_class, rm_bg, rm_fg,
                                      self->css_class, rm_bg, rm_fg);
              g_string_append_printf (string,
                                      "window.%s.superuser headerbar { background-color: %s; color: %s; }\n"
                                      "window.%s.superuser toolbarview > revealer > windowhandle { background-color: %s; color: %s; }\n",
                                      self->css_class, su_bg, su_fg,
                                      self->css_class, su_bg, su_fg);
            }
        }
      else
        {
          g_string_append_printf (string,
                                  "window.%s toolbarview > revealer > windowhandle { color: %s; background-color: %s; }\n",
                                  self->css_class, titlebar_fg, titlebar_bg);

          if (visual_process_leader)
            {
              g_string_append_printf (string,
                                      "window.%s.remote headerbar { background-color: %s; color: %s; }\n"
                                      "window.%s.remote toolbarview > revealer > windowhandle { background-color: %s; color: %s; }\n",
                                      self->css_class, rm_bg, rm_fg,
                                      self->css_class, rm_bg, rm_fg);
              g_string_append_printf (string,
                                      "window.%s.superuser headerbar { background-color: %s; color: %s; }\n"
                                      "window.%s.superuser toolbarview > revealer > windowhandle { background-color: %s; color: %s; }\n",
                                      self->css_class, su_bg, su_fg,
                                      self->css_class, su_bg, su_fg);
            }
        }

#if DEVELOPMENT_BUILD
      g_string_append_printf (string,
                              "window.%s headerbar.main-header-bar { background-image: cross-fade(5%% -gtk-recolor(url(\"resource:///org/gnome/Adwaita/styles/assets/devel-symbolic.svg\")), image(transparent)); background-repeat: repeat-x; }\n",
                              self->css_class);
#endif

      if (!prompt_palette_use_adwaita (self->palette))
        {
          new_tab_bg = face->indexed[4];
          new_tab_fg = face->indexed[7];
          new_tab_bg_str = gdk_rgba_to_string (&new_tab_bg);
          new_tab_fg_str = gdk_rgba_to_string (&new_tab_fg);

          g_string_append_printf (string,
                                  "window.%s taboverview button.new-tab-button { background-color: %s; color: %s; }\n"
                                  "window.%s taboverview button.new-tab-button:hover { background-color: shade(%s,.95); }\n"
                                  "window.%s taboverview button.new-tab-button:active { background-color: shade(%s,.90); }\n",
                                  self->css_class, new_tab_bg_str, new_tab_fg_str,
                                  self->css_class, new_tab_bg_str,
                                  self->css_class, new_tab_bg_str);
        }
    }

  gtk_css_provider_load_from_string (self->css_provider, string->str);
}

static gboolean
prompt_window_dressing_update_idle (gpointer user_data)
{
  PromptWindowDressing *self = PROMPT_WINDOW_DRESSING (user_data);

  self->queued_update = 0;

  prompt_window_dressing_update (self);

  return G_SOURCE_REMOVE;
}

static void
prompt_window_dressing_queue_update (PromptWindowDressing *self)
{
  g_assert (PROMPT_IS_WINDOW_DRESSING (self));

  if (self->queued_update == 0)
    self->queued_update = g_idle_add_full (G_PRIORITY_HIGH_IDLE,
                                           prompt_window_dressing_update_idle,
                                           self, NULL);
}

static void
prompt_window_dressing_set_window (PromptWindowDressing *self,
                                   PromptWindow         *window)
{
  g_assert (PROMPT_IS_WINDOW_DRESSING (self));
  g_assert (PROMPT_IS_WINDOW (window));

  g_weak_ref_set (&self->window_wr, window);

  gtk_widget_add_css_class (GTK_WIDGET (window), self->css_class);
}

static void
prompt_window_dressing_constructed (GObject *object)
{
  PromptWindowDressing *self = (PromptWindowDressing *)object;
  AdwStyleManager *style_manager = adw_style_manager_get_default ();
  PromptSettings *settings = prompt_application_get_settings (PROMPT_APPLICATION_DEFAULT);

  G_OBJECT_CLASS (prompt_window_dressing_parent_class)->constructed (object);

  g_signal_connect_object (settings,
                           "notify::visual-process-leader",
                           G_CALLBACK (prompt_window_dressing_queue_update),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (self->css_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION+1);

  g_signal_connect_object (style_manager,
                           "notify::dark",
                           G_CALLBACK (prompt_window_dressing_queue_update),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (style_manager,
                           "notify::color-scheme",
                           G_CALLBACK (prompt_window_dressing_queue_update),
                           self,
                           G_CONNECT_SWAPPED);

  prompt_window_dressing_queue_update (self);
}

static void
prompt_window_dressing_dispose (GObject *object)
{
  PromptWindowDressing *self = (PromptWindowDressing *)object;

  prompt_window_dressing_set_palette (self, NULL);

  if (self->css_provider != NULL) {
    gtk_style_context_remove_provider_for_display (gdk_display_get_default (),
                                                   GTK_STYLE_PROVIDER (self->css_provider));
    g_clear_object (&self->css_provider);
  }

  g_clear_object (&self->palette);
  g_weak_ref_set (&self->window_wr, NULL);

  g_clear_handle_id (&self->queued_update, g_source_remove);

  G_OBJECT_CLASS (prompt_window_dressing_parent_class)->dispose (object);
}

static void
prompt_window_dressing_finalize (GObject *object)
{
  PromptWindowDressing *self = (PromptWindowDressing *)object;

  g_weak_ref_clear (&self->window_wr);
  g_clear_pointer (&self->css_class, g_free);

  g_assert (self->palette == NULL);
  g_assert (self->queued_update == 0);
  g_assert (self->css_provider == NULL);

  G_OBJECT_CLASS (prompt_window_dressing_parent_class)->finalize (object);
}

static void
prompt_window_dressing_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  PromptWindowDressing *self = PROMPT_WINDOW_DRESSING (object);

  switch (prop_id)
    {
    case PROP_OPACITY:
      g_value_set_double (value, prompt_window_dressing_get_opacity (self));
      break;

    case PROP_PALETTE:
      g_value_set_object (value, prompt_window_dressing_get_palette (self));
      break;

    case PROP_WINDOW:
      g_value_take_object (value, prompt_window_dressing_dup_window (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_window_dressing_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  PromptWindowDressing *self = PROMPT_WINDOW_DRESSING (object);

  switch (prop_id)
    {
    case PROP_OPACITY:
      prompt_window_dressing_set_opacity (self, g_value_get_double (value));
      break;

    case PROP_PALETTE:
      prompt_window_dressing_set_palette (self, g_value_get_object (value));
      break;

    case PROP_WINDOW:
      prompt_window_dressing_set_window (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_window_dressing_class_init (PromptWindowDressingClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = prompt_window_dressing_constructed;
  object_class->dispose = prompt_window_dressing_dispose;
  object_class->finalize = prompt_window_dressing_finalize;
  object_class->get_property = prompt_window_dressing_get_property;
  object_class->set_property = prompt_window_dressing_set_property;

  properties[PROP_OPACITY] =
    g_param_spec_double ("opacity", NULL, NULL,
                         0, 1, 1,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_PALETTE] =
    g_param_spec_object ("palette", NULL, NULL,
                         PROMPT_TYPE_PALETTE,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_WINDOW] =
    g_param_spec_object ("window", NULL, NULL,
                         PROMPT_TYPE_WINDOW,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
prompt_window_dressing_init (PromptWindowDressing *self)
{
  self->css_provider = gtk_css_provider_new ();
  self->css_class = g_strdup_printf ("window-dressing-%u", ++last_sequence);
  self->opacity = 1.0;

  g_weak_ref_init (&self->window_wr, NULL);
}

PromptWindow *
prompt_window_dressing_dup_window (PromptWindowDressing *self)
{
  g_return_val_if_fail (PROMPT_IS_WINDOW_DRESSING (self), NULL);

  return PROMPT_WINDOW (g_weak_ref_get (&self->window_wr));
}

PromptWindowDressing *
prompt_window_dressing_new (PromptWindow *window)
{
  g_return_val_if_fail (PROMPT_IS_WINDOW (window), NULL);

  return g_object_new (PROMPT_TYPE_WINDOW_DRESSING,
                       "window", window,
                       NULL);
}

PromptPalette *
prompt_window_dressing_get_palette (PromptWindowDressing *self)
{
  g_return_val_if_fail (PROMPT_IS_WINDOW_DRESSING (self), NULL);

  return self->palette;
}

void
prompt_window_dressing_set_palette (PromptWindowDressing *self,
                                    PromptPalette        *palette)
{
  g_return_if_fail (PROMPT_IS_WINDOW_DRESSING (self));
  g_return_if_fail (!palette || PROMPT_IS_PALETTE (palette));

  if (g_set_object (&self->palette, palette))
    {
      prompt_window_dressing_queue_update (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PALETTE]);
    }
}

double
prompt_window_dressing_get_opacity (PromptWindowDressing *self)
{
  g_return_val_if_fail (PROMPT_IS_WINDOW_DRESSING (self), 1.);

  return self->opacity;
}

void
prompt_window_dressing_set_opacity (PromptWindowDressing *self,
                                    double                opacity)
{
  g_return_if_fail (PROMPT_IS_WINDOW_DRESSING (self));

  if (opacity != self->opacity)
    {
      self->opacity = opacity;
      prompt_window_dressing_queue_update (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_OPACITY]);
    }
}
