/****************************************************************************/
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
