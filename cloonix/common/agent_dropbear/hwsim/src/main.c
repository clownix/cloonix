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
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "ioc.h"
#include "hwsim.h"
#include "iface.h"
#include "blkd_addr.h"
#include "main.h"


static t_all_ctx *g_all_ctx;

static int g_fd_hwsim;


/****************************************************************************/
static int rx_from_fd_hwsim(void *ptr, int llid, int fd)
{
  hwsim_nl_sock_event();
  return 0;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_err_fd_hwsim(void *ptr, int llid, int err, int from)
{
  KERR("Closed link to mac80211_hwsim");
  g_fd_hwsim = -1;
}
/*-------------------------------------------------------------------------*/

/*****************************************************************************/
static void fast_timeout_heartbeat(t_all_ctx *all_ctx, void *data)
{
  hwsim_heartbeat();
  clownix_timeout_add(all_ctx, 1, fast_timeout_heartbeat, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timeout_heartbeat(t_all_ctx *all_ctx, void *data)
{
  int fd;
  blkd_addr_heartbeat();
  if (g_fd_hwsim == -1)
    {
    fd = hwsim_init();
    if (fd < 0)
      KERR("Bad nl link to mac80211_hwsim");
    else
      {
      if (!msg_watch_fd(g_all_ctx, fd, rx_from_fd_hwsim, rx_err_fd_hwsim))
        KOUT("Bad nl link to mac80211_hwsim");
      g_fd_hwsim = fd;
      }
    }
  clownix_timeout_add(all_ctx, 100, timeout_heartbeat, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void tx_flow_ctrl(void *ptr, int llid, int stop)
{
  KERR("%d %d", llid, stop);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void rx_flow_ctrl(void *ptr, int llid, int stop)
{
  KERR("%d %d", llid, stop);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void peer_flow_control(void *ptr, int llid,
                              char *name, int num, int tidx,
                              int rank, int stop)
{
  KERR("cloonix_evt_peer_flow_control name=%s num=%d tidx=%d rank=%d stop=%d",
  name, num, tidx, rank, stop);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int blkd_channel_create(void *ptr, int fd,
                        t_fd_event rx,
                        t_fd_event tx,
                        t_fd_error err,
                        char *from)
{
  if ((fd < 0) || (fd >= MAX_SIM_CHANNELS-1))
    KOUT("%d", fd);
  return channel_create((t_all_ctx *)ptr, 1, fd, rx, tx, err, from);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int tst_nb_idx(char *str, int *nb_idx)
{
  int result = 0;
  unsigned long val;
  char *endptr;
  val = strtoul(str, &endptr, 10);
  if ((endptr == NULL)||(endptr[0] != 0))
    result = -1;
  if ((val < 1) || (val > MAX_NB_IDX))
    {
    KERR("nb wlan between 1 and 8 val=%lu argv=%s", val, str);
    result = -1;
    }
  *nb_idx = (int) val;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_all_ctx *get_all_ctx(void)
{
  return (g_all_ctx);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int main_fd_hwsim_ok(void)
{
  return (g_fd_hwsim != -1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int main(int argc, char *argv[])
{ 
  int nb_idx;
  if (argc != 2)
    KOUT(" ");
  if (tst_nb_idx(argv[1], &nb_idx))
    KOUT(" ");
  daemon(0,0);
  g_fd_hwsim = -1;
  g_all_ctx = msg_mngt_init("hwsim");
  blkd_init((void *)g_all_ctx,tx_flow_ctrl,rx_flow_ctrl,peer_flow_control);
  blkd_set_our_mutype((void *) g_all_ctx, endp_type_hsim);
  iface_init(nb_idx);
  if (blkd_addr_init(nb_idx))
    KOUT("Problem init %d", nb_idx);
  clownix_timeout_add(g_all_ctx, 100, timeout_heartbeat, NULL, NULL, NULL);
  clownix_timeout_add(g_all_ctx, 100, fast_timeout_heartbeat, NULL, NULL, NULL);
  msg_mngt_loop(g_all_ctx);
  return 0;
}
/*--------------------------------------------------------------------------*/

