/*****************************************************************************/
/*    Copyright (C) 2006-2025 clownix@clownix.net License AGPL-3             */
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
#include <unistd.h>
#include <linux/reboot.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sysinfo.h>
#include <mntent.h>
#include <sys/ioctl.h>
#include <linux/fs.h>



#include "glob_common.h"
#include "commun.h"
#include "sock.h"
#include "x11_channels.h"
#include "nonblock.h"
#include "vwifi.h"


#define MAX_FD_NUM 150
#define CLOWNIX_MAX_CHANNELS 10000
#define MAX_SSH_NUM 30

void vwifi_start_ok(void);

static t_dropbear_ctx *g_ctx[CLOWNIX_MAX_CHANNELS];
static t_dropbear_ctx *g_fd_to_ctx[MAX_FD_NUM];
static t_dropbear_ctx *g_head_ctx;
static int g_dropbear_ctx_num;
static int g_reboot_countdown;
static int g_halt_countdown;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void send_to_dropbear(int fd, char *buf, int len)
{
  nonblock_send(fd, buf, len);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int receive_from_dropbear(int fd, char **msgrx)
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
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void call_poweroff(void)
{
  system("echo \"SHUTDOWN IN 1 SEC by cloonix-agent\" | wall");
  g_halt_countdown = 2;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void call_reboot(void)
{
  system("echo \"REBOOT IN 1 SEC by cloonix-agent\" | wall");
  g_reboot_countdown = 2;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int add_listen_x11(int display_sock_x11)
{
  int fd_listen;
  char x11_path[MAX_ASCII_LEN];
  memset(x11_path, 0, MAX_ASCII_LEN);
  snprintf(x11_path, MAX_ASCII_LEN-1, X11_DISPLAY_PREFIX,
           display_sock_x11 + X11_DISPLAY_OFFSET);
  fd_listen = util_socket_listen_unix(x11_path);
  if (fd_listen <= 0)
    KOUT("ERROR %d %d", fd_listen, errno);
  return fd_listen;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void remove_listen_x11(t_dropbear_ctx *dctx)
{
  char x11_path[MAX_ASCII_LEN];
  if (dctx->fd_listen_x11 != -1)
    {
    memset(x11_path, 0, MAX_ASCII_LEN);
    snprintf(x11_path, MAX_ASCII_LEN-1, X11_DISPLAY_PREFIX,
             dctx->display_sock_x11);
    close(dctx->fd_listen_x11);
    unlink(x11_path);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void alloc_ctx(int display_sock_x11, int dido_llid, int fd,
                      int is_allocated_x11_display_idx)
{
  t_dropbear_ctx *dctx;
  if (g_fd_to_ctx[fd])
    KOUT("ERROR ");
  if ((display_sock_x11 < 1) || (display_sock_x11 > X11_DISPLAY_IDX_MAX))
    KOUT("ERROR%d", display_sock_x11);
  dctx = (t_dropbear_ctx *) malloc(sizeof(t_dropbear_ctx));
  memset(dctx, 0, sizeof(t_dropbear_ctx));
  dctx->dido_llid = dido_llid;
  dctx->fd = fd;
  nonblock_add_fd(dctx->fd);
  dctx->is_allocated_x11_display_idx = is_allocated_x11_display_idx;
  dctx->display_sock_x11 = display_sock_x11;
  dctx->fd_listen_x11 = add_listen_x11(dctx->display_sock_x11);
  g_ctx[dido_llid] = dctx;
  g_fd_to_ctx[fd] = dctx;
  if (g_head_ctx)
    g_head_ctx->prev = dctx;
  dctx->next = g_head_ctx;
  g_head_ctx = dctx;
  g_dropbear_ctx_num += 1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_dropbear_ctx *get_ctx(int dido_llid)
{
  return (g_ctx[dido_llid]);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_ctx(int dido_llid)
{
  char buf[MAX_A2D_LEN];
  int headsize = sock_header_get_size();
  char *payload = buf + headsize;
  t_dropbear_ctx *dctx = get_ctx(dido_llid);
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
    remove_listen_x11(dctx);
    if (dctx->is_allocated_x11_display_idx)
      x11_pool_release(dctx->display_sock_x11);
    if (dctx->next)
      dctx->next->prev = dctx->prev;
    if (dctx->prev)
      dctx->prev->next = dctx->next;
    if (dctx == g_head_ctx)
      g_head_ctx = dctx->next;
    free(dctx);
    g_dropbear_ctx_num -= 1;
    if (g_dropbear_ctx_num < 0)
      KOUT("ERROR ");
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void action_rx_dropbear(t_dropbear_ctx *dctx, int len, char *rx)
{
  char buf[MAX_A2D_LEN];
  int headsize = sock_header_get_size();
  char *payload = buf + headsize;
  memcpy(payload, rx, len);
  send_to_virtio(dctx->dido_llid, 0, 0, header_type_traffic,
                 header_val_none, len, buf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void action_events(fd_set *infd)
{
  int len;
  char *rx;
  t_dropbear_ctx *tst_dctx, *next, *dctx = g_head_ctx;
  while (dctx)
    {
    next = dctx->next;
    if (dctx->fd_listen_x11 == -1)
      KERR("ERROR %d %d", dctx->dido_llid, dctx->display_sock_x11);
    else if (FD_ISSET(dctx->fd_listen_x11, infd))
      {
      x11_event_listen(dctx->dido_llid, dctx->display_sock_x11, 
                       dctx->fd_listen_x11);
      }
    if (FD_ISSET(dctx->fd, infd))
      {
      tst_dctx = g_fd_to_ctx[dctx->fd];
      if (tst_dctx != dctx)
        KOUT("ERROR%p %p", tst_dctx, dctx);
      len = receive_from_dropbear(dctx->fd, &rx);
      if (len > 0)
        {
        action_rx_dropbear(dctx, len, rx);
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
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void action_prepare_fd_set(fd_set *infd, fd_set *outfd)
{
  t_dropbear_ctx *dctx = g_head_ctx;
  while (dctx)
    {
    FD_SET(dctx->fd, infd);
    nonblock_prepare_fd_set(dctx->fd, outfd);
    if (dctx->fd_listen_x11 != -1)
      FD_SET(dctx->fd_listen_x11, infd);
    dctx = dctx->next;
    } 
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int action_get_biggest_fd(void)
{
  int result = 0;
  t_dropbear_ctx *dctx = g_head_ctx;
  while (dctx)
    {
    if (dctx->fd > result)
      result = dctx->fd;
    if (dctx->fd_listen_x11 > result)
      result = dctx->fd_listen_x11;
    dctx = dctx->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static unsigned long long get_cached_ram(void)
{
  int found = 0;
  unsigned long val;
  unsigned long long result = 0;
  FILE *fhd;
  char line[80];
  fhd = fopen("/proc/meminfo", "r");
  if (fhd)
    {
    while(!found)
      {
      if (fgets(line, 80, fhd) != NULL)
        {
        if (sscanf(line, "Cached: %lu kB", &val) == 1)
          {
          result = (unsigned long long) val;
          result *= 1024;
          found = 1;
          }
        }
      }
    fclose(fhd);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void create_msg_df(char *msg, int max)
{
  int err, len_done = 0;
  char *cmd = "/bin/df";
  char buf[101];
  FILE *fp;
  err = access(cmd, X_OK);
  if (err)
    KERR("ERROR %s NOT FOUND", cmd);
  else
    {
    fp = popen(cmd, "r");
    while (fgets(buf, 100, fp))
      {
      buf[100] = 0;
      if ((strlen(buf) + len_done) >= max)
        break; 
      len_done += snprintf(msg+len_done, 100, "%s", buf);
      }
    pclose(fp);
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void send_back_sysinfo_df(char *rx)
{
  char buf[MAX_A2D_LEN];
  int headsize = sock_header_get_size();
  char *payload = buf + headsize;
  if (!strcmp(rx, LASTATSDF))
    {
    payload[MAX_RPC_MSG_LEN-1] = 0;
    create_msg_df(payload, MAX_RPC_MSG_LEN);
    send_to_virtio(0, 0, 0, header_type_stats, header_val_sysinfo_df,
                   strlen(payload) + 1, buf);
    }
  else
    KERR("ERROR");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void send_back_sysinfo(char *rx)
{
  char buf[MAX_A2D_LEN];
  int headsize = sock_header_get_size();
  char *payload = buf + headsize;
  struct sysinfo sys;
  unsigned long long llcached_ram;
  unsigned long cached_ram;
  
  if (!strcmp(rx, LASTATS))
    {
    if (sysinfo(&sys))
      KERR("ERROR SYSINFO %d", errno);
    else
      { 
      llcached_ram = get_cached_ram();
      llcached_ram /= sys.mem_unit;
      cached_ram = (unsigned long) llcached_ram;
      snprintf(payload, MAX_A2D_LEN-headsize-1, SYSINFOFORMAT,
               (unsigned long) sys.uptime, sys.loads[0], sys.loads[1], 
               sys.loads[2], sys.totalram, sys.freeram, cached_ram,
               sys.sharedram, sys.bufferram, sys.totalswap, sys.freeswap,
               (unsigned long) sys.procs, sys.totalhigh, sys.freehigh,
               (unsigned long) sys.mem_unit);
      send_to_virtio(0, 0, 0, header_type_stats, header_val_sysinfo,
                     strlen(payload) + 1, buf);
      }
    }
  else
    KERR("ERROR");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void helper_rx_virtio_vwifi(int type, int val,
                                   int vwifi_base, int vwifi_cid,
                                   int len, char *rx)
{
  if (type == header_type_vwifi_cli)
    {
    if (val == header_val_vwifi_trf_ok)
      vwifi_virtio_traf_cli(vwifi_cid, rx, len);
    else if (val == header_val_vwifi_syn_ok)
      {
      vwifi_virtio_syn_ok_cli(vwifi_cid);
      }
    else if (val == header_val_vwifi_syn_ko)
      {
      vwifi_virtio_syn_ko_cli(vwifi_cid);
      }
    else
      KERR("ERROR %d", val);
    }
  else if (type == header_type_vwifi_spy)
    {
    if (val == header_val_vwifi_trf_ok)
      vwifi_virtio_traf_spy(vwifi_cid, rx, len);
    else if (val == header_val_vwifi_syn_ok)
      vwifi_virtio_syn_ok_spy(vwifi_cid);
    else if (val == header_val_vwifi_syn_ko)
      vwifi_virtio_syn_ko_spy(vwifi_cid);
    else
      KERR("ERROR %d", val);
    }
  else if (type == header_type_vwifi_ctr)
    {
    if (val == header_val_vwifi_trf_ok)
      vwifi_virtio_traf_ctr(vwifi_cid, rx, len);
    else if (val == header_val_vwifi_syn_ok)
      vwifi_virtio_syn_ok_ctr(vwifi_cid);
    else if (val == header_val_vwifi_syn_ko)
      vwifi_virtio_syn_ko_ctr(vwifi_cid);
    else
      KERR("ERROR %d", val);
    }
  else
    KERR("ERROR %d %d", type, val);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
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
          vwifi_start_ok();
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
  else if (type == header_type_stats)
    {
    switch (val)
      {
      case header_val_sysinfo:
        send_back_sysinfo(rx);
        break;
      case header_val_sysinfo_df:
        send_back_sysinfo_df(rx);
        break;
      default:
        KERR("ERROR %d", val);
        break;
      }
    }
  else
    KERR("ERROR %d %d", type, val);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int choose_x11_display_idx(int inside_cloonix_x11_display_idx)
{
  int result = 0;
  if (inside_cloonix_x11_display_idx)
    result = inside_cloonix_x11_display_idx;
  else
    result = x11_pool_alloc();
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void helper_rx_virtio_noctx(int dido_llid, int type, int val, char *rx)
{
  char buf[MAX_A2D_LEN];
  int headsize = sock_header_get_size();
  char *payload = buf + headsize;
  int fd, inside_cloonix_x11_display_idx, display_sock_x11;
  int is_allocated_x11_display_idx;
  char xauth_cookie_format[MAX_ASCII_LEN];
  char *ptr;
  if ((type == header_type_ctrl_agent) && (val == header_val_add_dido_llid))
    {
    if (g_dropbear_ctx_num >= MAX_SSH_NUM)
      KERR("ERROR");
    else if (sscanf(rx, DBSSH_SERV_DOORS_REQ, &inside_cloonix_x11_display_idx,
                            xauth_cookie_format) != 2)
      KERR("ERROR %s", rx);
    else
      {
      ptr = strstr(rx, xauth_cookie_format);
      if (!ptr)
        KOUT("ERROR %s", ptr);
      memset(xauth_cookie_format, 0, MAX_ASCII_LEN);
      strncpy(xauth_cookie_format, ptr, MAX_ASCII_LEN-1);
      display_sock_x11=choose_x11_display_idx(inside_cloonix_x11_display_idx);
      if ((display_sock_x11 >= 1) && (display_sock_x11 <= X11_DISPLAY_IDX_MAX))
        {
        fd = sock_client_unix(UNIX_DROPBEAR_SOCK);
        if (fd >= MAX_FD_NUM)
          KOUT("ERROR %d", fd);
        else if (fd <= 0)
          KERR("ERROR %d %d", fd, errno);
        else 
          {
          if (inside_cloonix_x11_display_idx)
            is_allocated_x11_display_idx = 0;
          else
            is_allocated_x11_display_idx = 1;
          alloc_ctx(display_sock_x11, dido_llid, fd, 
                    is_allocated_x11_display_idx);
          snprintf(payload,MAX_ASCII_LEN-1,DBSSH_SERV_DOORS_RESP,display_sock_x11); 
          send_to_virtio(dido_llid, 0, 0, header_type_ctrl_agent,
                         header_val_add_dido_llid, strlen(payload) + 1, buf);
          }
        }
      else
        KERR("ERROR %s %d", rx, display_sock_x11);
      }
    }
  else if ((type==header_type_ctrl_agent) && (val==header_val_del_dido_llid))
    {
    }
  else if ((type == header_type_x11_ctrl) && (val == header_val_x11_open_serv))
    {
    KERR("ERROR %d %s", dido_llid, rx);
    }
  else if ((type==header_type_ctrl_agent) && (val==header_val_ack))
    {
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void action_rx_virtio(int dido_llid, int vwifi_base, int vwifi_cid,
                      int type, int val, int len, char *rx)
{
  t_dropbear_ctx *dctx;
  if (vwifi_cid)
    helper_rx_virtio_vwifi(type, val, vwifi_base, vwifi_cid, len, rx);
  else if (!dido_llid) 
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
          send_to_dropbear(dctx->fd, rx, len);
        else
          KERR("ERROR %d %d", type, val);
        }
      else if (type == header_type_ctrl_agent)
        {
        if (val == header_val_del_dido_llid)
          free_ctx(dido_llid);
        else
          KERR("ERROR %d %d", type, val);
        }
      else if (type == header_type_ctrl)
        {
        KERR("ERROR %d %d", type, val);
        }
      else if (type == header_type_x11_ctrl)
        {
        if (val == header_val_x11_open_serv)
          x11_rx_ack_open_x11(dido_llid, rx);
        else
          KERR("ERROR %d", val);
        }
      else
        KERR("ERROR %d %d", type, val);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void action_heartbeat(int cur_sec)
{
  if (g_reboot_countdown)
    {
    g_reboot_countdown -= 1;
    if (g_reboot_countdown == 0)
      {
      reboot(LINUX_REBOOT_CMD_RESTART);
      }
    }
  if (g_halt_countdown)
    {
    g_halt_countdown -= 1;
    if (g_halt_countdown == 0)
      {
      reboot(LINUX_REBOOT_CMD_POWER_OFF);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void action_init(void)
{
  g_reboot_countdown = 0;
  g_halt_countdown = 0;
  g_dropbear_ctx_num = 0;
  g_head_ctx = NULL;
  memset(g_fd_to_ctx, 0, MAX_FD_NUM * sizeof(t_dropbear_ctx *));
  memset(g_ctx, 0, CLOWNIX_MAX_CHANNELS * sizeof(t_dropbear_ctx *));
}
/*--------------------------------------------------------------------------*/

