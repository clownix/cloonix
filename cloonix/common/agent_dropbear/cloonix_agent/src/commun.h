/*****************************************************************************/
/*    Copyright (C) 2006-2018 cloonix@cloonix.net License AGPL-3             */
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
#include <syslog.h>
#undef KERR
#define KERR(format, a...)                              \
 do {                                                   \
    syslog(LOG_ERR, "%s %s line:%d   " format, \
    __FILE__,__FUNCTION__,__LINE__, ## a);              \
    } while (0)

#undef KOUT
#define KOUT(format, a...)                               \
 do {                                                    \
    syslog(LOG_ERR, "%s %s line:%d   " format, \
    __FILE__,__FUNCTION__,__LINE__, ## a);               \
    exit(-1);                                            \
    } while (0)
/*--------------------------------------------------------------------------*/

#define MAX_ASCII_LEN 200


#define VIRTIOPORT "/dev/virtio-ports/net.cloonix.0"
#define UNIX_DROPBEAR_SOCK "/tmp/unix_cloonix_dropbear_sock"
#define UNIX_X11_SOCKET_DIR "/tmp/.X11-unix"
#define UNIX_X11_SOCKET_PREFIX "/tmp/.X11-unix/X"

void send_ack_to_virtio(int dido_llid, 
                        unsigned long long s2c, 
                        unsigned long long c2s);
void send_to_virtio(int dido_llid, int len, int type, int val, char *msg);

void action_rx_virtio(int dido_llid, int len, int type, int val, char *rx);
void action_heartbeat(int cur_sec);
void action_init(void);

void action_prepare_fd_set(fd_set *infd, fd_set *outfd);
void action_events(fd_set *infd);
int  action_get_biggest_fd(void);





