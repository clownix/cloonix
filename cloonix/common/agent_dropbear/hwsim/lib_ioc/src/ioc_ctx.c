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
#include <stdint.h>
#include <sys/epoll.h>
#include <time.h>
#include "ioc.h"
#include "ioc_ctx.h"

 /*****************************************************************************/
t_all_ctx *ioc_ctx_init(char *name)
{
  t_all_ctx *all_ctx;
  t_ioc_ctx *ioc_ctx;
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts))
    KOUT(" ");
  all_ctx = (t_all_ctx *) malloc(sizeof(t_all_ctx));
  memset(all_ctx, 0, sizeof(t_all_ctx));
  all_ctx->ctx_head.ioc_ctx = (t_ioc_ctx *) malloc(sizeof(t_ioc_ctx));
  memset(all_ctx->ctx_head.ioc_ctx, 0, sizeof(t_ioc_ctx)); 
  ioc_ctx = (t_ioc_ctx *) all_ctx->ctx_head.ioc_ctx;
  ioc_ctx->g_first_rx_buf_max = IO_MAX_BUF_LEN;
  return all_ctx;
}
/*---------------------------------------------------------------------------*/
