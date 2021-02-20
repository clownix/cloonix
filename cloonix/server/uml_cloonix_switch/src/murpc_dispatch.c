/*****************************************************************************/
/*    Copyright (C) 2006-2021 clownix@clownix.net License AGPL-3             */
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
#include <sys/stat.h>
#include <signal.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_daemon.h"
#include "event_subscriber.h"
#include "hop_event.h"
#include "cfg_store.h"
#include "endp_mngt.h"
#include "mulan_mngt.h"
#include "unix2inet.h"
#include "dpdk_ovs.h"
#include "fmt_diag.h"
#include "murpc_dispatch.h"
#include "suid_power.h"
#include "snf_dpdk_process.h"
#include "nat_dpdk_process.h"
#include "d2d_dpdk_process.h"
#include "a2b_dpdk_process.h"


/****************************************************************************/
typedef struct t_mutimeout
{
  int llid;
  int cli_llid;
  int cli_tid;
  char name[MAX_NAME_LEN];
  int num;
  struct t_mutimeout *prev;
  struct t_mutimeout *next;
} t_mutimeout;
/*--------------------------------------------------------------------------*/

static t_mutimeout *g_head;
/*--------------------------------------------------------------------------*/

int get_glob_req_self_destruction(void);

/****************************************************************************/
void snf_globtopo_small_event(char *name, int num_evt, char *path)
{
  t_small_evt evt;
  memset(&evt, 0, sizeof(t_small_evt));
  strncpy(evt.name, name, MAX_NAME_LEN-1);
  evt.evt = num_evt;
  event_subscriber_send(topo_small_event, (void *) &evt);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_mutimeout *mutimeout_find(int llid, int cli_llid, int cli_tid)
{
  t_mutimeout *cur = g_head;
  while(cur)
    {
    if ((cur->llid == llid) && 
        (cur->cli_llid == cli_llid) && 
        (cur->cli_tid == cli_tid))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int mutimeout_unchain(int llid, int cli_llid, int cli_tid)
{
  int result = 0;
  t_mutimeout *cur = mutimeout_find(llid, cli_llid, cli_tid);
  if (cur)
    {
    result = 1;
    if (cur->prev)
      cur->prev->next = cur->next;
    if (cur->next)
      cur->next->prev = cur->prev;
    if (cur == g_head)
      g_head = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void muresp_timeout(void *data)
{
  t_mutimeout *mut = (t_mutimeout *) data;
  if (mutimeout_unchain(mut->llid, mut->cli_llid, mut->cli_tid))
    {
    KERR(" ");
    if (msg_exist_channel(mut->cli_llid))
      {
      send_mucli_dialog_resp(mut->cli_llid, mut->cli_tid,
                             mut->name, mut->num, "RESPKO TIMEOUT", 1);
      }
    }
  clownix_free(mut, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void mutimeout_chain(int llid, int cli_llid, int cli_tid, 
                            char *name, int num)
{
  t_mutimeout *cur; 
  cur = (t_mutimeout *) clownix_malloc(sizeof(t_mutimeout), 4);
  memset(cur, 0, sizeof(t_mutimeout));
  cur->llid = llid;
  cur->cli_llid = cli_llid;
  cur->cli_tid = cli_tid;
  cur->num = num;
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  if (g_head)
    g_head->prev = cur;
  cur->next = g_head;
  g_head = cur;
  clownix_timeout_add(2000, muresp_timeout, (void *) cur, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_mucli_dialog_req(int llid, int tid, char *name, int num, char *line)
{
  char newline[MAX_RPC_MSG_LEN];
  int is_wlan=0, endp_type, mullid=0;

  if (get_glob_req_self_destruction())
    return;

  if (!mullid)
    mullid = mulan_can_be_found_with_name(name);
  if (!mullid)
    {
    mullid = muwlan_can_be_found_with_name_num(name, num);
    if (mullid)
      is_wlan = 1;
    }
  if (!mullid)
    mullid = endp_mngt_can_be_found_with_name(name, num, &endp_type);
  if (!mullid)
    mullid = nat_dpdk_llid_exists_with_name(name);
  if (!mullid)
    mullid = a2b_dpdk_llid_exists_with_name(name);
  if (!mullid)
    send_mucli_dialog_resp(llid, tid, name, num, "KO NOT FOUND", 1);
  else
    {
    if (is_wlan)
      {
      mutimeout_chain(mullid, llid, tid, name, num);
      snprintf(newline, MAX_RPC_MSG_LEN-1,"name=%s num=%d %s",name,num,line);
      newline[MAX_RPC_MSG_LEN-1] = 0;
      rpct_send_cli_req(NULL, mullid, 0, llid, tid, newline);
      }
    else
      {
      mutimeout_chain(mullid, llid, tid, name, num);
      rpct_send_cli_req(NULL, mullid, 0, llid, tid, line);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_mucli_dialog_resp(int llid, int tid, 
                            char *name, int num, char *line, int status)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_cli_req(void *ptr, int llid, int tid, 
                    int cli_llid, int cli_tid, char *line)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void update_snf(char *name, char *line)
{
  int type;
  char *ptr, *ptrend;
  if (!endp_mngt_exists(name, 0, &type))
    KERR("%s", name);
  else if (type != endp_type_snf)
    KERR("%s", name);
  else
    {
    if (!strncmp(line, "GET_CONF_RESP", strlen("GET_CONF_RESP")))
      {
      ptr = line + strlen("GET_CONF_RESP");
      ptrend = strchr(ptr, ' ');
      if (!ptrend)
        KERR("%s", ptr); 
      else
        {
        ptr = ptrend + 1;
        if (ptr[0] == '1')
          snf_globtopo_small_event(name, snf_evt_capture_on, NULL);
        else
          snf_globtopo_small_event(name, snf_evt_capture_off, NULL);
        }
      }
    else
      KERR("%s %s", name, line);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_cli_resp(void *ptr, int llid, int tid, 
                        int cli_llid,int cli_tid, char *line)
{
  char name[MAX_NAME_LEN];
  int mutype, num = 0;

  if (get_glob_req_self_destruction())
    return;

  hop_event_hook(llid, FLAG_HOP_DIAG, line);
  if (cli_llid)
    {
    if (!mutimeout_unchain(llid, cli_llid, cli_tid))
      KERR("%d %s", tid, line);
    }
  if (endp_mngt_can_be_found_with_llid(llid, name, &num, &mutype))
    {
    if (mutype == endp_type_snf)
      update_snf(name, line);
    send_mucli_dialog_resp(cli_llid, cli_tid, name, num, line, 0);
    }
  else if (mulan_can_be_found_with_llid(llid, name))
    send_mucli_dialog_resp(cli_llid, cli_tid, name, num, line, 0);
  else if (nat_dpdk_name_exists_with_llid(llid, name))
    send_mucli_dialog_resp(cli_llid, cli_tid, name, num, line, 0);
  else
    KERR("CANNOT BE %s", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_evt_msg(void *ptr, int llid, int tid, char *line)
{
  char name[MAX_NAME_LEN];
  int num, mutype;

  if (get_glob_req_self_destruction())
    return;

  if (endp_mngt_can_be_found_with_llid(llid, name, &num, &mutype))
    {
    endp_mngt_rpct_recv_evt_msg(llid, tid, line);
    hop_event_hook(llid, FLAG_HOP_EVT, line);
    }
  else if (mulan_can_be_found_with_llid(llid, name))
    {
    mulan_rpct_recv_evt_msg(llid, tid, line);
    hop_event_hook(llid, FLAG_HOP_EVT, line);
    }
  else if (dpdk_ovs_find_with_llid(llid))
    {
    fmt_rx_rpct_recv_evt_msg(llid, tid, line);
    hop_event_hook(llid, FLAG_HOP_EVT, line);
    }
  else
    KERR("CANNOT DISPATCH %s", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_app_msg(void *ptr, int llid, int tid, char *line)
{
  char mac[MAX_NAME_LEN];
  char vmname[MAX_NAME_LEN];
  char satname[MAX_NAME_LEN];
  char msg[MAX_PATH_LEN];
  int num, mutype, llid_con;

  if (get_glob_req_self_destruction())
    return;

  if (endp_mngt_can_be_found_with_llid(llid, satname, &num, &mutype))
    {
    if (mutype == endp_type_nat)
      {
      if (sscanf(line, "unix2inet_conpath_evt_monitor llid=%d name=%s",
                 &llid_con, vmname) == 2)
        {
        unix2inet_monitor(llid, llid_con, satname, vmname);
        }
      else if (sscanf(line, "req_for_name_with_mac=%s", mac) == 1)
        {
        if (!cfg_get_name_with_mac(mac, vmname)) 
          sprintf(msg, "resp_for_name_with_mac=%s name_valid=%d name=%s", 
                        mac, 1, vmname);
        else
          sprintf(msg, "resp_for_name_with_mac=%s name_valid=%d name=%s", 
                        mac, 0, "nogood");
        rpct_send_app_msg(NULL, llid, 0, msg);
        }
      else
        KERR("%s", line);
      }
    else
      KERR("%s", line);
    }
  else if (nat_dpdk_name_exists_with_llid(llid, satname))
    {
    if (sscanf(line, "unix2inet_conpath_evt_monitor llid=%d name=%s",
               &llid_con, vmname) == 2)
      {
      //unix2inet_monitor(llid, llid_con, satname, vmname);
      }
    else
      KERR("%s", line);
    }
  else
    KERR("%s", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_diag_msg(void *ptr, int llid, int tid, char *line)
{
  char name[MAX_NAME_LEN];
  int num, mutype;

  if (get_glob_req_self_destruction())
    return;

  if (suid_power_diag_llid(llid))
    {
    suid_power_diag_resp(llid, tid, line);
    }
  else if (snf_dpdk_diag_llid(llid))
    {
    snf_dpdk_diag_resp(llid, tid, line);
    }
  else if (nat_dpdk_diag_llid(llid))
    {
    nat_dpdk_diag_resp(llid, tid, line);
    }
  else if (a2b_dpdk_diag_llid(llid))
    {
    a2b_dpdk_diag_resp(llid, tid, line);
    }
  else if (d2d_dpdk_diag_llid(llid))
    {
    d2d_dpdk_diag_resp(llid, tid, line);
    }
  else if (endp_mngt_can_be_found_with_llid(llid, name, &num, &mutype))
    {
    hop_event_hook(llid, FLAG_HOP_DIAG, line);
    endp_mngt_rpct_recv_diag_msg(llid, tid, line);
    }
  else if (mulan_can_be_found_with_llid(llid, name))
    {
    hop_event_hook(llid, FLAG_HOP_DIAG, line);
    mulan_rpct_recv_diag_msg(llid, tid, line);
    }
  else if (dpdk_ovs_find_with_llid(llid))
    {
    hop_event_hook(llid, FLAG_HOP_DIAG, line);
    dpdk_ovs_rpct_recv_diag_msg(llid, tid, line);
    }
  else
    {
    KERR("CANNOT DISPATCH %s", line);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void murpc_dispatch_send_tx_flow_control(int llid, int rank, int stop)
{
  char line[MAX_PATH_LEN];
  memset(line, 0, MAX_PATH_LEN);
  snprintf(line, MAX_PATH_LEN-1, 
  "cloonix_evt_peer_flow_control_tx rank=%d stop=%d", rank, stop);
  rpct_send_evt_msg(NULL, llid, 0, line);
}
/*---------------------------------------------------------------------------*/

