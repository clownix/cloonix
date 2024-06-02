/*****************************************************************************/
/*    Copyright (C) 2006-2024 clownix@clownix.net License AGPL-3             */
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
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "util_sock.h"
#include "cfg_store.h"
#include "pid_clone.h"
#include "utils_cmd_line_maker.h"
#include "llid_trace.h"
#include "doors_rpc.h"
#include "xwy.h"

#define CLOONIX_KIL_REQ "KillYou"
#define CLOONIX_PID_REQ "Your_Pid?"
#define CLOONIX_PID_RESP "My_Pid:%d"

int get_killing_cloonix(void);
int get_glob_req_self_destruction(void);


typedef struct t_xwy_params
{
  char bin[MAX_PATH_LEN];
  char unix_traffic_sock[MAX_PATH_LEN];
  char unix_control_sock[MAX_PATH_LEN];
  char cloonix_net_name[MAX_NAME_LEN];
} t_xwy_params;

enum {
  xwy_state_init = 8,
  xwy_state_conn_req,
  xwy_state_connected,
  xwy_state_pid_ok,
};

static t_xwy_params g_xwy_params;
static int g_xwy_kill_req;
static int g_xwy_state;
static int g_xwy_pid;
static int g_xwy_doors_connect;
static int g_xwy_last_pid;
static int g_xwy_llid;

int get_doorways_llid(void);

/*****************************************************************************/
static void set_state(int xwy_state)
{
  if ((xwy_state == xwy_state_init)      ||
      (xwy_state == xwy_state_conn_req)  ||
      (xwy_state == xwy_state_connected) ||
      (xwy_state == xwy_state_pid_ok))
    g_xwy_state = xwy_state;
  else
    KOUT("%d", xwy_state);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int start_xwy(void *data)
{
  t_xwy_params *dp = (t_xwy_params *) data;
  char *argv[] = {dp->bin, 
                  dp->unix_traffic_sock,
                  dp->unix_control_sock,
                  dp->cloonix_net_name,
                  NULL};
  execv(dp->bin, argv);
  KOUT("ERROR execv");
  return 0;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void fatal_xwy_err(int llid)
{
  if (llid != g_xwy_llid)
    KOUT("ERROR %d %d", llid, g_xwy_llid);
  if (!g_xwy_llid)
    KOUT(" ");
  llid_trace_free(g_xwy_llid, 0, __FUNCTION__);
  g_xwy_llid = 0;
  g_xwy_pid = 0;
  g_xwy_doors_connect = 0;
  set_state(xwy_state_init);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void xwy_err_cb (int llid, int err, int from)
{
  if (g_xwy_kill_req ==0)
    KERR("ERROR %d %d", err, from);
  fatal_xwy_err(llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int xwy_rx_cb(int llid, int fd)
{
  int len, pid;
  char buf[2*MAX_PATH_LEN];
  memset(buf, 0, 2*MAX_PATH_LEN);
  len = util_read(buf, 2*MAX_PATH_LEN, fd);
  if (len < 0)
    {
    KERR("ERROR %d", errno);
    fatal_xwy_err(llid);
    }
  else
    {
    if (sscanf(buf, CLOONIX_PID_RESP, &pid) == 1)
      {
      if (pid <= 0)
        KERR("%d", pid);
      if ((g_xwy_pid) && (g_xwy_pid != pid))
        KERR("pid changed: %d %d", g_xwy_pid, pid);
      set_state(xwy_state_pid_ok);
      g_xwy_pid = pid;
      g_xwy_last_pid = pid;
      }
    else
      KERR("%d %s", len, buf);
    }
  return len;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void reset_starting(void)
{
  if (g_xwy_llid)
    llid_trace_free(g_xwy_llid, 0, __FUNCTION__);
  g_xwy_llid = 0;
  g_xwy_pid = 0;
  g_xwy_doors_connect = 0;
  set_state(xwy_state_init);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_monitor_xwy_pid(void *data)
{
  static int count = 0;
  int fd;
  if (g_xwy_state == xwy_state_init)
    {
    if (g_xwy_kill_req == 0)
      {
      if (get_glob_req_self_destruction())
        return;
      if (get_killing_cloonix())
        return;
      pid_clone_launch(start_xwy, NULL, NULL,
                      (void *) (&g_xwy_params),
                      NULL, NULL, "xwy", -1, 1);
      set_state(xwy_state_conn_req);
      }
    }
  else if (g_xwy_state == xwy_state_conn_req)
    {
    if (util_nonblock_client_socket_unix(g_xwy_params.unix_control_sock, &fd))
      {
      reset_starting();
      KERR("ERROR %s", g_xwy_params.unix_control_sock);
      }
    else
      {
      if (fd < 0)
        KOUT("ERROR");
      g_xwy_llid = msg_watch_fd(fd, xwy_rx_cb, xwy_err_cb, "cloon");
      if (g_xwy_llid == 0)
        KOUT("ERROR");
      llid_trace_alloc(g_xwy_llid,"CLOON",0,0, type_llid_trace_unix_xwy);
      set_state(xwy_state_connected);
      }
    }
  else if (g_xwy_state == xwy_state_connected)
    {
    if (!msg_exist_channel(g_xwy_llid))
      {
      reset_starting();
      KERR("ERROR %s", g_xwy_params.unix_control_sock);
      }
    else
      {
      watch_tx(g_xwy_llid, strlen(CLOONIX_PID_REQ) + 1, CLOONIX_PID_REQ);
      count += 1;
      if (count > 5)
        KERR("WARNING XWY PID REQ %d", count);
      }
    }
 else if (g_xwy_state == xwy_state_pid_ok)
    {
    if (!msg_exist_channel(g_xwy_llid))
      {
      reset_starting();
      KERR("ERROR %s", g_xwy_params.unix_control_sock);
      }
    else
      {
      count = 0;
      watch_tx(g_xwy_llid, strlen(CLOONIX_PID_REQ) + 1, CLOONIX_PID_REQ);
      if (g_xwy_doors_connect == 0)
        g_xwy_doors_connect = 1;
      else if (g_xwy_doors_connect == 1)
        {
        g_xwy_doors_connect = 2;
        xwy_request_doors_connect();
        }
      }
    }
  else
    KOUT("%d", g_xwy_state);
  if (g_xwy_state == xwy_state_pid_ok)
    clownix_timeout_add(200, timer_monitor_xwy_pid, NULL, NULL, NULL);
  else if (g_xwy_state == xwy_state_conn_req)
    clownix_timeout_add(100, timer_monitor_xwy_pid, NULL, NULL, NULL);
  else
    clownix_timeout_add(10, timer_monitor_xwy_pid, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void kill_xwy(void)
{
  g_xwy_kill_req = 1;
  watch_tx(g_xwy_llid, strlen(CLOONIX_KIL_REQ) + 1, CLOONIX_KIL_REQ);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int xwy_pid(void)
{
  return g_xwy_last_pid;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void xwy_request_doors_connect(void)
{
  char buf[2*MAX_PATH_LEN];
  memset(buf, 0, 2*MAX_PATH_LEN);
  snprintf(buf, 2*MAX_PATH_LEN, XWY_CONNECT, g_xwy_params.unix_traffic_sock);
  buf[MAX_PATH_LEN-1] = 0;
  doors_send_command(get_doorways_llid(), 0, "noname", buf);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int init_xwy_done(void)
{
  int result = 0;
  if (g_xwy_doors_connect == 2)
    result = 1;
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void init_xwy(char *cloonix_net_name)
{
  g_xwy_kill_req = 0;
  g_xwy_pid = 0;
  g_xwy_doors_connect = 0;
  g_xwy_last_pid = 0;
  g_xwy_llid = 0;
  set_state(xwy_state_init);
  memset(&g_xwy_params, 0, sizeof(t_xwy_params));
  snprintf(g_xwy_params.cloonix_net_name, MAX_NAME_LEN, cloonix_net_name);
  snprintf(g_xwy_params.bin, MAX_PATH_LEN, "%s/server/cloonix-xwy-srv", 
                                            cfg_get_bin_dir());
  g_xwy_params.bin[MAX_PATH_LEN-1] = 0;
  snprintf(g_xwy_params.unix_traffic_sock, MAX_PATH_LEN, "%s/%s", 
                                   cfg_get_root_work(), XWY_TRAFFIC_SOCK);
  g_xwy_params.unix_traffic_sock[MAX_PATH_LEN-1] = 0;
  snprintf(g_xwy_params.unix_control_sock, MAX_PATH_LEN, "%s/%s", 
                                   cfg_get_root_work(), XWY_CONTROL_SOCK);
  g_xwy_params.unix_control_sock[MAX_PATH_LEN-1] = 0;
  clownix_timeout_add(10, timer_monitor_xwy_pid, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/




