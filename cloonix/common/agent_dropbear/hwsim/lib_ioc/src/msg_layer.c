/****************************************************************************/
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>


#include "ioc.h"
#include "ioc_ctx.h"
#include "channel.h"
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int msg_watch_fd(t_all_ctx *all_ctx, int fd,
                 t_fd_event rx_data, t_fd_error err)
{
  int llid;
  if (fd < 0)
    KERR(" ");
  else if (!err)
    KERR(" ");
  else if ((fd < 0) || (fd >= MAX_SIM_CHANNELS-1))
    KERR("%d %d", fd, MAX_SIM_CHANNELS);
  else
    {
    llid = channel_create(all_ctx, 0, fd, rx_data,
                          NULL, err, (char *) __FUNCTION__);
    if (!llid)
      KERR(" ");
    }
  return (llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void msg_delete_channel(t_all_ctx *all_ctx, int llid)
{
  int is_blkd = channel_delete(all_ctx, llid);
  if (is_blkd)
    {
    if (blkd_delete((void *) all_ctx, llid))
      KERR(" ");
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void msg_mngt_heartbeat_init(t_all_ctx *all_ctx, t_heartbeat_cb heartbeat)
{
  channel_beat_set (all_ctx, heartbeat);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void msg_mngt_loop(t_all_ctx *all_ctx)
{
  channel_loop(all_ctx);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void signal_pipe(int no_use)
{
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
t_all_ctx *msg_mngt_init (char *name)
{
  t_all_ctx *all_ctx = ioc_ctx_init(name);
  struct sigaction act;
  memset(&act, 0, sizeof(act));
  sigfillset(&act.sa_mask);
  act.sa_flags = 0;
  act.sa_handler = signal_pipe;
  sigaction(SIGPIPE, &act, NULL);
  channel_init(all_ctx);
  return (all_ctx);
} 
/*---------------------------------------------------------------------------*/

