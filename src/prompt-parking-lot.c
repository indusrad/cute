/*
 * prompt-parking-lot.c
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

#include "prompt-parking-lot.h"

#define DEFAULT_TIMEOUT_SECONDS 5

typedef struct _PromptParkedTab
{
  GList             link;
  PromptParkingLot *lot;
  PromptTab        *tab;
  guint             source_id;
} PromptParkedTab;

struct _PromptParkingLot
{
  GObject parent_instance;
  GQueue  tabs;
  guint   timeout;
};

enum {
  PROP_0,
  PROP_TIMEOUT,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PromptParkingLot, prompt_parking_lot, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
prompt_parking_lot_remove (PromptParkingLot *self,
                           PromptParkedTab  *parked)
{
  g_clear_handle_id (&parked->source_id, g_source_remove);
  g_queue_unlink (&self->tabs, &parked->link);
  g_clear_object (&parked->tab);
  parked->lot = NULL;
  g_free (parked);
}

static void
prompt_parking_lot_dispose (GObject *object)
{
  PromptParkingLot *self = (PromptParkingLot *)object;

  while (self->tabs.head != NULL)
    prompt_parking_lot_remove (self, self->tabs.head->data);

  G_OBJECT_CLASS (prompt_parking_lot_parent_class)->dispose (object);
}

static void
prompt_parking_lot_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  PromptParkingLot *self = PROMPT_PARKING_LOT (object);

  switch (prop_id)
    {
    case PROP_TIMEOUT:
      g_value_set_uint (value, self->timeout);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_parking_lot_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  PromptParkingLot *self = PROMPT_PARKING_LOT (object);

  switch (prop_id)
    {
    case PROP_TIMEOUT:
      prompt_parking_lot_set_timeout (self, g_value_get_uint (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
prompt_parking_lot_class_init (PromptParkingLotClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = prompt_parking_lot_dispose;
  object_class->get_property = prompt_parking_lot_get_property;
  object_class->set_property = prompt_parking_lot_set_property;

  properties[PROP_TIMEOUT] =
    g_param_spec_uint ("timeout", NULL, NULL,
                       0, G_MAXUINT, DEFAULT_TIMEOUT_SECONDS,
                       (G_PARAM_READWRITE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
prompt_parking_lot_init (PromptParkingLot *self)
{
  self->timeout = DEFAULT_TIMEOUT_SECONDS;
}

PromptParkingLot *
prompt_parking_lot_new (void)
{
  return g_object_new (PROMPT_TYPE_PARKING_LOT, NULL);
}

guint
prompt_parking_lot_get_timeout (PromptParkingLot *self)
{
  g_return_val_if_fail (PROMPT_IS_PARKING_LOT (self), 0);

  return self->timeout;
}

void
prompt_parking_lot_set_timeout (PromptParkingLot *self,
                                guint             timeout)
{
  g_return_if_fail (PROMPT_IS_PARKING_LOT (self));

  if (timeout != self->timeout)
    {
      self->timeout = timeout;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TIMEOUT]);
    }
}

static gboolean
prompt_parking_lot_source_func (gpointer data)
{
  PromptParkedTab *parked = data;

  g_assert (parked != NULL);
  g_assert (PROMPT_IS_PARKING_LOT (parked->lot));
  g_assert (PROMPT_IS_TAB (parked->tab));
  g_assert (parked->source_id != 0);

  prompt_parking_lot_remove (parked->lot, parked);

  return G_SOURCE_REMOVE;
}

void
prompt_parking_lot_push (PromptParkingLot *self,
                         PromptTab        *tab)
{
  PromptParkedTab *parked;

  g_return_if_fail (PROMPT_IS_PARKING_LOT (self));
  g_return_if_fail (PROMPT_IS_TAB (tab));

  parked = g_new0 (PromptParkedTab, 1);
  parked->link.data = parked;
  parked->tab = g_object_ref (tab);
  parked->lot = self;
  parked->source_id = g_timeout_add_seconds_full (G_PRIORITY_LOW,
                                                  self->timeout,
                                                  prompt_parking_lot_source_func,
                                                  parked, NULL);

  g_queue_push_tail_link (&self->tabs, &parked->link);
}

/**
 * prompt_parking_lot_pop:
 * @self: a #PromptParkingLot
 *
 * Returns: (transfer full) (nullable): a #PromptTab or %NULL
 */
PromptTab *
prompt_parking_lot_pop (PromptParkingLot *self)
{
  PromptParkedTab *parked;
  PromptTab *ret = NULL;

  g_return_val_if_fail (PROMPT_IS_PARKING_LOT (self), NULL);

  if ((parked = g_queue_peek_head (&self->tabs)))
    {
      ret = g_steal_pointer (&parked->tab);
      prompt_parking_lot_remove (self, parked);
    }

  return ret;
}
