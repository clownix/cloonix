/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#ifndef __FreeBSD__
#include <linux/reboot.h>
#endif

#include "glob_common.h"
#include "commun.h"
#include "sock.h"
#include "nonblock.h"

#define MAX_FD_NUM 150
#define CLOWNIX_MAX_CHANNELS 10000
#define MAX_PIPE_NUM 30
/* Queued for the exec server: past PIPE_TX_HIGH the host is told xoff,
   under PIPE_TX_LOW xon; past PIPE_TX_CAP the pipe is closed. */
#define PIPE_TX_HIGH (1024 * 1024)
#define PIPE_TX_LOW  (256 * 1024)
#define PIPE_TX_CAP  (32 * 1024 * 1024)

static t_pipe_ctx *g_ctx[CLOWNIX_MAX_CHANNELS];
static t_pipe_ctx *g_fd_to_ctx[MAX_FD_NUM];
static t_pipe_ctx *g_head_ctx;
static int g_pipe_ctx_num;
static int g_reboot_countdown;
static int g_halt_countdown;

#ifdef __FreeBSD__
#define REBOOT_RESTART   RB_AUTOBOOT
#define REBOOT_POWER_OFF (RB_POWEROFF | RB_HALT)
#else
#define REBOOT_RESTART   LINUX_REBOOT_CMD_RESTART
#define REBOOT_POWER_OFF LINUX_REBOOT_CMD_POWER_OFF
#endif

static void free_ctx(int dido_llid);

static void tell_host_flow(t_pipe_ctx *dctx, int val)
{
  char buf[MAX_ASCII_LEN];
  int headsize = sock_header_get_size();
  char *txt = (val == header_val_xoff) ? "xoff" : "xon";
  strcpy(buf + headsize, txt);
  send_to_virtio(dctx->dido_llid, 0, 0, header_type_ctrl_agent, val,
                 strlen(txt) + 1, buf);
}

static void send_to_pipe(t_pipe_ctx *dctx, char *buf, int len)
{
  int q = nonblock_queued(dctx->fd);
  if (q + len > PIPE_TX_CAP)
    {
    KERR("WARNING byte pipe %d: %d bytes wait for the exec server, past the "
         "cap of %d, the pipe is closed", dctx->dido_llid, q, PIPE_TX_CAP);
    free_ctx(dctx->dido_llid);
    return;
    }
  nonblock_send(dctx->fd, buf, len);
  if ((!dctx->told_xoff) && (q + len > PIPE_TX_HIGH))
    {
    tell_host_flow(dctx, header_val_xoff);
    dctx->told_xoff = 1;
    }
}

static int receive_from_pipe(int fd, char **msgrx)
{
  static char rx[MAX_A2D_LEN];
  int  headsize = sock_header_get_size();
  int nb_repeat, len, count = MAX_A2D_LEN - headsize;
  *msgrx = rx;
  nb_repeat = 0;
  len = read(fd, rx, count);
  while (len == -1)
    {
    nb_repeat += 1;
    /* the exec server closed with data unread: the end of a session */
    if (errno == ECONNRESET)
      return 0;
    if ((errno != EINTR) && (errno != EAGAIN))
      {
      KERR("ERROR errno %d", errno);
      break;
      }
    if (nb_repeat > 10)
      {
      KERR("ERROR REPEAT");
      break;
      }
    usleep(1000);
    len = read(fd, rx, count);
    }
  return len;
}

static void call_poweroff(void)
{
  system("echo \"SHUTDOWN IN 1 SEC by cloonix-agent\" | wall");
  g_halt_countdown = 2;
}

static void call_reboot(void)
{
  system("echo \"REBOOT IN 1 SEC by cloonix-agent\" | wall");
  g_reboot_countdown = 2;
}

static void alloc_ctx(int dido_llid, int fd)
{
  t_pipe_ctx *dctx;
  if (g_fd_to_ctx[fd])
    KOUT("ERROR ");
  dctx = (t_pipe_ctx *) malloc(sizeof(t_pipe_ctx));
  memset(dctx, 0, sizeof(t_pipe_ctx));
  dctx->dido_llid = dido_llid;
  dctx->fd = fd;
  nonblock_add_fd(dctx->fd);
  g_ctx[dido_llid] = dctx;
  g_fd_to_ctx[fd] = dctx;
  if (g_head_ctx)
    g_head_ctx->prev = dctx;
  dctx->next = g_head_ctx;
  g_head_ctx = dctx;
  g_pipe_ctx_num += 1;
}

static t_pipe_ctx *get_ctx(int dido_llid)
{
  if ((dido_llid <= 0) || (dido_llid >= CLOWNIX_MAX_CHANNELS))
    return NULL;
  return (g_ctx[dido_llid]);
}

static void free_ctx(int dido_llid)
{
  char buf[MAX_A2D_LEN];
  int headsize = sock_header_get_size();
  char *payload = buf + headsize;
  t_pipe_ctx *dctx = get_ctx(dido_llid);
  if (dctx)
    {
    memcpy(payload, LABREAK, strlen(LABREAK) + 1);
    send_to_virtio(dido_llid, 0, 0,
                   header_type_ctrl_agent, header_val_del_dido_llid,
                   strlen(LABREAK) + 1, buf);
    nonblock_del_fd(dctx->fd);
    close(dctx->fd);
    if (!g_fd_to_ctx[dctx->fd])
      KOUT("ERROR ");
    g_fd_to_ctx[dctx->fd] = NULL;
    g_ctx[dido_llid] = NULL;
    if (dctx->next)
      dctx->next->prev = dctx->prev;
    if (dctx->prev)
      dctx->prev->next = dctx->next;
    if (dctx == g_head_ctx)
      g_head_ctx = dctx->next;
    free(dctx);
    g_pipe_ctx_num -= 1;
    if (g_pipe_ctx_num < 0)
      KOUT("ERROR ");
    }
}

static void action_rx_pipe(t_pipe_ctx *dctx, int len, char *rx)
{
  char buf[MAX_A2D_LEN];
  int headsize = sock_header_get_size();
  char *payload = buf + headsize;
  memcpy(payload, rx, len);
  send_to_virtio(dctx->dido_llid, 0, 0, header_type_traffic,
                 header_val_none, len, buf);
}

void action_events(fd_set *infd)
{
  int len;
  char *rx;
  t_pipe_ctx *tst_dctx, *next, *dctx = g_head_ctx;
  while (dctx)
    {
    next = dctx->next;
    if (FD_ISSET(dctx->fd, infd))
      {
      tst_dctx = g_fd_to_ctx[dctx->fd];
      if (tst_dctx != dctx)
        KOUT("ERROR%p %p", tst_dctx, dctx);
      len = receive_from_pipe(dctx->fd, &rx);
      if (len > 0)
        {
        action_rx_pipe(dctx, len, rx);
        }
      else
        {
        if (len != 0)
          KERR("ERROR NEG READ DIDO LLID ACTION %d", dctx->dido_llid);
        free_ctx(dctx->dido_llid);
        }
      }
    dctx = next;
    }
}

/* A pipe is read while the host takes its bytes: not past the mark of the
   virtio port, not after an xoff of the host. */
void action_prepare_fd_set(fd_set *infd, fd_set *outfd, int virtio_full)
{
  t_pipe_ctx *dctx = g_head_ctx;
  while (dctx)
    {
    if ((dctx->told_xoff) && (nonblock_queued(dctx->fd) < PIPE_TX_LOW))
      {
      tell_host_flow(dctx, header_val_xon);
      dctx->told_xoff = 0;
      }
    if ((!virtio_full) && (!dctx->host_xoff))
      FD_SET(dctx->fd, infd);
    nonblock_prepare_fd_set(dctx->fd, outfd);
    dctx = dctx->next;
    } 
}

int action_get_biggest_fd(void)
{
  int result = 0;
  t_pipe_ctx *dctx = g_head_ctx;
  while (dctx)
    {
    if (dctx->fd > result)
      result = dctx->fd;
    dctx = dctx->next;
    }
  return result;
}

static void helper_rx_virtio_nodido_llid(int type, int val, int len, char *rx)
{
  char buf[MAX_A2D_LEN];
  int headsize = sock_header_get_size();
  char *payload = buf + headsize;
  int val_id;
  memcpy(payload, rx, len);
  if (type == header_type_ctrl)
    {
    switch (val)
      {
      case header_val_ping:
        if (sscanf(rx, LAPING, &val_id) == 1)
          {
          send_to_virtio(0, 0, 0, header_type_ctrl,
                         header_val_ping, len, buf);
          }
        else
          KERR("ERROR %s", rx);
        break;
      case header_val_reboot:
        if (sscanf(rx, LABOOT, &val_id) == 1)
          {
          call_reboot();
          }
        else
          KERR("ERROR %s", rx);
        break;
      case header_val_halt:
        if (sscanf(rx, LAHALT, &val_id) == 1)
          {
          call_poweroff();
          }
        else
          KERR("ERROR %s", rx);
        break;

      default:
        KERR("ERROR %d", val);
        break;
      }
    }
  else if (type == header_type_wifi_ctrl)
    wifi_rx_host_ctrl(val, len, rx);
  else if (type == header_type_wifi_frame)
    wifi_rx_host_frame(len, rx);
  else
    KERR("ERROR %d %d", type, val);
}

static void helper_rx_virtio_noctx(int dido_llid, int type, int val, char *rx)
{
  char buf[MAX_A2D_LEN];
  int headsize = sock_header_get_size();
  char *payload = buf + headsize;
  int fd;
  char cookie[MAX_ASCII_LEN];
  if ((type == header_type_ctrl_agent) && (val == header_val_add_dido_llid))
    {
    /* a refusal is the answer that is not DBSSH_SERV_DOORS_RESP: the host
       writes its WARNING with the reason */
    if ((dido_llid <= 0) || (dido_llid >= CLOWNIX_MAX_CHANNELS))
      KERR("WARNING a byte pipe numbered %d, out of range", dido_llid);
    else if (g_pipe_ctx_num >= MAX_PIPE_NUM)
      {
      KERR("WARNING byte pipe %d refused: %d open already, the most",
           dido_llid, MAX_PIPE_NUM);
      snprintf(payload, MAX_ASCII_LEN-1, "REFUSED %d byte pipes open "
               "already, the most", MAX_PIPE_NUM);
      send_to_virtio(dido_llid, 0, 0, header_type_ctrl_agent,
                     header_val_add_dido_llid, strlen(payload) + 1, buf);
      }
    else if (sscanf(rx, "DBSSH_SERV_DOORS_REQ cookie=%199s", cookie) != 1)
      KERR("WARNING a byte pipe asked with %.100s", rx);
    else
      {
      fd = sock_client_unix(UNIX_CXP_SOCK);
      if ((fd <= 0) || (fd >= MAX_FD_NUM))
        {
        if (fd > 0)
          close(fd);
        KERR("WARNING byte pipe %d refused: no exec server on %s (fd %d)",
             dido_llid, UNIX_CXP_SOCK, fd);
        snprintf(payload, MAX_ASCII_LEN-1, "REFUSED no exec server on %s "
                 "(fd %d)", UNIX_CXP_SOCK, fd);
        send_to_virtio(dido_llid, 0, 0, header_type_ctrl_agent,
                       header_val_add_dido_llid, strlen(payload) + 1, buf);
        }
      else 
        {
        alloc_ctx(dido_llid, fd);
        snprintf(payload, MAX_ASCII_LEN-1, DBSSH_SERV_DOORS_RESP); 
        send_to_virtio(dido_llid, 0, 0, header_type_ctrl_agent,
                       header_val_add_dido_llid, strlen(payload) + 1, buf);
        }
      }
    }
  else if ((type==header_type_ctrl_agent) && (val==header_val_del_dido_llid))
    {
    }
  else if ((type==header_type_ctrl_agent) && (val==header_val_ack))
    {
    }
}

void action_rx_virtio(int dido_llid, int vwifi_base, int vwifi_cid,
                      int type, int val, int len, char *rx)
{
  t_pipe_ctx *dctx;
  if (!dido_llid) 
    helper_rx_virtio_nodido_llid(type, val, len, rx);
  else
    {
    dctx = get_ctx(dido_llid);
    if (!dctx)
      helper_rx_virtio_noctx(dido_llid, type, val, rx);
    else
      {
      if (type == header_type_traffic)
        {
        if (val == header_val_none)
          send_to_pipe(dctx, rx, len);
        else
          KERR("ERROR %d %d", type, val);
        }
      else if (type == header_type_ctrl_agent)
        {
        if (val == header_val_del_dido_llid)
          free_ctx(dido_llid);
        else if (val == header_val_xoff)
          dctx->host_xoff = 1;
        else if (val == header_val_xon)
          dctx->host_xoff = 0;
        else
          KERR("ERROR %d %d", type, val);
        }
      else if (type == header_type_ctrl)
        {
        KERR("ERROR %d %d", type, val);
        }
      else
        KERR("ERROR %d %d", type, val);
      }
    }
}

void action_heartbeat(int cur_sec)
{
  if (g_reboot_countdown)
    {
    g_reboot_countdown -= 1;
    if (g_reboot_countdown == 0)
      {
      reboot(REBOOT_RESTART);
      }
    }
  if (g_halt_countdown)
    {
    g_halt_countdown -= 1;
    if (g_halt_countdown == 0)
      {
      reboot(REBOOT_POWER_OFF);
      }
    }
}

void action_init(void)
{
  g_reboot_countdown = 0;
  g_halt_countdown = 0;
  g_pipe_ctx_num = 0;
  g_head_ctx = NULL;
  memset(g_fd_to_ctx, 0, MAX_FD_NUM * sizeof(t_pipe_ctx *));
  memset(g_ctx, 0, CLOWNIX_MAX_CHANNELS * sizeof(t_pipe_ctx *));
}

