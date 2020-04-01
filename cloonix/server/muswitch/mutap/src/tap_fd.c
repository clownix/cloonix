/*****************************************************************************/
/*    Copyright (C) 2006-2020 clownix@clownix.net License AGPL-3             */
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <signal.h>

#include "ioc.h"
#include "tun_tap.h"
#include "sock_fd.h"

/*---------------------------------------------------------------------------*/
static int g_fd_tap;
static int g_llid_tap;
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int rx_from_tap(void *ptr, int llid, int fd)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  int len, tap_type = blkd_get_our_mutype(ptr);
  t_blkd *bd;
  char *data;
  
  bd = blkd_create_tx_empty(0,0,0);
  data = bd->payload_blkd;
  len = read (fd, data, PAYLOAD_BLKD_SIZE);
  while(1)
    {
    if (len == 0)
      KOUT(" ");
    else if (len < 0)
      {
      if ((errno != EAGAIN) && (errno != EINTR))
        KOUT("rx_from_tap ERROR %d %d", len, errno);
      len = 0;
      blkd_free(ptr, bd);
      break;
      }
    else
      {
      bd->payload_len = len;
      if (tap_type == endp_type_tap)
        sock_fd_tx(all_ctx, bd);
      else
        KOUT(" ");
      bd = blkd_create_tx_empty(0,0,0);
      data = bd->payload_blkd;
      len = read (fd, data, PAYLOAD_BLKD_SIZE);
      }
    }
  return len;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void err_cb_tap (void *ptr, int llid, int err, int from)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void tap_fd_tx(t_all_ctx *all_ctx, t_blkd *blkd)
{
  int len, fd;
  fd = get_fd_with_llid(all_ctx, g_llid_tap);
  if (fd < 0)
    KOUT(" ");
  else if (g_fd_tap != fd)
    KOUT("%d %d", g_fd_tap, fd);
  else
    {
    len = write (fd, blkd->payload_blkd, blkd->payload_len);
    if(blkd->payload_len != len)
      KERR("%d %d", blkd->payload_len, len);
    }
  blkd_free((void *) all_ctx, blkd);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int tap_fd_open(t_all_ctx *all_ctx, char *tap_name)
{
  int result = -1;
  if (tun_tap_open(all_ctx, tap_name, &g_fd_tap))
    KERR("TAP OPEN ERROR %s", tap_name);
  else
    {
    if ((g_fd_tap < 0) || (g_fd_tap >= MAX_SELECT_CHANNELS-1))
      KOUT("%d", g_fd_tap);

    g_llid_tap = msg_watch_fd(all_ctx, g_fd_tap, rx_from_tap, err_cb_tap);
    if (g_llid_tap <= 0)
      KERR("TAP FD ERROR %s", tap_name);
    else
      result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void tap_fd_init(t_all_ctx *all_ctx)
{
  g_fd_tap    = -1;
  g_llid_tap  = 0;
}
/*---------------------------------------------------------------------------*/
