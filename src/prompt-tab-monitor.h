/*
 * prompt-tab-monitor.h
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

#pragma once

#include <gio/gio.h>

#include "prompt-tab.h"

G_BEGIN_DECLS

typedef enum _PromptProcessLeader
{
  PROMPT_PROCESS_LEADER_KIND_UNKNOWN,
  PROMPT_PROCESS_LEADER_KIND_SUPERUSER,
  PROMPT_PROCESS_LEADER_KIND_REMOTE,
  PROMPT_PROCESS_LEADER_KIND_CONTAINER,
} PromptProcessLeaderKind;

#define PROMPT_TYPE_TAB_MONITOR (prompt_tab_monitor_get_type())

G_DECLARE_FINAL_TYPE (PromptTabMonitor, prompt_tab_monitor, PROMPT, TAB_MONITOR, GObject)

PromptTabMonitor        *prompt_tab_monitor_new                     (PromptTab        *tab);
PromptProcessLeaderKind  prompt_tab_monitor_get_process_leader_kind (PromptTabMonitor *self);

G_END_DECLS
