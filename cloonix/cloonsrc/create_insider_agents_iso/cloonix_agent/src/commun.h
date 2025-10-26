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
#define MAX_ASCII_LEN 200
#define VIRTIOPORT "/dev/virtio-ports/net.cloon.0"
#define UNIX_DROPBEAR_SOCK "/cloonixmnt/tmp/unix_cloonix_dropbear_sock"
#define UNIX_VWIFI_CLI "/cloonixmnt/tmp/unix_vwifi_client"
#define UNIX_VWIFI_SPY "/cloonixmnt/tmp/unix_vwifi_spy"
#define UNIX_VWIFI_CTR "/cloonixmnt/tmp/unix_vwifi_ctrl"

typedef struct t_dropbear_ctx
{
  int dido_llid;
  int fd;
  int is_allocated_x11_display_idx;
  int display_sock_x11;
  int fd_listen_x11;
  struct t_dropbear_ctx *prev;
  struct t_dropbear_ctx *next;
} t_dropbear_ctx;


void send_ack_to_virtio(int dido_llid, 
                        unsigned long long s2c, 
                        unsigned long long c2s);
void send_to_virtio(int dido_llid, int vwifi_base, int vwifi_cid,
                    int type, int val, int len, char *msg);

void action_rx_virtio(int dido_llid, int vwifi_base, int vwifi_cid,
                      int type, int val, int len, char *rx);
void action_heartbeat(int cur_sec);
void action_init(void);

void action_prepare_fd_set(fd_set *infd, fd_set *outfd);
void action_events(fd_set *infd);
int  action_get_biggest_fd(void);

/*---------------------------------------------------------------------------*/





