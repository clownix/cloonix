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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <time.h>
#include "ioc.h"
#define MAX_RX_BUF 512 

/*****************************************************************************/
static void err_wake_out(void *ptr, int llid, int err, int from)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int rx_wake_out(void *ptr, int llid, int fd)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  char buffer[MAX_RX_BUF];
  int i, nb, len;
  len = read(fd, buffer, MAX_RX_BUF);
  for (i=0; i<len; i++)
    {
    nb = buffer[i];
    if (nb)
      {
      if (nb >= MAX_TYPE_CB)
        KOUT("%d", nb);
      if (all_ctx->g_cb[nb])
        all_ctx->g_cb[nb](all_ctx);
      }
    }
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void wake_out_epoll(t_all_ctx *all_ctx, uint8_t nb, t_wake_out_epoll cb)
{
  if (nb >= MAX_TYPE_CB)
    KOUT("%d", nb);
  all_ctx->g_cb[nb] = cb;
  if (write(all_ctx->g_out_evt_fd, &nb, sizeof(nb)) != 1)
    KOUT("%d", errno);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void init_wake_out_epoll(t_all_ctx *all_ctx)
{
  int fds[2];
  memset(all_ctx->g_cb, 0, MAX_TYPE_CB * sizeof(t_wake_out_epoll));
  if (pipe(fds))
    KOUT(" ");
  nonblock_fd(fds[0]);
  nonblock_fd(fds[1]);
  all_ctx->g_out_evt_fd = fds[1];
  if ((fds[0] < 0) || (fds[0] >= MAX_SELECT_CHANNELS-1))
    KOUT("%d", fds[0]);
  msg_watch_fd(all_ctx, fds[0], rx_wake_out, err_wake_out);
}
/*---------------------------------------------------------------------------*/

