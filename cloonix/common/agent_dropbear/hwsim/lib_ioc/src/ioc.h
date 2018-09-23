/****************************************************************************/
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <sys/types.h>
#include <asm/types.h>
#include <sys/time.h>

#include "ioc_blkd.h"


#define MAX_CHANNELS 1000
#define MAX_SIM_CHANNELS 100
#define IO_MAX_BUF_LEN     20000
/*--------------------------------------------------------------------------*/
typedef void (*t_fd_error)(void *ptr, int llid, int err, int from);
typedef int  (*t_fd_event)(void *ptr, int llid, int fd);
/*--------------------------------------------------------------------------*/
typedef struct t_all_ctx
{
  t_all_ctx_head ctx_head;
} t_all_ctx;
/*---------------------------------------------------------------------------*/

/****************************************************************************/
typedef void (*t_fct_timeout)(t_all_ctx *all_ctx, void *data);
void clownix_timeout_add(t_all_ctx *all_ctx, int nb_beats, t_fct_timeout cb, 
                         void *data, long long *abs_beat, int *ref);
int clownix_timeout_exists(t_all_ctx *all_ctx, long long abs_beat, int ref);
void *clownix_timeout_del(t_all_ctx *all_ctx, long long abs_beat, int ref,
                          const char *file, int line);
void clownix_timeout_del_all(t_all_ctx *all_ctx);
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int channel_create(t_all_ctx *all_ctx, int is_blkd, int fd, t_fd_event rx_cb,
                   t_fd_event tx_cb, t_fd_error err_cb, char *from);
/*---------------------------------------------------------------------------*/

/****************************************************************************/
typedef void (*t_heartbeat_cb)(t_all_ctx *all_ctx, int delta);
/*---------------------------------------------------------------------------*/

/****************************************************************************/
t_all_ctx *msg_mngt_init (char *name);
int msg_exist_channel(t_all_ctx *all_ctx, int llid, const char *fct);
void msg_delete_channel(t_all_ctx *all_ctx, int llid);
void msg_mngt_heartbeat_init (t_all_ctx *all_ctx, t_heartbeat_cb heartbeat);
void msg_mngt_loop(t_all_ctx *all_ctx);
int msg_watch_fd(t_all_ctx *all_ctx,int fd,t_fd_event rx_data,t_fd_error err);
/*****************************************************************************/

