/*****************************************************************************/
/*    Copyright (C) 2006-2021 clownix@clownix.net License AGPL-3             */
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
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdint.h>
#include <syslog.h>
#include <string.h>
#include <linux/if.h>

long long cloonix_get_msec(void);
long long cloonix_get_usec(void);
void cloonix_set_pid(int pid);
int cloonix_get_pid(void);

#ifndef KERR
#define KERR(format, a...)                               \
 do {                                                    \
    syslog(LOG_ERR | LOG_USER, "%s"                      \
    " line:%d " format "\n",                             \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__), \
     __LINE__, ## a);                                    \
    printf("%s line:%d " format "\n",                    \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__), \
     __LINE__, ## a);                                    \
    } while (0)
#endif

#ifndef KOUT
#define KOUT(format, a...)                               \
 do {                                                    \
    syslog(LOG_ERR | LOG_USER, "KILL %s"                 \
    " line:%d   " format "\n\n",                         \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__), \
     __LINE__, ## a);                                    \
    printf("KILL %s line:%d " format "\n",               \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__), \
     __LINE__, ## a);                                    \
    exit(-1);                                            \
    } while (0)
#endif

#define MAX_NAME_LEN 64
#define MAX_PATH_LEN 256
#define CLOWNIX_MAX_CHANNELS 10000
#define MAX_SELECT_CHANNELS 500

enum {
  endp_type_tap=13,
  endp_type_phy,
  endp_type_a2b,
  endp_type_nat,
  endp_type_kvm,
  endp_type_d2d,
};

typedef void (*t_fd_local_flow_ctrl)(int llid, int stop);
typedef void (*t_fd_dist_flow_ctrl)(int llid, char *name, int num,
                                    int tidx, int rank, int stop);
typedef void (*t_fd_error)(int llid, int err, int from);
typedef int  (*t_fd_event)(int llid, int fd);
typedef void (*t_fd_connect)(int llid, int llid_new);
#include "glob_common.h"
#include "rpct.h"
/****************************************************************************/

