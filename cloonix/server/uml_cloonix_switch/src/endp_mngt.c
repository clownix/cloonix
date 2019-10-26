/*****************************************************************************/
/*    Copyright (C) 2006-2019 cloonix@cloonix.net License AGPL-3             */
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
#include "doors_rpc.h"
#include "commun_daemon.h"
#include "event_subscriber.h"
#include "cfg_store.h"
#include "utils_cmd_line_maker.h"
#include "system_callers.h"
#include "llid_trace.h"
#include "mulan_mngt.h"
#include "endp_mngt.h"
#include "file_read_write.h"
#include "pid_clone.h"
#include "hop_event.h"
#include "stats_counters.h"
#include "doorways_mngt.h"
#include "c2c_utils.h"
#include "endp_evt.h"
#include "lan_to_name.h"
#include "layout_rpc.h"
#include "layout_topo.h"
#include "unix2inet.h"
#include "murpc_dispatch.h"
#include "uml_clownix_switch.h"


void local_add_sat(int llid, int tid, char *name, int type, 
                   t_c2c_req_info *c2c_info);


/****************************************************************************/
typedef struct t_time_delay
{
  char name[MAX_NAME_LEN];
  int num;
  t_c2c_req_info c2c_info;
} t_time_delay;
/*--------------------------------------------------------------------------*/


/****************************************************************************/
typedef struct t_lan_connect
{
  char name[MAX_NAME_LEN];
  int num;
  int tidx;
  char lan[MAX_NAME_LEN];
  char lan_sock[MAX_PATH_LEN];
} t_lan_connect;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
typedef struct t_priv_endp
{
  char name[MAX_NAME_LEN];
  int num;
  int clone_start_pid;
  int clone_start_pid_just_done;
  int pid;
  int getsuidroot;
  int open_endp;
  int open_endp_req_sent;
  int init_pcap_snf;
  int endp_type;
  int llid;
  int cli_llid;
  int cli_tid;
  int waiting_resp;
  char waiting_resp_txt[MAX_NAME_LEN];
  int periodic_count;
  int unanswered_pid_req;
  int doors_fd_ready;
  int doors_fd_value;
  int muendp_stop_done;
  int trace_alloc_count;
  char c2c_passwd_slave[MSG_DIGEST_LEN];
  t_topo_c2c c2c;
  t_topo_snf snf;
  t_lan_attached lan_attached[MAX_TRAF_ENDPOINT];
  struct t_priv_endp *prev;
  struct t_priv_endp *next;
} t_priv_endp;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
typedef struct t_argendp
{
  char net_name[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  int num;
  char bin_path[MAX_PATH_LEN];
  char sock[MAX_PATH_LEN];
  int endp_type;
  int cli_llid;
  int cli_tid;
} t_argendp;
/*--------------------------------------------------------------------------*/


static t_priv_endp *g_head_muendp;
static int g_nb_muendp;


/****************************************************************************/
static int try_send_endp(t_priv_endp *mu, char *msg)
{
  int result = -1;
  if (mu->llid)
    {
    if (msg_exist_channel(mu->llid))
      {
      hop_event_hook(mu->llid, FLAG_HOP_DIAG, msg);
      rpct_send_diag_msg(NULL, mu->llid, mu->pid, msg);
      result = 0;
      }
    else
      KERR("%s", msg);
    }
  else
    KERR("%s", msg);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void status_reply_if_possible(int is_ok, t_priv_endp *muendp, char *txt)
{
  char info[MAX_PATH_LEN];
  memset(info, 0, MAX_PATH_LEN);
  if ((muendp->cli_llid) && msg_exist_channel(muendp->cli_llid))
    {
    snprintf(info, MAX_PATH_LEN-1, "%s %s", muendp->name, txt);  
    if (is_ok)
      send_status_ok(muendp->cli_llid, muendp->cli_tid, info);
    else
      send_status_ko(muendp->cli_llid, muendp->cli_tid, info);
    muendp->cli_llid = 0;
    muendp->cli_tid = 0;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int trace_alloc(t_priv_endp *mu)
{
  int llid;
  char *sock = utils_get_endp_path(mu->name, mu->num);
  llid = string_client_unix(sock, uml_clownix_switch_error_cb, 
                                  uml_clownix_switch_rx_cb, "endp");
  if (llid)
    {
    if (hop_event_alloc(llid, type_hop_endp, mu->name, mu->num))
      KERR("%s", mu->name);
    if (mu->endp_type == endp_type_tap)
      llid_trace_alloc(llid, mu->name, 0, 0, type_llid_trace_endp_tap);
    else 
    if (mu->endp_type == endp_type_snf) 
      llid_trace_alloc(llid, mu->name, 0, 0, type_llid_trace_endp_snf);
    else 
    if (mu->endp_type == endp_type_c2c)
      llid_trace_alloc(llid, mu->name, 0, 0, type_llid_trace_endp_c2c);
    else 
    if (mu->endp_type == endp_type_nat)
      llid_trace_alloc(llid, mu->name, 0, 0, type_llid_trace_endp_nat);
    else 
    if (mu->endp_type == endp_type_a2b)
      llid_trace_alloc(llid, mu->name, 0, 0, type_llid_trace_endp_a2b);
    else 
    if (mu->endp_type == endp_type_raw)
      llid_trace_alloc(llid, mu->name, 0, 0, type_llid_trace_endp_raw);
    else
    if (mu->endp_type == endp_type_wif)
      llid_trace_alloc(llid, mu->name, 0, 0, type_llid_trace_endp_wif);
    else
    if (mu->endp_type == endp_type_kvm_eth)
      llid_trace_alloc(llid, mu->name, 0, 0, type_llid_trace_endp_kvm_eth);
    else
    if (mu->endp_type == endp_type_kvm_wlan)
      llid_trace_alloc(llid, mu->name, 0, 0, type_llid_trace_endp_kvm_wlan);
    else
      KOUT("%d", mu->endp_type);
    }
  return llid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_priv_endp *muendp_find_with_name(char *name, int num)
{
  t_priv_endp *cur = NULL;
  if (name[0])
    { 
    cur = g_head_muendp;
    while(cur)
      {
      if ((!strcmp(cur->name, name)) && (cur->num == num))
        break;
      cur = cur->next;
      }
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_priv_endp *muendp_find_with_llid(int llid)
{
  t_priv_endp *cur = g_head_muendp;
  if ((llid <1) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  while(cur && (cur->llid != llid))
    cur = cur->next;
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int endp_mngt_can_be_found_with_llid(int llid, char *name, int *num,
                                     int *endp_type)
{
  t_priv_endp *cur = g_head_muendp;
  memset(name, 0, MAX_NAME_LEN);
  if ((llid <1) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  while(cur && (cur->llid != llid))
    cur = cur->next;
  if (cur)
    {
    strncpy(name, cur->name, MAX_NAME_LEN-1);
    *num = cur->num;
    *endp_type = cur->endp_type;
    return 1;
    }
  else
    return 0; 
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int endp_mngt_can_be_found_with_name(char *name, int num, int *endp_type)
{
  int result = 0;
  t_priv_endp *cur = muendp_find_with_name(name, num);
  *endp_type = 0;
  if (cur)
    {
    if (cur->endp_type == endp_type_kvm_wlan)
      {
      *endp_type = cur->endp_type;
      }
    else
      {
      if (msg_exist_channel(cur->llid))
        {
        result = cur->llid;
        *endp_type = cur->endp_type;
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_priv_endp *muendp_alloc(char *name, int num, int llid, int tid, int endp_type)
{
  t_priv_endp *mu;
  mu = (t_priv_endp *) clownix_malloc(sizeof(t_priv_endp), 4);
  memset(mu, 0, sizeof(t_priv_endp));
  strncpy(mu->name, name, MAX_NAME_LEN-1);
  mu->num = num;
  mu->cli_llid = llid;
  mu->cli_tid = tid;
  mu->endp_type = endp_type;
  if (g_head_muendp)
    g_head_muendp->prev = mu;
  mu->next = g_head_muendp;
  g_head_muendp = mu; 
  g_nb_muendp += 1;
  return mu;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_topo_snf *get_snf(char *name, int num)
{
  t_topo_snf *result = NULL;
  t_priv_endp *mu = muendp_find_with_name(name, num);
  if (!mu)
    KERR("%s %d", name, num);
  else
    {
    if (mu->endp_type != endp_type_snf)
      KERR("%s %d %d", name, num, mu->endp_type);
    else
      result = &(mu->snf);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void endp_mngt_snf_set_name(char *name, int num)
{
  t_topo_snf *snf = get_snf(name, num);
  if (snf)
    strncpy(snf->name, name, MAX_NAME_LEN-1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void endp_mngt_snf_set_capture(char *name, int num, int capture_on)
{
  t_topo_snf *snf = get_snf(name, num);
  if (snf)
    snf->capture_on = capture_on;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void endp_mngt_snf_set_recpath(char *name, int num, char *recpath)
{
  t_topo_snf *snf = get_snf(name, num);
  if (snf)
    {
    strncpy(snf->recpath, recpath, MAX_PATH_LEN-1);
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void endp_mngt_c2c_info(char *name, int num, int local_is_master, 
                        char *master, char *slave, int ip, int port,
                        char *slave_passwd)

{
  t_priv_endp *mu = muendp_find_with_name(name, num);
  if (!mu)
    KERR("%s %d", name, num);
  else
    {
    if (mu->endp_type != endp_type_c2c)
      KERR("%s %d %d", name, num, mu->endp_type);
    else
      {
      strncpy(mu->c2c.name, name, MAX_NAME_LEN-1);
      strncpy(mu->c2c.master_cloonix, master, MAX_NAME_LEN-1);
      strncpy(mu->c2c.slave_cloonix, slave, MAX_NAME_LEN-1);
      memcpy(mu->c2c_passwd_slave, slave_passwd, MSG_DIGEST_LEN);
      mu->c2c.local_is_master = local_is_master;
      mu->c2c.ip_slave = ip;
      mu->c2c.port_slave = port;
      event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void endp_mngt_c2c_peered(char *name, int num, int is_peered)
{
  t_priv_endp *mu = muendp_find_with_name(name, num);
  if (!mu)
    KERR("%s %d", name, num);
  else
    {
    if (mu->endp_type != endp_type_c2c)
      KERR("%s %d %d", name, num, mu->endp_type);
    else
      mu->c2c.is_peered = is_peered;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void endp_mngt_add_attached_lan(int llid, char *name, int num,
                                int tidx, char *lan)
{
  int lan_num;
  t_priv_endp *mu = muendp_find_with_name(name, num);
  if (!mu)
    KERR("%s %d %s", name, num, lan);
  else  
    {
    if (mu->lan_attached[tidx].lan_num != 0)
      KERR("%s %d %s %d", name, num, lan, mu->lan_attached[tidx].lan_num);
    else
      {
      lan_num = lan_add_name(lan, llid);
      if ((lan_num <= 0) || (lan_num >= MAX_LAN))
        KOUT("%d", lan_num);
      mu->lan_attached[tidx].lan_num = lan_num;
      event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void endp_mngt_del_attached_lan(char *name, int num, int tidx, char *lan)
{
  int lan_num, llid, llid_con;
  t_priv_endp *mu = muendp_find_with_name(name, num);
  if (!mu)
    KERR("%s %d %s", name, num, lan);
  else  
    {
    if (mu->lan_attached[tidx].lan_num == 0)
      KERR("%s %d %s", name, num, lan);
    else
      {
      lan_num = lan_del_name(lan);
      if (mu->lan_attached[tidx].lan_num != lan_num)
        KERR("%s %d %s %d %d", name, num, lan, lan_num,
                            mu->lan_attached[tidx].lan_num);
      else
        {
        if (mu->lan_attached[tidx].unix2inet_evt_on)
          {
          llid = mu->lan_attached[tidx].unix2inet_llid;
          llid_con = mu->lan_attached[tidx].unix2inet_llid_con;
          unix2inet_lan_event(llid, llid_con);
          }
        memset(&(mu->lan_attached[tidx]), 0, sizeof(t_lan_attached));
        event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void muendp_free(char *name, int num)
{
  int i, lan_num;
  t_priv_endp *mu = muendp_find_with_name(name, num);
  char *pth = utils_get_endp_path(name, num);
  if (mu)
    {
    for (i=0; i<MAX_TRAF_ENDPOINT; i++)
      {
      lan_num = mu->lan_attached[i].lan_num;
      if (lan_num)
        KOUT("%s", lan_get_with_num(lan_num));
      }
    status_reply_if_possible(0, mu, "ERROR"); 
    if (mu->endp_type == endp_type_c2c)
      c2c_free_ctx(name);
    if (mu->prev)
      mu->prev->next = mu->next;
    if (mu->next)
      mu->next->prev = mu->prev;
    if (mu == g_head_muendp)
      g_head_muendp = mu->next;
    if (mu->llid)
      {
      llid_trace_free(mu->llid, 0, __FUNCTION__);
      }
    if ((num == 0) && (mu->endp_type != endp_type_kvm_eth))
      layout_del_sat(name);
    if (mu->pid)
      kill(mu->pid, SIGKILL);
    clownix_free(mu, __FUNCTION__);
    if (g_nb_muendp <= 0)
      KOUT("%d", g_nb_muendp);
    g_nb_muendp -= 1;
    stats_counters_death(name, num);
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    unlink(pth);
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_c2c_restart(void *data)
{
  t_time_delay *td = (t_time_delay *) data;
  local_add_sat(0, 0, td->name, endp_type_c2c, &(td->c2c_info));
  clownix_free(data, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void arm_restart_c2c(char *name, int num, t_c2c_req_info *c2c_info)
{
  t_time_delay *td;
  td = (t_time_delay *) clownix_malloc(sizeof(t_time_delay), 4);
  memset(td, 0, sizeof(t_time_delay));
  strncpy(td->name, name, MAX_NAME_LEN-1);
  td->num = num;
  memcpy(&td->c2c_info, c2c_info, sizeof(t_c2c_req_info));
  clownix_timeout_add(200, timer_c2c_restart, (void *)td, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void endp_mngt_c2c_disconnect(char *name, int num)
{
  char savname[MAX_NAME_LEN];
  t_c2c_req_info c2c_info;
  t_priv_endp *mu = muendp_find_with_name(name, num);
  memset(savname, 0, MAX_NAME_LEN); 
  memset(&c2c_info, 0, sizeof(t_c2c_req_info));
  if (!mu)
    KERR("%s", name);
  else
    {
    strncpy(savname, name, MAX_NAME_LEN-1);
    strncpy(c2c_info.cloonix_slave, mu->c2c.slave_cloonix, MAX_NAME_LEN-1);
    memcpy(c2c_info.passwd_slave, mu->c2c_passwd_slave, MSG_DIGEST_LEN);
    c2c_info.ip_slave = mu->c2c.ip_slave;
    c2c_info.port_slave = mu->c2c.port_slave;
    endp_evt_quick_death(savname, num);
    muendp_free(savname, num);
    arm_restart_c2c(savname, num, &c2c_info);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void endp_mngt_pid_resp(int llid, char *name, int toppid, int pid)
{
  t_priv_endp *muendp = muendp_find_with_llid(llid);
  if (muendp)
    {
    muendp->unanswered_pid_req = 0;
    if (strcmp(name, muendp->name))
      KERR("%s %s", name, muendp->name);
    if (muendp->pid == 0)
      {
      if (muendp->endp_type == endp_type_snf)
        rpct_send_cli_req(NULL, llid, 0, 0, 0, "-get_conf");
      muendp->pid = pid;
      }
    else
      {
      if (muendp->pid != pid)
        KERR("%s %d %d", name, pid, muendp->pid);
      }
    }
  else
    KERR("%s %d", name, pid);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int llid_flow_to_restrict(char *name, int num, int tidx)
{
  int llid = mulan_can_be_found_with_name(name);
  KERR("TO RESTRICT: name:%s num:%d tidx:%d llid:%d", name, num, tidx, llid);
  return llid;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void fill_eventfull(t_priv_endp *mu, int tidx, int ms, 
                           int ptx, int prx, int btx, int brx)
{
  mu->lan_attached[tidx].eventfull_tx_p += ptx;
  mu->lan_attached[tidx].eventfull_rx_p += prx;
  mu->lan_attached[tidx].eventfull_tx_b += btx;
  mu->lan_attached[tidx].eventfull_rx_b += brx;
  mu->lan_attached[tidx].eventfull_ms = ms;
  stats_counters_update_endp_tx_rx(mu->name,mu->num,ms,ptx,btx,prx,brx);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void endp_mngt_rpct_recv_evt_msg(int llid, int tid, char *line)
{
  int num, tidx, peer_llid, ptx, btx, prx, brx, rank, stop;
  unsigned int ms;
  t_priv_endp *mu = muendp_find_with_llid(llid);
  char name[MAX_NAME_LEN];
  if (mu)
    {
    if (sscanf(line, 
        "cloonix_evt_peer_flow_control name=%s num=%d tidx=%d rank=%d stop=%d",
         name, &num, &tidx, &rank, &stop) == 5)
      {
      peer_llid = llid_flow_to_restrict(name, num, tidx);
      if (peer_llid)
        murpc_dispatch_send_tx_flow_control(peer_llid, rank, stop);
      else
        KERR("%s %d", name, num);
      }
    else if (sscanf(line,"endp_eventfull_tx_rx %u %d %d %d %d %d",
                         &ms, &tidx, &ptx, &btx, &prx, &brx) == 6)
      {
      fill_eventfull(mu, tidx, ms, ptx, prx, btx, brx);
      }
    else
      KERR("%s", line);
    }
  else
    KERR("%s", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void endp_mngt_wlan_eventfull(char *name, int num, int tidx, char *wlan, 
                              int ms, int ptx, int btx, int prx, int brx)
{
  t_priv_endp *mu = muendp_find_with_name(name, num);
  if (!mu)
    KERR("%u name:%s num:%d tidx:%d wlan:%s %d %d %d %d",
                  ms, name, num, tidx, wlan, ptx, btx, prx, brx);
  else
    {
    fill_eventfull(mu, tidx, ms, ptx, prx, btx, brx);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void endp_mngt_rpct_recv_diag_msg(int llid, int tid, char *line)
{
  t_priv_endp *mu = muendp_find_with_llid(llid);
  char name[MAX_NAME_LEN];
  char lan[MAX_NAME_LEN];
  int num, tidx, rank;
  if (mu)
    {
    if (!strcmp(line, "cloonix_resp_suidroot_ok"))
      mu->getsuidroot = 1;
    else if (!strcmp(line, "cloonix_resp_suidroot_ko"))
      {
      mu->getsuidroot = 1;
      mu->open_endp = 1;
      status_reply_if_possible(0, mu, 
      "\"sudo chmod u+s /usr/local/bin/cloonix"
      "/server/muswitch/mutap/cloonix_mutap\"");

      endp_mngt_send_quit(mu->name, mu->num);
      }
    else if ((!strcmp(line, "cloonix_resp_tap_ok"))  ||
             (!strcmp(line, "cloonix_resp_wif_ok"))  ||
             (!strcmp(line, "cloonix_resp_raw_ok"))  ||
             (!strcmp(line, "cloonix_resp_snf_ok"))  ||
             (!strcmp(line, "cloonix_resp_c2c_ok"))  ||
             (!strcmp(line, "cloonix_resp_nat_ok"))  ||
             (!strcmp(line, "cloonix_resp_kvm_eth_ok"))  ||
             (!strcmp(line, "cloonix_resp_a2b_ok")))
      {
      mu->open_endp = 1;
      endp_evt_birth(mu->name, mu->num, mu->endp_type);
      event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
      status_reply_if_possible(1, mu, "OK"); 
      }
    else if ((!strcmp(line, "cloonix_resp_tap_ko"))  ||
             (!strcmp(line, "cloonix_resp_wif_ko"))  ||
             (!strcmp(line, "cloonix_resp_raw_ko")))
      {
      mu->open_endp = 1;
      status_reply_if_possible(0, mu, line);
      endp_mngt_send_quit(mu->name, mu->num);
      }
    else if (sscanf(line, 
             "cloonix_resp_connect_ok lan=%s name=%s num=%d tidx=%d rank=%d",
             lan, name, &num, &tidx, &rank) == 5)
      {
      if (strcmp(name, mu->name))
        KERR("%s %s", name, mu->name);
      if (strcmp(mu->waiting_resp_txt, "unix_sock"))
        KERR("%s %d %d %s", mu->name, tid, mu->pid, line);
      endp_evt_connect_OK(name, num, lan, tidx, rank);
      }
    else if  (sscanf(line,
              "cloonix_resp_disconnect_ok lan=%s name=%s num=%d tidx=%d",
              lan, name, &num, &tidx) == 4)
      {
      if (strcmp(name, mu->name))
        KERR("%s %s", name, mu->name);
      if (strcmp(mu->waiting_resp_txt, "disconnect"))
        KERR("%s %d %d %s", mu->name, tid, mu->pid, line);
      }
    else if  (sscanf(line,
              "cloonix_resp_connect_ko lan=%s name=%s num=%d tidx=%d", 
              lan, name, &num, &tidx) == 4)
      {
      if (strcmp(name, mu->name))
        KERR("%s %s", name, mu->name);
      if (strcmp(mu->waiting_resp_txt, "unix_sock"))
        KERR("%s %d %d %s", mu->name, tid, mu->pid, line);
      endp_evt_connect_KO(name, num, lan, tidx);
      }
    else
      KERR("%s %s", mu->name, line);
    mu->waiting_resp = 0;
    memset(mu->waiting_resp_txt, 0, MAX_NAME_LEN);
    }
  else
    KERR("%s", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void endp_mngt_err_cb (int llid)
{
  t_priv_endp *mu = muendp_find_with_llid(llid);
  if (mu)
    {
    status_reply_if_possible(0, mu, "llid_err");
    endp_evt_quick_death(mu->name, mu->num);
    muendp_free(mu->name, mu->num);
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void send_type_req(t_priv_endp *cur)
{
  char msg_info[MAX_PATH_LEN];
  memset(msg_info, 0, MAX_PATH_LEN);
  if (cur->endp_type == endp_type_tap)
    try_send_endp(cur, "cloonix_req_tap");
  else if (cur->endp_type == endp_type_wif)
    try_send_endp(cur, "cloonix_req_wif");
  else if (cur->endp_type == endp_type_raw)
    try_send_endp(cur, "cloonix_req_raw");
  else if (cur->endp_type == endp_type_snf)
    try_send_endp(cur, "cloonix_req_snf");
  else if (cur->endp_type == endp_type_c2c)
    try_send_endp(cur, "cloonix_req_c2c");
  else if (cur->endp_type == endp_type_a2b)
    try_send_endp(cur, "cloonix_req_a2b");
  else if (cur->endp_type == endp_type_nat)
    try_send_endp(cur, "cloonix_req_nat");
  else if (cur->endp_type == endp_type_kvm_eth)
    try_send_endp(cur, "cloonix_req_kvm_eth");
  else
    KERR("%d", cur->endp_type);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void set_clone_start_pid(char *name, t_priv_endp *endp, int pid)
{
  if (!pid)
    KERR("BADPID");
  if (endp->clone_start_pid)
    KERR("CLONEPID %s %d %d", name, pid, endp->clone_start_pid);
  endp->clone_start_pid = pid;
  endp->clone_start_pid_just_done = 1;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_endp_beat(void *data)
{
  char line[2*MAX_PATH_LEN];
  t_priv_endp *next, *cur = g_head_muendp;
  while(cur)
    {
    next = cur->next;
    if (cur->clone_start_pid)
      {
      if (cur->clone_start_pid_just_done == 1)
        cur->clone_start_pid_just_done = 0;
      else
        {
        if (cur->trace_alloc_count == 0)
          cur->trace_alloc_count = 1;
        else if (cur->llid == 0)
          {
          cur->llid = trace_alloc(cur);
          cur->trace_alloc_count += 1;
          if (cur->trace_alloc_count == 100)
            KERR("ENDP %s %d WARN 1 NOT LISTENING", cur->name, cur->num);
          if (cur->trace_alloc_count == 200)
            KERR("ENDP %s %d WARN 2 NOT LISTENING", cur->name, cur->num);
          if (cur->trace_alloc_count > 300)
            {
            KERR("ENDP %s %d ERR NOT LISTENING", cur->name, cur->num);
            endp_mngt_send_quit(cur->name, cur->num);
            }
          }
        else if (cur->pid == 0) 
          rpct_send_pid_req(NULL,cur->llid,type_hop_endp,cur->name,cur->num);
        else if ((cur->getsuidroot == 0) && 
                 ((cur->endp_type == endp_type_tap) ||
                  (cur->endp_type == endp_type_raw) ||
                  (cur->endp_type == endp_type_wif)))
          try_send_endp(cur, "cloonix_req_suidroot");
        else if (cur->open_endp == 0)
          {
          if (cur->open_endp_req_sent)
            KERR("ENDP %s %d", cur->name, cur->num);
          else
            {
            cur->open_endp_req_sent = 1;
            send_type_req(cur);
            }
          }
        else
          {
          if ((cur->endp_type == endp_type_snf) &&
              (cur->init_pcap_snf == 0))
            {
            snprintf(line, 2*MAX_PATH_LEN - 1, "-set_conf %s/%s.pcap",
                     utils_get_snf_pcap_dir(), cur->name);
            rpct_send_cli_req(NULL, cur->llid, 0, 0, 0, line);
            cur->init_pcap_snf = 1;
            }
          cur->periodic_count += 1;
          if (cur->periodic_count >= 5)
            {
            rpct_send_pid_req(NULL,cur->llid,type_hop_endp,
                              cur->name,cur->num);
            cur->periodic_count = 1;
            cur->unanswered_pid_req += 1;
            if (cur->unanswered_pid_req > 2)
              {
              KERR("ENDP %s %d NOT RESPONDING KILLING IT",cur->name,cur->num);
              endp_mngt_send_quit(cur->name, cur->num);
              }
            }
          }
        }
      }
    cur = next;
    }
  clownix_timeout_add(10, timer_endp_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void muendp_watchdog(void *data)
{
  t_argendp *mua = (t_argendp *) data;
  t_priv_endp *muendp = muendp_find_with_name(mua->name, mua->num);
  if (muendp && ((!muendp->llid) || (!muendp->pid)))
    {
    status_reply_if_possible(0, muendp, "timeout");
    KERR("%s", muendp->name);
    endp_evt_quick_death(muendp->name, muendp->num);
    muendp_free(muendp->name, muendp->num);
    }
  clownix_free(mua, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void doors_recv_c2c_clone_death(int llid, int tid, char *name)
{
  t_priv_endp *muendp = muendp_find_with_name(name, 0);
  event_print("End doors muendp %s", name);
  if (muendp)
    {
    status_reply_if_possible(0, muendp, "death");
    endp_evt_quick_death(muendp->name, muendp->num);
    muendp_free(muendp->name, muendp->num);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void muendp_death(void *data, int status, char *name)
{
  t_argendp *mua = (t_argendp *) data;
  t_priv_endp *muendp = muendp_find_with_name(mua->name, mua->num);
  if (strcmp(name, mua->name))
    KOUT("%s %s", name, mua->name);
  if (muendp)
    {
    event_print("End muendp %s %d", name, muendp->num);
    status_reply_if_possible(0, muendp, "death");
    endp_evt_quick_death(muendp->name, muendp->num);
    muendp_free(muendp->name, muendp->num);
    }
  clownix_free(mua, __FUNCTION__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static char **muendp_birth_argv(t_argendp *mu)
{
  static char endp_type[MAX_NAME_LEN];
  static char net_name[MAX_NAME_LEN];
  static char name[MAX_NAME_LEN];
  static char bin_path[MAX_PATH_LEN];
  static char sock[MAX_PATH_LEN];
  static char *argv[] = {bin_path, net_name, name, sock, endp_type, NULL};
  memset(endp_type, 0, MAX_NAME_LEN);
  memset(net_name, 0, MAX_NAME_LEN);
  memset(name, 0, MAX_NAME_LEN);
  memset(bin_path, 0, MAX_PATH_LEN);
  memset(sock, 0, MAX_PATH_LEN);
  snprintf(endp_type, MAX_NAME_LEN, "%d", mu->endp_type);
  snprintf(net_name, MAX_NAME_LEN, "%s", mu->net_name);
  snprintf(name, MAX_NAME_LEN, "%s", mu->name);
  snprintf(bin_path, MAX_PATH_LEN, "%s", mu->bin_path);
  snprintf(sock, MAX_PATH_LEN, "%s", mu->sock);
  endp_type[MAX_NAME_LEN-1] = 0;
  net_name[MAX_NAME_LEN-1] = 0;
  name[MAX_NAME_LEN-1] = 0;
  bin_path[MAX_PATH_LEN-1] = 0;
  sock[MAX_PATH_LEN-1] = 0;
  return argv;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int muendp_birth(void *data)
{
  t_argendp *mu = (t_argendp *) data;
  char **argv = muendp_birth_argv(mu);

//VIP
//sleep(1000000);

  execv(mu->bin_path, argv);
  return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void create_two_endp_arg(char *name, int num, int endp_type,
                                 t_argendp **mua1, t_argendp **mua2)
{
  char *bin_path = utils_get_endp_bin_path(endp_type);
  *mua1 = (t_argendp *) clownix_malloc(sizeof(t_argendp), 4);
  *mua2 = (t_argendp *) clownix_malloc(sizeof(t_argendp), 4);
  memset(*mua1, 0, sizeof(t_argendp));
  memset(*mua2, 0, sizeof(t_argendp));
  strncpy((*mua1)->net_name, cfg_get_cloonix_name(), MAX_NAME_LEN-1);
  strncpy((*mua2)->net_name, cfg_get_cloonix_name(), MAX_NAME_LEN-1);
  strncpy((*mua1)->name, name, MAX_NAME_LEN-1);
  strncpy((*mua2)->name, name, MAX_NAME_LEN-1);
  strncpy((*mua1)->bin_path, bin_path, MAX_PATH_LEN-1);
  strncpy((*mua2)->bin_path, bin_path, MAX_PATH_LEN-1);
  strncpy((*mua1)->sock, utils_get_endp_path(name, num), MAX_PATH_LEN-1);
  strncpy((*mua2)->sock, utils_get_endp_path(name, num), MAX_PATH_LEN-1);
  (*mua1)->endp_type = endp_type;
  (*mua2)->endp_type = endp_type;
  (*mua1)->num = num;
  (*mua2)->num = num;
  if (!file_exists(bin_path, X_OK))
    KERR("%s Does not exist or not exec", bin_path);

}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int endp_mngt_connection_state_is_restfull(char *name, int num)
{
  int result = 0;
  t_priv_endp *mu = muendp_find_with_name(name, num);
  if ((mu) && (mu->waiting_resp == 0))
    result = 1;
  else if (mu)
    KERR("%s %s", name, mu->waiting_resp_txt);
  else
    KERR("%s", name);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void doors_recv_c2c_clone_birth_pid(int llid, int tid, char *name, int pid)
{
  t_priv_endp *mu = muendp_find_with_name(name, 0);
  if (!mu)
    KERR("%s %d", name, pid);
  else
    {
    if (mu->clone_start_pid)
      KERR("%s %d %d", name, pid, mu->clone_start_pid);
    set_clone_start_pid(name, mu, pid);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fd_ready_doors_clone_has_arrived(char *name, int doors_fd)
{
  int result = -1;
  t_priv_endp *mu = muendp_find_with_name(name, 0);
  if (mu)
    {
    if (mu->endp_type != endp_type_c2c)
      KERR("%s", name);
    if (!mu->doors_fd_ready)
      {
      mu->doors_fd_ready = 1;
      mu->doors_fd_value = doors_fd;
      result = 0;
      }
    }
  else
    KERR("%s %d", name, doors_fd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void fd_ready_doors_clone(void *data)
{
  t_argendp  *mua2 = (t_argendp  *) data;
  t_priv_endp *mu = muendp_find_with_name(mua2->name, 0);
  if (mu)
    {
    if (mu->endp_type != endp_type_c2c)
      KERR("%s", mu->name);
    if (mu->doors_fd_ready)
      {
      doors_send_c2c_clone_birth(get_doorways_llid(), 0, mua2->net_name,  
                                 mua2->name, mu->doors_fd_value, 
                                 mua2->endp_type, mua2->bin_path, 
                                 mua2->sock);
      clownix_free(mua2, __FUNCTION__);
      }
    else
      {
      clownix_timeout_add(10,fd_ready_doors_clone,(void *)mua2,NULL,NULL);
      }
    }
  else
    {
    KERR("Musat %s has disapeared", mua2->name);
    clownix_free(mua2, __FUNCTION__);
    }
}


/****************************************************************************/
int endp_mngt_start(int llid, int tid, char *name, int num, int endp_type)
{
  int pid, nb_eth, result = -1;
  char *sock = utils_get_endp_path(name, num);
  t_priv_endp *mu = muendp_find_with_name(name, num);
  t_priv_endp *mu0;
  t_argendp  *mua1, *mua2;
  char **argv;
  if (mu == NULL)
    {
    result = 0;
    if (file_exists(sock, F_OK))
      unlink(sock);
    mu = muendp_alloc(name, num, llid, tid, endp_type);
    if (!mu)
      KOUT(" ");

    if ((num == 0) && (mu->endp_type != endp_type_kvm_eth))
      layout_add_sat(name, llid);

    if (mu->endp_type == endp_type_c2c)
      {
      create_two_endp_arg(name, num, endp_type, &mua1, &mua2);
      clownix_free(mua1, __FUNCTION__);
      clownix_timeout_add(10,fd_ready_doors_clone,(void *)mua2,NULL,NULL);
      }
    else if  ((num == 0) &&
              ((mu->endp_type == endp_type_snf) ||
               (mu->endp_type == endp_type_tap) ||
               (mu->endp_type == endp_type_wif) ||
               (mu->endp_type == endp_type_raw) ||
               (mu->endp_type == endp_type_a2b) ||
               (mu->endp_type == endp_type_nat)))
      {
      create_two_endp_arg(name, num, endp_type, &mua1, &mua2);
      argv = muendp_birth_argv(mua2);
      utils_send_creation_info("muendp", argv);
      pid = pid_clone_launch(muendp_birth, muendp_death, NULL,
                             mua2, mua2, NULL, name, -1, 1);
      set_clone_start_pid(name, mu, pid);
      clownix_timeout_add(500, muendp_watchdog, (void *) mua1, NULL, NULL);
      }
    else if ((num == 1) && (mu->endp_type == endp_type_a2b))
      {
      mu0 = muendp_find_with_name(name, 0);
      if (!mu0) 
        KERR("%s %d", name, num);
      else
        set_clone_start_pid(name, mu, mu0->clone_start_pid);
      }
    else if (mu->endp_type == endp_type_kvm_wlan)
      {
      if (endp_is_wlan(mu->name, mu->num, &nb_eth))
        endp_evt_birth(mu->name, mu->num, mu->endp_type);
      else
        KERR("%s %d %d", mu->name, mu->num, mu->endp_type);
      }
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_lan_connect(void *data)
{
  char cmd[2*MAX_PATH_LEN];
  t_lan_connect *mc = ( t_lan_connect *) data;
  t_priv_endp *mu;
  if (!mc)
    KOUT(" ");
  mu = muendp_find_with_name(mc->name, mc->num);
  if (!mu)
    KERR("%s %s", mc->name, mc->lan);
  else
    {
    memset(cmd, 0, 2*MAX_PATH_LEN);
    snprintf(cmd, 2*MAX_PATH_LEN-1, 
             "cloonix_req_connect sock=%s lan=%s name=%s num=%d tidx=%d",
             mc->lan_sock, mc->lan, mc->name, mc->num, mc->tidx);
    if (try_send_endp(mu, cmd))
      {
      mu->waiting_resp = 0;
      memset(mu->waiting_resp_txt, 0, MAX_NAME_LEN);
      KERR("%s %s %s",mc->name, mc->lan, mc->lan_sock);
      }
    }
  clownix_free(data, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int endp_mngt_lan_connect(int delay, char *name, int num, int tidx, char *lan)
{
  int result = -1;
  t_lan_connect *mc;
  t_priv_endp *mu = muendp_find_with_name(name, num);
  char *lan_sock = utils_mulan_get_sock_path(lan);
  if (mu && (lan[0]) && (lan_sock[0]) && (mu->waiting_resp == 0))
    {
    result = 0;
    mc=(t_lan_connect *)clownix_malloc(sizeof(t_lan_connect), 5); 
    memset(mc, 0, sizeof(t_lan_connect));
    strncpy(mc->name, name, MAX_NAME_LEN-1);
    mc->num = num;
    mc->tidx = tidx;
    strncpy(mc->lan, lan, MAX_NAME_LEN-1);
    strncpy(mc->lan_sock, lan_sock, MAX_PATH_LEN-1);
    mu->waiting_resp = 1;
    strcpy(mu->waiting_resp_txt, "unix_sock");
    clownix_timeout_add(delay,timer_lan_connect,(void *)mc,NULL,NULL);
    }
  else
    KERR("%s %s %s %d %s", name, lan, lan_sock, 
                           mu->waiting_resp, mu->waiting_resp_txt);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int endp_mngt_lan_disconnect(char *name, int num, int tidx, char *lan)
{
  int result = -1;
  char cmd[MAX_PATH_LEN];
  t_priv_endp *mu = muendp_find_with_name(name, num);
  char *lan_sock = utils_mulan_get_sock_path(lan);
  if (mu && (lan[0]) && (lan_sock[0]) && (mu->waiting_resp == 0))
    {
    memset(cmd, 0, MAX_PATH_LEN);
    snprintf(cmd, MAX_PATH_LEN-1, 
             "cloonix_req_disconnect lan=%s name=%s num=%d tidx=%d",
             lan, name, num, tidx);
    result = try_send_endp(mu, cmd);
    if (!result)
      {
      mu->waiting_resp = 1;
      strcpy(mu->waiting_resp_txt, "disconnect");
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_endp_free(void *data)
{
  t_time_delay *td = (t_time_delay *) data;
  endp_evt_quick_death(td->name, td->num);
  muendp_free(td->name, td->num);
  clownix_free(data, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void endp_mngt_send_quit(char *name, int num)
{
  t_priv_endp *mu = muendp_find_with_name(name, num);
  t_time_delay *td;
  if (mu)
    {
    td = (t_time_delay *) clownix_malloc(sizeof(t_time_delay), 4); 
    memset(td, 0, sizeof(t_time_delay));
    strncpy(td->name, name, MAX_NAME_LEN-1);
    td->num = num;
    try_send_endp(mu, "cloonix_req_quit");
    mu->waiting_resp = 1;
    strcpy(mu->waiting_resp_txt, "quit");
    clownix_timeout_add(50, timer_endp_free, (void *)td, NULL, NULL);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int endp_mngt_exists(char *name, int num, int *endp_type)
{
  int result = 0;
  t_priv_endp *mu = muendp_find_with_name(name, num);
  *endp_type = 0;
  if (mu)
    {
    *endp_type = mu->endp_type;
    result = 1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int endp_mngt_stop(char *name, int num)
{
  int result = -1;
  t_priv_endp *mu = muendp_find_with_name(name, num);
  if ((mu) && (!mu->muendp_stop_done))
    {
    mu->muendp_stop_done = 1;
    result = 0;
    if (endp_evt_death(name, num))
      muendp_free(name, num);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void endp_mngt_stop_all_sat(void)
{
  t_priv_endp *next, *cur = g_head_muendp;
  while(cur)
    {
    next = cur->next;
    if ((cur->endp_type == endp_type_tap) ||
        (cur->endp_type == endp_type_wif) ||
        (cur->endp_type == endp_type_raw) ||
        (cur->endp_type == endp_type_a2b) ||
        (cur->endp_type == endp_type_snf) ||
        (cur->endp_type == endp_type_nat))
      {
      endp_mngt_stop(cur->name, cur->num);
      }
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int endp_mngt_get_all_llid(int32_t **llid_tab)
{
  t_priv_endp *cur = g_head_muendp;
  int i, result = 0;
  *llid_tab = NULL;
  while(cur)
    {
    if (cur->llid)
      result++;
    cur = cur->next;
    }
  if (result)
    {
    *llid_tab = (int *)clownix_malloc(result * sizeof(int64_t), 5);
    memset((*llid_tab), 0, result * sizeof(int64_t));
    cur = g_head_muendp;
    i = 0;
    while(cur)
      {
      if (cur->llid)
        (*llid_tab)[i++] = cur->llid;
      cur = cur->next;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int endp_mngt_get_all_pid(t_lst_pid **lst_pid)
{
  t_lst_pid *glob_lst = NULL;
  t_priv_endp *cur = g_head_muendp;
  int i, result = 0;
  event_print("%s", __FUNCTION__);
  while(cur)
    {
    if (cur->pid)
      result++;
    cur = cur->next;
    }
  if (result)
    {
    glob_lst = (t_lst_pid *)clownix_malloc(result*sizeof(t_lst_pid),5);
    memset(glob_lst, 0, result*sizeof(t_lst_pid));
    cur = g_head_muendp;
    i = 0;
    while(cur)
      {
      if (cur->pid)
        {
        strncpy(glob_lst[i].name, cur->name, MAX_NAME_LEN-1);
        glob_lst[i].pid = cur->pid;
        i++;
        }
      cur = cur->next;
      }
    if (i != result)
      KOUT("%d %d", i, result);
    }
  *lst_pid = glob_lst;
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static t_endp *fill_endp(t_priv_endp *mu)
{
  t_endp *endp = (t_endp *) clownix_malloc(sizeof(t_endp), 11);
  memset(endp, 0, sizeof(t_endp));
  strcpy(endp->name, mu->name);
  endp->num = mu->num;
  endp->pid = mu->pid;
  endp->endp_type = mu->endp_type;
  memcpy(&(endp->c2c), &(mu->c2c), sizeof(t_topo_c2c));
  memcpy(&(endp->snf), &(mu->snf), sizeof(t_topo_snf));
  memcpy(endp->lan_attached, mu->lan_attached, 
         MAX_TRAF_ENDPOINT * sizeof(t_lan_attached));
  endp->next = (void *) mu->next;
  return endp;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
t_endp *endp_mngt_get_first(int *nb_endp)
{
  t_priv_endp *mu = g_head_muendp;
  t_endp *result = NULL;
  if (mu)
    result = fill_endp(mu);
  *nb_endp = g_nb_muendp;
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
t_endp *endp_mngt_get_next(t_endp *endp)
{
  t_priv_endp *mu = (t_priv_endp *) endp->next;
  t_endp *result = NULL;
  if (mu)
    result = fill_endp(mu);
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int endp_mngt_get_nb(int type)
{
  t_priv_endp *cur = g_head_muendp;
  int result = 0;
  while(cur)
    {
    if ((cur->endp_type == type) && (cur->num == 0))
      result++;
    cur = cur->next;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int endp_mngt_get_nb_sat(void)
{
  t_priv_endp *cur = g_head_muendp;
  int result = 0;
  while(cur)
    {
    if (cur->num == 0)
      {
      if ((cur->endp_type == endp_type_tap) ||
          (cur->endp_type == endp_type_wif) ||
          (cur->endp_type == endp_type_raw) ||
          (cur->endp_type == endp_type_a2b) ||
          (cur->endp_type == endp_type_nat))
        result++;
      }
    cur = cur->next;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int endp_mngt_get_nb_all(void)
{
  return g_nb_muendp;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int endp_mngt_kvm_pid_clone(char *name, int num, int pid)
{
  int result = -1;
  t_priv_endp *mu = muendp_find_with_name(name, num);
  if (!mu)
    KERR("%s %d", name, num);
  else 
    {
    if (mu->endp_type != endp_type_kvm_eth)
      KERR("%s %d %d", name, num, mu->endp_type);
    else
      {
      set_clone_start_pid(name, mu, pid);
      result = 0;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void endp_mngt_erase_eventfull_stats(void)
{
  int i;
  t_priv_endp *cur = g_head_muendp;
  while(cur)
    {
    for (i=0; i<MAX_TRAF_ENDPOINT; i++)
      {
      cur->lan_attached[i].eventfull_rx_p = 0;
      cur->lan_attached[i].eventfull_tx_p = 0;
      cur->lan_attached[i].eventfull_rx_b = 0;
      cur->lan_attached[i].eventfull_tx_b = 0;
      }

    cur = cur->next;
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int endp_mngt_get_all_lan(char *name, int nb_num, int *nb_lan, int *lan)
{
  int i, j, result = -1;
  t_priv_endp *endp;
  *nb_lan = 0;
  for (i=0; i<nb_num; i++)
    {
    endp = muendp_find_with_name(name, i);
    if (endp)
      {
      result = 0;
      for (j=0; j<MAX_TRAF_ENDPOINT; j++)
        {
        if (endp->lan_attached[j].lan_num)
          {
          lan[*nb_lan] = endp->lan_attached[j].lan_num;
          *nb_lan += 1;
          }
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int endp_mngt_set_evt(char *name, int nb_num, int lan, int llid, int llid_con)
{
  int i, j, result = -1;
  t_priv_endp *endp;
  for (i=0; i<nb_num; i++)
    {
    endp = muendp_find_with_name(name, i);
    if (endp)
      {
      for (j=0; j<MAX_TRAF_ENDPOINT; j++)
        {
        if (endp->lan_attached[j].lan_num == lan)
          {
          endp->lan_attached[j].unix2inet_evt_on = 1;
          endp->lan_attached[j].unix2inet_llid = llid;
          endp->lan_attached[j].unix2inet_llid_con = llid_con;
          result = 0;
          }
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void muendp_wlan_death(char *name, int num)
{
  t_priv_endp *muendp = muendp_find_with_name(name, num);
  if (muendp)
    {
    endp_evt_quick_death(muendp->name, muendp->num);
    muendp_free(muendp->name, muendp->num);
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void endp_mngt_init(void)
{
  g_head_muendp = NULL;
  g_nb_muendp = 0;
  endp_evt_init();
  clownix_timeout_add(10, timer_endp_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

