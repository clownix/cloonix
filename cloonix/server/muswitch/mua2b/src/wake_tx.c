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
#include "sched.h"
#define MAX_RX_BUF 512 

static int g_0_to_1_fd[2];
static int g_1_to_0_fd[2];

/*****************************************************************************/
static void err_wake_tx(void *ptr, int llid, int err, int from)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int rx_wake_tx0(void *ptr, int llid, int fd)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  char buffer[MAX_RX_BUF];
  read(fd, buffer, MAX_RX_BUF);
  sched_tx0_activate(all_ctx);
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int rx_wake_tx1(void *ptr, int llid, int fd)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  char buffer[MAX_RX_BUF];
  read(fd, buffer, MAX_RX_BUF);
  sched_tx1_activate(all_ctx);
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void wake_tx_send(t_all_ctx *all_ctx, int num)
{
  uint8_t nb = 1;
  if (num == 0)
    {
    if (write(g_1_to_0_fd[1], &nb, sizeof(nb)) != 1)
      KOUT("%d", errno);
    }
  else if (num == 1)
    {
    if (write(g_0_to_1_fd[1], &nb, sizeof(nb)) != 1)
      KOUT("%d", errno);
    }
  else
    KOUT("%d", num);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void wake_tx_watch_fd(t_all_ctx *all_ctx, int num)
{
  if (num == 0)
    msg_watch_fd(all_ctx, g_1_to_0_fd[0], rx_wake_tx0, err_wake_tx);
  else if (num == 1)
    msg_watch_fd(all_ctx, g_0_to_1_fd[0], rx_wake_tx1, err_wake_tx);
  else
    KOUT("%d", num);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void wake_tx_init(void)
{
  if (pipe(g_0_to_1_fd))
    KOUT(" ");
  if (pipe(g_1_to_0_fd))
    KOUT(" ");
  nonblock_fd(g_0_to_1_fd[0]);
  nonblock_fd(g_0_to_1_fd[1]);
  nonblock_fd(g_1_to_0_fd[0]);
  nonblock_fd(g_1_to_0_fd[1]);
}
/*---------------------------------------------------------------------------*/
