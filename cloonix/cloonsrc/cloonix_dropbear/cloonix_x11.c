/****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <netinet/in.h>



#include "glob_common.h"
#include "io_clownix.h"
#include "doorways_sock.h"
#include "util_sock.h"
#include "cloonix_x11.h"


typedef struct t_x11_ctx
{
  int dido_llid;
  int sub_dido_idx;
  int x11_llid;
  int payload_rx_x11;
  int payload_tx_x11;
  struct t_x11_ctx *prev;
  struct t_x11_ctx *next;

} t_x11_ctx;


static t_x11_ctx *x11_llid_2_ctx[CLOWNIX_MAX_CHANNELS];
static t_x11_ctx *dido_llid_2_ctx[CLOWNIX_MAX_CHANNELS];


/*****************************************************************************/
static void fd_cloexec(int fd)
{
  int flags = fcntl(fd, F_GETFD);
  if (flags == -1)
    KOUT(" ");
  flags |= FD_CLOEXEC;
  if (fcntl(fd, F_SETFD, flags) == -1)
    KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void nonblock(int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
    KOUT(" ");
  flags |= O_NONBLOCK|O_NDELAY;
  if (fcntl(fd, F_SETFL, flags) == -1)
    KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int sock_cli_unix(char *pname)
{
  int fd, result = -1;
  if (!util_proxy_client_socket_unix(pname, &fd))
    result = fd;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int sock_cli_inet(int port)
{
  int   sock, res, result = -1;
  struct sockaddr_in adresse;
  sock = socket (AF_INET,SOCK_STREAM,0);
  if (sock <= 0)
    KOUT(" ");
  adresse.sin_family = AF_INET;
  adresse.sin_port = htons((short) port);
  adresse.sin_addr.s_addr = htonl(0x7F000001);
  res = connect(sock,(struct sockaddr *) &adresse, sizeof(adresse));
  if (res == 0)
    {
    result = sock;
    nonblock(sock);
    fd_cloexec(sock);
    }
  else
    close(sock);
  return result;
}
/*---------------------------------------------------------------------------*/



/****************************************************************************/
static void tx_x11_openokko_to_door(int dido_llid, int sub_dido_idx,
                                    const char *str)
{
  int len;
  char buf[MAX_DOOR_CTRL_LEN];
  memset(buf, 0, MAX_DOOR_CTRL_LEN);
  snprintf(buf, MAX_DOOR_CTRL_LEN-1, str);
  len = strlen(buf) + 1;
  send_ctrl_x11_to_doors(dido_llid, sub_dido_idx, len, buf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_x11_ctx *get_ctx_with_x11_llid(int x11_llid)
{
  t_x11_ctx *result;
  if ((x11_llid <= 0) || (x11_llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", x11_llid);
  result = x11_llid_2_ctx[x11_llid];
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static t_x11_ctx *get_head_ctx_with_dido_llid(int dido_llid)
{
  t_x11_ctx *result;
  if ((dido_llid <= 0) || (dido_llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", dido_llid);
  result = dido_llid_2_ctx[dido_llid];
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static t_x11_ctx *get_ctx_with_sub_dido_idx(int dido_llid, int sub_dido_idx)
{
  t_x11_ctx *cur = get_head_ctx_with_dido_llid(dido_llid);
  if ((sub_dido_idx < 1) || (sub_dido_idx > X11_DISPLAY_IDX_MAX))
    KOUT("%d %d", sub_dido_idx, X11_DISPLAY_IDX_MAX);
  while (cur && (cur->sub_dido_idx != sub_dido_idx))
    cur = cur->next;
  return cur;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void alloc_ctx(int dido_llid, int sub_dido_idx, int x11_llid)
{
  t_x11_ctx *cur = get_ctx_with_sub_dido_idx(dido_llid, sub_dido_idx);
  if (cur)
    KOUT("%d %d %d", dido_llid, sub_dido_idx, x11_llid);
  cur = get_ctx_with_x11_llid(x11_llid);
  if (cur)
    KOUT("%d %d %d", dido_llid, sub_dido_idx, x11_llid);
  cur = (t_x11_ctx *) clownix_malloc(sizeof(t_x11_ctx), 10);
  memset(cur, 0, sizeof(t_x11_ctx));
  cur->dido_llid = dido_llid;
  cur->sub_dido_idx = sub_dido_idx;
  cur->x11_llid = x11_llid;
  if (dido_llid_2_ctx[dido_llid])
    dido_llid_2_ctx[dido_llid]->prev = cur;
  cur->next = dido_llid_2_ctx[dido_llid];
  dido_llid_2_ctx[dido_llid] = cur;
  x11_llid_2_ctx[x11_llid] = cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_ctx(int x11_llid)
{
  t_x11_ctx *ctx, *cur = get_ctx_with_x11_llid(x11_llid);
  if (!cur)
    KOUT(" ");
  ctx = get_ctx_with_sub_dido_idx(cur->dido_llid, cur->sub_dido_idx);
  if (cur != ctx)
    KOUT("%p %p", cur, ctx);
  if (cur->x11_llid != x11_llid)
    KOUT("%d %d", cur->x11_llid, x11_llid);
  if (msg_exist_channel(cur->x11_llid))
    msg_delete_channel(cur->x11_llid);
  tx_x11_openokko_to_door(cur->dido_llid, cur->sub_dido_idx, DOOR_X11_OPENKO); 
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == dido_llid_2_ctx[cur->dido_llid])
    dido_llid_2_ctx[cur->dido_llid] = cur->next;
  x11_llid_2_ctx[cur->x11_llid] = NULL;
  clownix_free(cur, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void x11_err_cb (int x11_llid, int err, int from)
{
  t_x11_ctx *ctx = get_ctx_with_x11_llid(x11_llid);
  (void) from;
  if (ctx)
    {
    KERR("ERROR %d %d %d %d", ctx->dido_llid, ctx->sub_dido_idx, err, from);
    free_ctx(x11_llid);
    }
  else
    {
    KERR("ERROR %s %d %d", __FUNCTION__, err, from);
    if (msg_exist_channel(x11_llid))
      msg_delete_channel(x11_llid);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int x11_rx_cb(int x11_llid, int x11_fd)
{
  int len, result = 0;
  static char buf[MAX_A2D_LEN];
  int headsize = doorways_header_size();
  t_x11_ctx *cur = get_ctx_with_x11_llid(x11_llid);
  if (!cur)
    KERR("ERROR %d %d", x11_llid, x11_fd);
  else
    {
    len = util_read (buf, MAX_A2D_LEN - headsize, x11_fd);
    if (len < 0)
      {
      if ((errno != EINTR) && (errno != EAGAIN)) 
        {
        KERR("ERROR payload_tx_x11: errno:%d", errno);
        free_ctx(x11_llid);
        }
      }
    else
      {
      send_traf_x11_to_doors(cur->dido_llid, cur->sub_dido_idx, len, buf);
      result = len;
      cur->payload_tx_x11 += len;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int local_x11_open(int dido_llid, int sub_dido_idx,
                          int is_inet, int port, char *x11_path)
{
  int x11_fd, x11_llid, result = -1;
  t_x11_ctx *cur = get_ctx_with_sub_dido_idx(dido_llid, sub_dido_idx);
  if (cur)
    KERR("ERROR %d %d", dido_llid, sub_dido_idx);
  else
    {
    if (is_inet)
      x11_fd = sock_cli_inet(port);
    else
      x11_fd = sock_cli_unix(x11_path);
    if (x11_fd >= 0)
      {
      x11_llid = msg_watch_fd(x11_fd, x11_rx_cb, x11_err_cb, "x11");
      if (x11_llid == 0)
        KOUT(" ");
      alloc_ctx(dido_llid, sub_dido_idx, x11_llid);
      result = 0;
      }
    else 
      KERR("ERROR %d %d %d %d %s",
           dido_llid, sub_dido_idx, is_inet, port, x11_path);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void receive_traf_x11_from_doors(int dido_llid, int sub_dido_idx,
                                 int len, char *buf)
{
  t_x11_ctx *cur = get_ctx_with_sub_dido_idx(dido_llid, sub_dido_idx);
  if (!cur)
    KERR("ERROR %d %d %d", dido_llid, sub_dido_idx, cur->payload_rx_x11);
  else
    {
    watch_tx(cur->x11_llid, len, buf);
    cur->payload_rx_x11 += len;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int x11_path_is_tst(char *x11_path)
{
  int fd, result = -1;
  if (util_proxy_client_socket_unix(x11_path, &fd))
    KERR("ERROR X11 SOCKET NOT FOUND: %s", x11_path);
  else
    {
    close(fd);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/



/****************************************************************************/
static int get_x11_path(int port, int idx_x11, char *x11_path)
{
  int val, is_inet = 0;
  char *display;
  memset(x11_path, 0, MAX_PATH_LEN);
  if (port > 6000)
    is_inet = 1;
  else
    {
    display = getenv("DISPLAY");
    if (sscanf(display, ":%d", &val) == 1)
      {
      snprintf(x11_path, MAX_PATH_LEN-1, X11_DISPLAY_PREFIX, val);
      if (x11_path_is_tst(x11_path))
        {
        KERR("ERROR X11 socket not found: %s", x11_path);
        memset(x11_path, 0, MAX_PATH_LEN);
        }
      }
    else
      KERR("ERROR X11 display not good: %s", display);
    }
  return is_inet;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void receive_ctrl_x11_from_doors(int dido_llid, int sub_dido_idx,
                                 int len, char *buf)
{
  int idx_x11, port, is_inet;
  char x11_path[MAX_PATH_LEN];
  t_x11_ctx *cur;
  if (sscanf(buf, DOOR_X11_OPEN, &idx_x11, &port) == 2)
    {
    is_inet = get_x11_path(port, idx_x11, x11_path);
    if (!local_x11_open(dido_llid, sub_dido_idx, is_inet, port, x11_path))
      tx_x11_openokko_to_door(dido_llid, sub_dido_idx, DOOR_X11_OPENOK); 
    else
      {
      KERR("WARNING %d %d %d %s", dido_llid, sub_dido_idx, len, buf);
      tx_x11_openokko_to_door(dido_llid, sub_dido_idx, DOOR_X11_OPENKO); 
      }
    }
  else if (!strcmp(buf, DOOR_X11_OPENKO))
    {
    cur = get_ctx_with_sub_dido_idx(dido_llid, sub_dido_idx);
    if (cur)
      {
      free_ctx(cur->x11_llid);
      }
    }
  else
    KOUT("ERROR %d %d %d %s", dido_llid, sub_dido_idx, len, buf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_init(void)
{
  memset(x11_llid_2_ctx, 0,  CLOWNIX_MAX_CHANNELS * sizeof(t_x11_ctx *));
  memset(dido_llid_2_ctx, 0, CLOWNIX_MAX_CHANNELS * sizeof(t_x11_ctx *));
}
/*--------------------------------------------------------------------------*/

