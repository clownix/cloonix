/*****************************************************************************/
/*    Copyright (C) 2006-2018 cloonix@cloonix.net License AGPL-3             */
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
#include <sys/types.h>
#include <sys/time.h>
#include <math.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doors_rpc.h"
#include "llid_traffic.h"
#include "llid_x11.h"
#include "llid_backdoor.h"
#include "stats.h"
#include "doorways_sock.h"
#include "dispach.h"
#include "local_dropbear.h"
#include "c2c.h"
#include "pid_clone.h"
/*--------------------------------------------------------------------------*/
static int  g_doorways_llid;
static char g_root_work[MAX_PATH_LEN];
static char g_password[MSG_DIGEST_LEN];
static int g_llid_doors_listen;
static int g_server_inet_port;
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_u2i_nat_path_with_name(char *nat)
{
  static char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "%s/endp/%s_0_u2i", g_root_work, nat);
  return path;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_local_dropbear_sock(void)
{
  static char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "%s/%s", g_root_work, DROPBEAR_SOCK);
  return path;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_switch_path(void)
{
  static char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,  "%s/%s", g_root_work, CLOONIX_SWITCH);
  return path;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static char *get_ctrl_doors(void)
{
  static char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "%s/%s", g_root_work, DOORS_CTRL_SOCK);
  return path;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int get_doorways_llid(void)
{
  return g_doorways_llid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static time_t my_second(void)
{
  static time_t offset = 0;
  struct timeval tv;
  gettimeofday(&tv, NULL);
  if (offset == 0)
    offset = tv.tv_sec;
  return (tv.tv_sec - offset);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void heartbeat (int delta)
{
  static int count_ticks_blkd = 0;
  static int count_ticks_rpct = 0;
  static int g_time_count = 0;
  int cur_sec = my_second();
  count_ticks_blkd += 1;
  count_ticks_rpct += 1;
  if (count_ticks_blkd == 5)
    {
    blkd_heartbeat(NULL);
    count_ticks_blkd = 0;
    }
  if (count_ticks_rpct == 100)
    {
    rpct_heartbeat(NULL);
    count_ticks_rpct = 0;
    }
  if (cur_sec >= g_time_count+1)
    {
    g_time_count += 1;
    stats_heartbeat(cur_sec);
    }

}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void doors_recv_c2c_req_idx(int llid, int tid, char *name)
{
  c2c_from_switch_req_idx(name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void doors_recv_c2c_req_conx(int llid, int tid, char *name, int peer_idx,
                             int peer_ip, int peer_port, char *passwd)
{
  c2c_from_switch_req_conx(name, peer_idx, peer_ip, peer_port, passwd);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void doors_recv_c2c_req_free(int llid, int tid, char *name)
{
  c2c_from_switch_req_free(name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void doors_recv_command(int llid, int tid, char *name, char *cmd)
{
  int job_idx;
  char addr[MAX_PATH_LEN];
  memset(addr, 0, MAX_PATH_LEN);
  if (!strcmp(cmd, CLOONIX_UP_VPORT_AND_RUNNING))
    llid_backdoor_cloonix_up_vport_and_running(name);
  else if (!strcmp(cmd, CLOONIX_DOWN_AND_NOT_RUNNING))
    llid_backdoor_cloonix_down_and_not_running(name);
  else if (sscanf(cmd, REBOOT_REQUEST, &job_idx) == 1)
    llid_backdoor_tx_reboot_to_agent(name, job_idx);
  else if (sscanf(cmd, HALT_REQUEST, &job_idx) == 1)
    llid_backdoor_tx_halt_to_agent(name, job_idx);
  else if (!strcmp(cmd, FIFREEZE_FITHAW_FREEZE))
    llid_backdoor_tx_fifreeze_freeze_to_agent(name);
  else if (!strcmp(cmd, FIFREEZE_FITHAW_THAW))
    llid_backdoor_tx_fifreeze_thaw_to_agent(name);
  else if (!strcmp(cmd, STOP_DOORS_LISTENING))
    {
    if (msg_exist_channel(g_llid_doors_listen))
      msg_delete_channel(g_llid_doors_listen);
    }
  else
    KERR("%s  %s", name, cmd);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_pid_req(void *ptr, int llid, int tid, char *name, int num)
{
  rpct_send_pid_resp(ptr, llid, tid, name, num, cloonix_get_pid(), getpid());
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_hop_sub(void *ptr, int llid, int tid, int flags_hop)
{
  rpct_hop_print_add_sub(ptr, llid, tid, flags_hop);
  DOUT(NULL, FLAG_HOP_DOORS, "Hello from doors process %d", getpid());
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_hop_unsub(void *ptr, int llid, int tid)
{
  rpct_hop_print_del_sub(ptr, llid);
}
/*---------------------------------------------------------------------------*/


/****************************************************************************/
void doors_recv_add_vm(int llid,int tid,char *name, char *way2agent)
{
  DOUT(NULL, FLAG_HOP_DOORS, "%s %s", __FUNCTION__, name);
  llid_backdoor_begin_unix(name, way2agent);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void doors_recv_del_vm(int llid, int tid, char *name)
{
  DOUT(NULL, FLAG_HOP_DOORS, "%s %s", __FUNCTION__, name);
  llid_backdoor_end_unix(name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void err_ctrl_cb (void *ptr, int llid, int err, int from)
{
  exit(0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_ctrl_cb (int llid, int len, char *buf)
{
  if (doors_xml_decoder(llid, len, buf))
    {
    if (rpct_decoder(NULL, llid, len, buf))
      KOUT("%s", buf);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void end_of_init(char *passwd)
{
  static int done = 0;
  int llid;

  if (!done)
    {
    llid = doorways_sock_server_inet(0, g_server_inet_port, passwd,
                                     dispach_door_llid, dispach_door_end,
                                     dispach_door_rx);
    if (!llid)
      KOUT(" ");
    g_llid_doors_listen = llid;
    llid_backdoor_init();
    done = 1;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void connect_from_ctrl_client(void *ptr, int llid, int llid_new)
{
  msg_mngt_set_callbacks (llid_new, err_ctrl_cb, rx_ctrl_cb);
  g_doorways_llid = llid_new;
  end_of_init(g_password);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void connect_dummy(void *ptr, int llid, int llid_new)
{
  KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int tst_port_is_not_used(int port)
{
  int result = -1;
  int llid = string_server_inet(port, connect_dummy, "tst");
  if (msg_exist_channel(llid))
    {
    result = 0;
    msg_delete_channel(llid);
    }
  return result;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static int tst_port(char *str_port, int *port)
{
  int result = 0;
  unsigned long val;
  char *endptr;
  val = strtoul(str_port, &endptr, 10);
  if ((endptr == NULL)||(endptr[0] != 0))
    result = -1;
  if ((val < 1) || (val > 65535))
    result = -1;
  *port = (int) val;
  if (tst_port_is_not_used(*port))
    {
    KERR("PORT:%d is in use", *port);
    result = -1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void fct_timeout_after_birth_self_destruct(void *data)
{
  if (g_doorways_llid == 0)
    KOUT("DOORS NO CONNECT AFTER BIRTH");
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static char *get_name(char *root_work)
{
  static char name[MAX_NAME_LEN];
  char big_name[MAX_PATH_LEN];
  char *ptr;
  memset(name, 0, MAX_NAME_LEN);
  ptr = strrchr(big_name, '/');
  if (!ptr)
    snprintf(name, MAX_NAME_LEN-1, "%s", "doorways");
  else
    {
    ptr += 1;
    snprintf(name, MAX_NAME_LEN-1, "%s_%s", "doorways", ptr);
    } 
  return name;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int main (int argc, char *argv[])
{
  char *ctrl_path;
  doors_xml_init();
  x11_init();
  c2c_init();
  local_dropbear_init();
  msg_mngt_init(get_name(argv[1]), IO_MAX_BUF_LEN);
  dispach_init();
  pid_clone_init();
  g_doorways_llid = 0;
  g_llid_doors_listen = 0;
  if (argc != 4)
    KOUT(" ");
  memset(g_root_work, 0, MAX_PATH_LEN);
  strncpy(g_root_work, argv[1], MAX_PATH_LEN);
  if (tst_port(argv[2], &g_server_inet_port))
    KOUT(" ");
  memset(g_password, 0, MSG_DIGEST_LEN);
  strncpy(g_password, argv[3], MSG_DIGEST_LEN-1);
  doorways_sock_init();
  ctrl_path = get_ctrl_doors();
  unlink(ctrl_path);
  msg_mngt_heartbeat_init(heartbeat);
  string_server_unix(ctrl_path, connect_from_ctrl_client, "ctrl");

//VIP
//  clownix_timeout_add(10000, fct_timeout_after_birth_self_destruct, NULL,
//                      NULL, NULL);
  daemon(0,0);
  cloonix_set_pid(getpid());
  clownix_timeout_add(1000, fct_timeout_after_birth_self_destruct, NULL,
                      NULL, NULL);

  msg_mngt_loop();
  return 0;
}
/*--------------------------------------------------------------------------*/



