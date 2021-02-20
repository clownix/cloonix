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
#include <libgen.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_daemon.h"
#include "event_subscriber.h"
#include "cfg_store.h"
#include "pid_clone.h"
#include "utils_cmd_line_maker.h"
#include "system_callers.h"
#include "llid_trace.h"
#include "file_read_write.h"
#include "lan_to_name.h"
#include "endp_mngt.h"
#include "endp_evt.h"
#include "automates.h"
#include "hop_event.h"
#include "murpc_dispatch.h"
#include "uml_clownix_switch.h"


enum {
  traffic_lan_link_idle = 0,
  traffic_lan_link_wait,
  traffic_lan_link_done,
};

/****************************************************************************/
typedef struct t_zombie_kill
{
  char lan[MAX_NAME_LEN];
  int pid_to_kill;
  int count;
  struct t_zombie_kill *prev;
  struct t_zombie_kill *next;
} t_zombie_kill;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
typedef struct t_wlan_name_num
{
  char name[MAX_NAME_LEN];
  int  num;
  struct t_wlan_name_num *prev;
  struct t_wlan_name_num *next;
} t_wlan_name_num;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
typedef struct t_mulan
{
  char lan[MAX_NAME_LEN];
  char sock[MAX_PATH_LEN];
  char traf[MAX_PATH_LEN];
  int llid;
  int clone_start_pid;
  int pid;
  int traffic_lan_link_state;
  int periodic_count;
  int unanswered_pid_req;
  int is_wlan;
  t_wlan_name_num *head_wlan_name_num;
  struct t_mulan *prev;
  struct t_mulan *next;
} t_mulan;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
typedef struct t_mulan_arg
{
  char lan[MAX_NAME_LEN];
  char sock[MAX_PATH_LEN];
} t_mulan_arg;
/*--------------------------------------------------------------------------*/

static t_mulan *g_head_mulan;
static t_zombie_kill *g_head_zombie;

/****************************************************************************/
static t_wlan_name_num *wlan_name_num_find(t_mulan *mu, char *name, int num)
{
  t_wlan_name_num *cur = mu->head_wlan_name_num;
  while(cur)
    {
    if (!strcmp(cur->name, name) && (cur->num == num))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void wlan_name_num_alloc(t_mulan *mu, char *name, int num)
{
  t_wlan_name_num *cur = wlan_name_num_find(mu, name, num);
  if (cur)
    KERR("%s %s %d", mu->lan, name, num);
  else
    {
    cur = (t_wlan_name_num *)malloc(sizeof(t_wlan_name_num));
    memset(cur, 0, sizeof(t_wlan_name_num));
    strncpy(cur->name, name, MAX_NAME_LEN-1);
    cur->num = num;
    if (mu->head_wlan_name_num)
      mu->head_wlan_name_num->prev = cur;
    cur->next = mu->head_wlan_name_num;
    mu->head_wlan_name_num = cur;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void wlan_name_num_free(t_mulan *mu, char *name, int num)
{
  t_wlan_name_num *cur = wlan_name_num_find(mu, name, num);
  if (!cur)
    KERR("%s %s %d", mu->lan, name, num);
  else
    {
    if (cur->prev) 
      cur->prev->next = cur->next;
    if (cur->next) 
      cur->next->prev = cur->prev;
    if (cur == mu->head_wlan_name_num) 
      mu->head_wlan_name_num = cur->next;
    free(cur);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_mulan *get_name_num(char *name, int num)
{
  t_mulan *cur = g_head_mulan;
  while(cur)
    {
    if (wlan_name_num_find(cur, name, num))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int try_rpct_send_diag_msg(int llid, int pid, char *msg)
{
  int result = -1;
  if (llid)
    {
    if (msg_exist_channel(llid))
      {
      hop_event_hook(llid, FLAG_HOP_DIAG, msg);
      rpct_send_diag_msg(NULL, llid, pid, msg);
      result = 0;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_mulan *mulan_find_with_name(char *lan)
{
  t_mulan *cur = g_head_mulan;
  while(cur && (strcmp(cur->lan, lan)))
    cur = cur->next;
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int mulan_exists(char *lan, int *is_wlan)
{
  int result;
  t_mulan *cur = mulan_find_with_name(lan);
  if (cur)
    {
    *is_wlan = cur->is_wlan;
    result = 1;
    }
  else
    {
    *is_wlan = 0;
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_zombie_kill(void *data)
{
  t_zombie_kill *zk = (t_zombie_kill *) data;
  int is_wlan;
  if (mulan_exists(zk->lan, &is_wlan))
    {
    clownix_timeout_add(500, timer_zombie_kill, (void *) zk, NULL, NULL);
    zk->count += 1;
    if (zk->count > 3)
      {
      if (!zk->pid_to_kill)
        KERR("%s", zk->lan);
      else
        {
        if (!kill(zk->pid_to_kill, SIGTERM))
          KERR("Emergency SIGTERM kill for %s", zk->lan);
        else
          KERR("Fail Emergency SIGTERM kill for %s", zk->lan);
        }
      }
    }
  else
    {
    if (zk->next)
      zk->next->prev = zk->prev;
    if (zk->prev)
      zk->prev->next = zk->next;
    if (zk == g_head_zombie)
      g_head_zombie = zk->next;
    clownix_free(zk, __FUNCTION__);
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static t_zombie_kill *zombie_find_with_name(char *lan)
{
  t_zombie_kill *cur = g_head_zombie;
  while(cur && (strcmp(cur->lan, lan)))
    cur = cur->next;
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void trigger_zombie_kill(char *lan, int llid, int pid_to_kill)
{
  t_zombie_kill *zk = zombie_find_with_name(lan);
  if (zk)
    {
    if (zk->pid_to_kill != pid_to_kill)
      KERR("%s %d %d", lan, zk->pid_to_kill, pid_to_kill);
    }
  else
    {
    endp_evt_mulan_death(lan);
    try_rpct_send_diag_msg(llid, pid_to_kill, "cloonix_req_quit");
    zk = (t_zombie_kill *) clownix_malloc(sizeof(t_zombie_kill), 4);
    memset(zk, 0,  sizeof(t_zombie_kill));
    strncpy(zk->lan, lan, MAX_NAME_LEN-1);
    zk->pid_to_kill = pid_to_kill;
    if (g_head_zombie)
      g_head_zombie->prev = zk;
    zk->next = g_head_zombie;
    g_head_zombie = zk;
    clownix_timeout_add(500, timer_zombie_kill, (void *) zk, NULL, NULL);
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void trace_alloc(t_mulan *mulan)
{
  char *sock = mulan->sock;
  char *lan = mulan->lan;
  int llid;
  llid = string_client_unix(sock, uml_clownix_switch_error_cb, 
                                  uml_clownix_switch_rx_cb, "mulan");
  if (llid)
    {
    mulan->llid = llid;
    if (hop_event_alloc(llid, type_hop_mulan, lan, 0))
       KERR("BAD HOP CONNECT %s", lan);
    llid_trace_alloc(llid, lan, 0, 0, type_llid_trace_mulan);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_mulan *mulan_find_with_llid(int llid)
{
  t_mulan *cur = g_head_mulan;
  if ((llid <1) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  while(cur && (cur->llid != llid))
    cur = cur->next;
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int mulan_can_be_found_with_llid(int llid, char *lan)
{
  t_mulan *cur = g_head_mulan;
  memset(lan, 0, MAX_NAME_LEN);
  if ((llid <1) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  while(cur && (cur->llid != llid))
    cur = cur->next;
  if (cur)
    {
    strncpy(lan, cur->lan, MAX_NAME_LEN-1); 
    return 1;
    }
  else
    return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int mulan_can_be_found_with_name(char *lan)
{
  int result = 0;
  t_mulan *cur = mulan_find_with_name(lan);
  if (cur)
    {
    if (msg_exist_channel(cur->llid))
    result = cur->llid;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int muwlan_can_be_found_with_name_num(char *name, int num)
{
  int result = 0;
  t_mulan *cur = get_name_num(name, num);
  if (cur)
    {
    if (msg_exist_channel(cur->llid))
      result = cur->llid;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static char *get_traf_path(char *lan)
{
  static char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
    snprintf(path,MAX_PATH_LEN-1,"%s/%s", utils_get_muswitch_traf_dir(), lan);
  return path;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_mulan *mulan_alloc(int is_wlan, char *lan)
{
  t_mulan *mulan = NULL;
  if (lan[0] == 0)
    KOUT(" ");
  if (!mulan_find_with_name(lan))
    {
    mulan = (t_mulan *) clownix_malloc(sizeof(t_mulan), 4);
    memset(mulan, 0, sizeof(t_mulan));
    mulan->is_wlan = is_wlan;
    strncpy(mulan->lan, lan, MAX_NAME_LEN-1);
    strncpy(mulan->sock, utils_mulan_get_sock_path(lan), MAX_PATH_LEN-1);
    if (is_wlan == 0)
      strncpy(mulan->traf,get_traf_path(lan),MAX_PATH_LEN-1);
    if (g_head_mulan)
      g_head_mulan->prev = mulan;
    mulan->next = g_head_mulan;
    g_head_mulan = mulan; 
    inc_lock_self_destruction_dir();
    }
  return mulan;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void mulan_request_quit(t_mulan *mulan)
{
  trigger_zombie_kill(mulan->lan, mulan->llid, mulan->clone_start_pid);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int  mulan_get_all_llid(int32_t **llid_tab)
{
  t_mulan *cur = g_head_mulan;
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
    cur = g_head_mulan;
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

/****************************************************************************/
int  mulan_get_all_pid(t_lst_pid **lst_pid)
{
  t_lst_pid *glob_lst = NULL;
  t_mulan *cur = g_head_mulan;
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
    cur = g_head_mulan;
    i = 0;
    while(cur)
      {
      if (cur->pid)
        {
        strncpy(glob_lst[i].name, cur->lan, MAX_NAME_LEN-1);
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
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void mulan_pid_resp(int llid, char *lan, int pid)
{
  t_mulan *mulan = mulan_find_with_llid(llid);
  if (mulan)
    {
    mulan->unanswered_pid_req = 0;
    if (strcmp(lan, mulan->lan))
      KERR("%s %s", lan, mulan->lan);
    if (mulan->pid == 0)
      {
      if (mulan->clone_start_pid != pid)
        {
        KERR("WRONG PID %s %d %d", lan, pid, mulan->clone_start_pid);
        if (mulan->clone_start_pid == 0)
          {
          KERR("MODIFYING START PID %s %d", lan, pid);
          mulan->clone_start_pid = pid;
          }
        }
      else
        {
        mulan->pid = pid;
        if (mulan->is_wlan)
          endp_evt_mulan_birth(lan);
        }
      }
    else
      {
      if (mulan->pid != pid)
        KERR("%s %d %d", lan, pid, mulan->pid);
      }
    }
  else
    KERR("%s %d", lan, pid);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int llid_flow_to_restrict(char *name, int num, int tidx)
{
  int type, llid = endp_mngt_can_be_found_with_name(name, num, &type);
  if (!llid)
    KERR("FLOW TO RESTRICT NOT FOUND!!! %s num:%d tidx:%d", name, num, tidx);
  llid = 0;
//TODO
  return llid;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void mulan_rpct_recv_evt_msg(int llid, int tid, char *line)
{
  char name[MAX_NAME_LEN];
  char wlan[MAX_NAME_LEN];
  int num, tidx, ptx, btx, prx, brx, rank, stop;
  unsigned int ms;
  t_mulan *mulan = mulan_find_with_llid(llid);
  if (mulan)
    {
    if (sscanf(line, 
        "cloonix_evt_peer_flow_control name=%s num=%d tidx=%d rank=%d stop=%d",
        name, &num, &tidx, &rank, &stop) == 5)
      {
      llid = llid_flow_to_restrict(name, num, tidx);
      if (llid)
        murpc_dispatch_send_tx_flow_control(llid, rank, stop);
      }
    else if (sscanf(line,
             "wlan_eventfull_tx_rx %u %s %d %d %s %d %d %d %d",
             &ms, name, &num, &tidx, wlan, &ptx, &btx, &prx, &brx) == 9)
      {
      endp_mngt_wlan_eventfull(name, num, tidx, wlan, ms, ptx, btx, prx, brx);
      }
    else
      KERR("%s", line);
    }
  else
    KERR("%s", line);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void mulan_rpct_recv_diag_msg(int llid, int tid, char *line)
{
  char name[MAX_NAME_LEN];
  char tmpbuf[MAX_PATH_LEN];
  char *lan;
  int num, tidx;
  t_mulan *mulan = mulan_find_with_llid(llid);
  if (mulan)
    {
    lan = mulan->lan;
    if (!strcmp(line, "cloonix_resp_quit"))
      {
      }
    else if (sscanf(line, "cloonix_resp_listen_ok=%s", tmpbuf) == 1)
      {
      if (mulan->traffic_lan_link_state == traffic_lan_link_wait)
        {
        if (!strcmp(tmpbuf, mulan->traf))
          {
          mulan->traffic_lan_link_state = traffic_lan_link_done; 
          endp_evt_mulan_birth(lan);
          }
        else
          KERR("%s %s %s", lan, mulan->traf, tmpbuf);
        }
      else
        KERR("%s %s", lan, tmpbuf);
      }
    else if (sscanf(line, "cloonix_resp_listen_ko=%s", tmpbuf) == 1)
      {
      KERR("%s %s", lan, tmpbuf);
      mulan_request_quit(mulan);
      }
    else if (sscanf(line, 
                    "cloonix_resp_connect_ok=%s name=%s num=%d tidx=%d",
                    tmpbuf, name, &num, &tidx) == 4)
      {
      wlan_name_num_alloc(mulan, name, num);
      endp_evt_connect_OK(name, num, lan, tidx, 0);
      }
    else if (sscanf(line, 
                    "cloonix_resp_connect_ko=%s name=%s num=%d tidx=%d",
                    tmpbuf, name, &num, &tidx) == 4)
      {
      KERR("%s %s", lan, tmpbuf);
      endp_evt_connect_KO(name, num, lan, tidx);
      }
    else if (sscanf(line, 
                    "cloonix_resp_disconnect_ok name=%s num=%d tidx=%d",
                    name, &num, &tidx) == 3)
      {
      wlan_name_num_free(mulan, name, num);
      }
    else if (sscanf(line, 
                    "cloonix_resp_disconnect_ko name=%s num=%d tidx=%d",
                    name, &num, &tidx) == 3)
      {
      wlan_name_num_free(mulan, name, num);
      }
    else if (!strcmp(line, "SELF-DESTROYING"))
      {
      }
    else
      KERR("%s", line);
    } 
  else
    KERR("%s", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void mulan_err_cb (int llid)
{
  t_mulan *mulan = mulan_find_with_llid(llid);
  if (mulan)
    {
    event_print("%s %s", __FUNCTION__, mulan->lan);
    if (mulan->llid != llid)
      KERR("BAD  %d %d", mulan->llid, llid);
    mulan->llid = 0;
    trigger_zombie_kill(mulan->lan, llid, mulan->clone_start_pid);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void get_spy_with_path(char *spy_path, char *path)
{
  char *dirc, *basec, *bname, *dname;
  dirc = strdup(path);
  basec = strdup(path);
  dname = dirname(dirc);
  bname = basename(basec);
  if (strlen(dname) + strlen("spy_") + strlen(bname) + 3 >= MAX_PATH_LEN)
    KOUT("%s/spy_%s", dname, bname);
  memset(spy_path, 0, MAX_PATH_LEN);
  snprintf(spy_path, MAX_PATH_LEN-1, "%s/spy_%s", dname, bname);
  free(dirc);
  free(basec);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void mulan_death(void *data, int status, char *lan)
{
  t_mulan_arg *mu = (t_mulan_arg *) data;
  t_mulan *mulan = mulan_find_with_name(mu->lan);
  char spy_path[MAX_PATH_LEN];

  if (strcmp(lan, mu->lan))
    KOUT("%s %s", lan, mu->lan);
  event_print("End muswitch %s", lan);
  if (!mulan)
    KERR("%s", lan);
  else
    {
    if (mulan->llid)
      llid_trace_free(mulan->llid, 0, __FUNCTION__);
    endp_evt_mulan_death(mulan->lan);
    unlink(mulan->sock);
    get_spy_with_path(spy_path, mulan->sock);
    unlink(spy_path);
    if (!mulan->is_wlan)
      unlink(mulan->traf);
    if (mulan->prev)
      mulan->prev->next = mulan->next;
    if (mulan->next)
      mulan->next->prev = mulan->prev;
    if (mulan == g_head_mulan)
      g_head_mulan = mulan->next;
    clownix_free(mulan, __FUNCTION__);
    dec_lock_self_destruction_dir();
    }
  clownix_free(mu, __FUNCTION__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static char **mulan_birth_argv(t_mulan_arg *mu, int is_wlan)
{
  static char bin_path[MAX_PATH_LEN];
  static char sock[MAX_PATH_LEN];
  static char *argv[] = {bin_path, sock, NULL};
  snprintf(sock, MAX_PATH_LEN, "%s", mu->sock);
  snprintf(bin_path,MAX_PATH_LEN,"%s",utils_get_muswitch_bin_path(is_wlan));
  sock[MAX_PATH_LEN-1] = 0;
  bin_path[MAX_PATH_LEN-1] = 0;
  return argv;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int mulan_birth(void *data)
{
  char **argv = (char **) data;
  my_mkdir(utils_get_muswitch_sock_dir(), 0);
  my_mkdir(utils_get_muswitch_traf_dir(), 0);
//VIP
//sleep(10000000);
  execv(argv[0], argv);
  return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_mulan_watchdog(void *data)
{
  t_mulan_arg *mu = (t_mulan_arg *) data;
  t_mulan *mulan = mulan_find_with_name(mu->lan);
  if (mulan)
    {
    if ((!mulan->llid) || (!mulan->pid))
      KERR("%s", mu->lan);
    }
  clownix_free(mu, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int mulan_start(char *lan, int is_wlan)
{
  int pid;
  char **argv;
  int result = -1;
  t_mulan_arg *mu1, *mu2;
  t_mulan *mulan = mulan_find_with_name(lan);
  if (mulan)
    KERR("mulan %s exists", lan);
  else if (zombie_find_with_name(lan))
    KERR("zombie of mulan %s exists", lan);
  else
    {
    result = 0;
    if (file_exists(utils_mulan_get_sock_path(lan), F_OK))
      unlink(utils_mulan_get_sock_path(lan));
    if (is_wlan == 0)
      {
      if (file_exists(get_traf_path(lan), F_OK))
        unlink(get_traf_path(lan));
      }
    mulan = mulan_alloc(is_wlan, lan);
    if (!mulan)
      KOUT("Exists %s", lan);
    if (is_wlan == 0)
      {
      mu1 = (t_mulan_arg *) clownix_malloc(sizeof(t_mulan_arg), 4);
      strncpy(mu1->lan, mulan->lan, MAX_NAME_LEN-1);
      strncpy(mu1->sock, mulan->sock, MAX_PATH_LEN-1);
      memset(mu1, 0, sizeof(t_mulan_arg));
      clownix_timeout_add(300, timer_mulan_watchdog, (void *) mu1, NULL, NULL);
      }
    mu2 = (t_mulan_arg *) clownix_malloc(sizeof(t_mulan_arg), 4);
    memset(mu2, 0, sizeof(t_mulan_arg));
    strncpy(mu2->lan, mulan->lan, MAX_NAME_LEN-1);
    strncpy(mu2->sock, mulan->sock, MAX_PATH_LEN-1);
    argv = mulan_birth_argv(mu2, is_wlan);
    utils_send_creation_info("mulan", argv);
    pid = pid_clone_launch(mulan_birth, mulan_death, NULL,
                           argv, mu2, NULL, mu2->lan, -1, 1);
    mulan->clone_start_pid = pid;

//VIP
//    mulan->clone_start_pid = 0;

    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void mulan_test_stop(char *lan)
{
  int type;
  t_mulan *mulan = mulan_find_with_name(lan);
  if (mulan)
    {
    if (!endp_evt_lan_is_in_use(lan, &type))
      {
      mulan_request_quit(mulan);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_mulan_beat(void *data)
{
  t_mulan *cur = g_head_mulan;
  char cmd[2*MAX_PATH_LEN];
  while(cur)
    {
    if (cur->llid == 0)
      trace_alloc(cur);
    else if (cur->pid == 0) 
      rpct_send_pid_req(NULL, cur->llid, type_hop_mulan, cur->lan, 0);
    else if ((cur->is_wlan == 0) && 
             (cur->traffic_lan_link_state == traffic_lan_link_idle))
      {
      memset(cmd, 0, 2*MAX_PATH_LEN);
      snprintf(cmd,2*MAX_PATH_LEN-1,"cloonix_req_listen=%s lan=%s",
                                     cur->traf, cur->lan);
      if (!try_rpct_send_diag_msg(cur->llid, cur->pid, cmd))
        cur->traffic_lan_link_state = traffic_lan_link_wait; 
      }
    if (cur->pid) 
      {
      cur->periodic_count += 1;
      if (cur->periodic_count >= 5)
        {
        rpct_send_pid_req(NULL, cur->llid, type_hop_mulan, cur->lan, 0);
        cur->periodic_count = 1;
        cur->unanswered_pid_req += 1;
        if (cur->unanswered_pid_req > 3)
          {
          KERR("LAN %s NOT RESPONDING KILLING IT", cur->lan);
          mulan_request_quit(cur);
          }
        }
      }
    cur = cur->next;
    }
  clownix_timeout_add(50, timer_mulan_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void mulan_del_all(void)
{
  t_mulan *next, *cur = g_head_mulan;
  while(cur)
    {
    next = cur->next;
    mulan_request_quit(cur);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int mulan_is_zombie(char *lan)
{
  int result = 0;
  if (zombie_find_with_name(lan))
    result = 1;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int mulan_is_oper(char *lan)
{
  int result = 0;
  t_mulan *cur = mulan_find_with_name(lan);
  if (cur)
    {
    if (cur->pid)
      {
      if (cur->is_wlan)
        result = 1;
      else if (cur->traffic_lan_link_state == traffic_lan_link_done)
        result = 1;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int mulan_get_is_wlan(char *lan)
{
  int result = 0;
  t_mulan *mulan = mulan_find_with_name(lan);
  if (!mulan)
    KERR("%s", lan);
  else
    result = mulan->is_wlan;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int mulan_send_wlan_req_disconnect(char *lan, char *name, int num, int tidx)
{
  int result = -1;
  t_mulan *mulan = mulan_find_with_name(lan);
  char cmd[MAX_PATH_LEN];
  if (!mulan)
    KERR("%s", lan);
  else
    {
    memset(cmd, 0, MAX_PATH_LEN);
    snprintf(cmd, MAX_PATH_LEN-1,
             "cloonix_req_disconnect wlan=%s name=%s num=%d tidx=%d", 
             lan, name, num, tidx);
    if (try_rpct_send_diag_msg(mulan->llid, mulan->pid, cmd))
      KERR("%s", lan);
    else
      result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int mulan_send_wlan_req_connect(char *sock, char *lan, 
                                char *name, int num, int tidx)
{
  int result = -1;
  t_mulan *mulan = mulan_find_with_name(lan);
  char cmd[MAX_PATH_LEN];
  if (!mulan)
    KERR("%s", lan);
  else
    {
    memset(cmd, 0, MAX_PATH_LEN);
    snprintf(cmd, MAX_PATH_LEN-1,
             "cloonix_req_connect=%s wlan=%s name=%s num=%d tidx=%d",
              sock, lan, name, num, tidx);
    if (try_rpct_send_diag_msg(mulan->llid, mulan->pid, cmd))
      KERR("%s", lan);
    else
      result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void mulan_init(void)
{
  g_head_mulan = NULL;
  clownix_timeout_add(50, timer_mulan_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

