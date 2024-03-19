/*****************************************************************************/
/*    Copyright (C) 2006-2024 clownix@clownix.net License AGPL-3             */
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
typedef struct t_llid_tid
{
  int llid;
  int tid;
} t_llid_tid;

typedef struct t_lst_pid
{
  char name[MAX_NAME_LEN];
  int pid;
  int unusedalign;
} t_lst_pid;
/*--------------------------------------------------------------------------*/

#define STAT_FORMAT "%*d (%s %*s %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u "\
                    "%lu %lu %lu %lu %*d %*d %*u %*u %*d %*u %lu"

/****************************************************************************/
typedef struct t_pid_info
{
        unsigned long      utime    ;
        unsigned long      cutime   ;
        unsigned long      stime    ;
        unsigned long      cstime   ;
        unsigned long      rss      ;
        char               comm[MAX_PATH_LEN];
} t_pid_info;
/*--------------------------------------------------------------------------*/

enum
  {
  auto_idle = 0,
  auto_create_disk,
  auto_delay_possible_ovs_start,
  auto_create_vm_launch,
  auto_create_vm_connect,
  auto_max,
  };

#define DIR_CONF "config"
#define FILE_COW "cow"
#define CLOONIX_FILE_NAME "name"
#define FILE_IMAGE "image.bin"
#define CLOONIX_INTERNAL_COM "cloonix_internal_com"
#define DIR_CLOONIX_DISKS "disks"
#define QMONITOR_UNIX "mon"
#define QMP_UNIX "qmp"
#define QGA_UNIX "qga"
#define QBACKDOOR_UNIX "qdagent"
#define QBACKDOOR_HVCO_UNIX "qdhvc"





