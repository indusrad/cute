/* capsule-process.h
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
#include <vte/vte.h>

G_BEGIN_DECLS

typedef enum _CapsuleProcessLeader
{
  CAPSULE_PROCESS_LEADER_KIND_UNKNOWN,
  CAPSULE_PROCESS_LEADER_KIND_SUPERUSER,
  CAPSULE_PROCESS_LEADER_KIND_REMOTE,
} CapsuleProcessLeaderKind;

#define CAPSULE_TYPE_PROCESS (capsule_process_get_type())

G_DECLARE_DERIVABLE_TYPE (CapsuleProcess, capsule_process, CAPSULE, PROCESS, GObject)

struct _CapsuleProcessClass
{
  GObjectClass parent_class;

  void                     (*force_exit)        (CapsuleProcess       *self);
  CapsuleProcessLeaderKind (*get_leader_kind)   (CapsuleProcess       *self);
  gboolean                 (*get_if_exited)     (CapsuleProcess       *self);
  gboolean                 (*get_if_signaled)   (CapsuleProcess       *self);
  int                      (*get_exit_status)   (CapsuleProcess       *self);
  int                      (*get_term_sig)      (CapsuleProcess       *self);
  gboolean                 (*has_leader)        (CapsuleProcess       *self);
  void                     (*wait_check_async)  (CapsuleProcess       *self,
                                                 GCancellable         *cancellable,
                                                 GAsyncReadyCallback   callback,
                                                 gpointer              user_data);
  gboolean                 (*wait_check_finish) (CapsuleProcess       *self,
                                                 GAsyncResult         *result,
                                                 GError              **error);
};

CapsuleProcess           *capsule_process_new               (GSubprocess          *subprocess,
                                                             VtePty               *pty);
void                      capsule_process_force_exit        (CapsuleProcess       *self);
CapsuleProcessLeaderKind  capsule_process_get_leader_kind   (CapsuleProcess       *self);
gboolean                  capsule_process_get_if_exited     (CapsuleProcess       *self);
gboolean                  capsule_process_get_if_signaled   (CapsuleProcess       *self);
int                       capsule_process_get_exit_status   (CapsuleProcess       *self);
int                       capsule_process_get_term_sig      (CapsuleProcess       *self);
gboolean                  capsule_process_has_leader        (CapsuleProcess       *self);
void                      capsule_process_wait_check_async  (CapsuleProcess       *self,
                                                             GCancellable         *cancellable,
                                                             GAsyncReadyCallback   callback,
                                                             gpointer              user_data);
gboolean                  capsule_process_wait_check_finish (CapsuleProcess       *self,
                                                             GAsyncResult         *result,
                                                             GError              **error);

G_END_DECLS
