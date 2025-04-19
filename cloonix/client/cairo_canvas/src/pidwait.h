/*****************************************************************************/
/*    Copyright (C) 2006-2025 clownix@clownix.net License AGPL-3             */
/*                                                                           */
/*  This program is free software: you can redistribute it and/or modify     */
/*  it under the terms of the GNU Affero General Public License as           */
/*  published by the Free Software Foundation, either version 3 of the       */
/*  License, or (at your option) any later version.                          */
/*                                                                           */
/*  This program is distributed in the hope that it will be useful,          */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of           */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            */
/*  GNU Affero General Public License for more details.a                     */
/*                                                                           */
/*  You should have received a copy of the GNU Affero General Public License */
/*  along with this program.  If not, see <http://www.gnu.org/licenses/>.    */
/*                                                                           */
/*****************************************************************************/
enum {
  type_pid_none=0,
  type_pid_spicy_gtk,
  type_pid_wireshark,
  type_pid_xephyr_frame,
  type_pid_xephyr_session,
  type_pid_dtach,
  type_pid_crun_screen,
};
typedef struct t_pid_wait
{
  int  type;
  int  sync_wireshark;
  int  count_sync;
  int  count;
  char name[MAX_NAME_LEN];
  char ident[MAX_PATH_LEN];
  char ascii_type[MAX_NAME_LEN];
  int  num;
  int  pid;
  int  release_previous_done;
  int  release_forbiden1;
  int  release_forbiden2;
  int  release_requested;
  int  missed_run;
  int  display;
  char ascii_display[MAX_NAME_LEN];
  char **argv;
  struct t_pid_wait *prev;
  struct t_pid_wait *next;
} t_pid_wait;
/*--------------------------------------------------------------------------*/
void pidwait_kill_all_active(void);
t_pid_wait *pidwait_find(int type, char *name, int num);
void pidwait_alloc(int type, char *name, int num, char *ident, char **argv);
void pidwait_init(void);
/*--------------------------------------------------------------------------*/
