/*****************************************************************************/
/*    Copyright (C) 2006-2022 clownix@clownix.net License AGPL-3             */
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
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_consts.h"
#include "cloonix.h"
#include "interface.h"
#include "doorways_sock.h"
#include "client_clownix.h"
#include "popup.h"
#include "main_timer_loop.h"
#include "pid_clone.h"
#include "bank.h"
#include "eventfull_eth.h"
#include "layout_rpc.h"
#include "lib_topo.h"
#include "utils.h"
#include "canvas_ctx.h"
#include "menu_dialog_cnt.h"
#include "menu_dialog_kvm.h"

void layout_set_ready_for_send(void);
int check_before_start_launch(char **argv);
void wireshark_kill(char *name);

/*---------------------------------------------------------------------------*/
typedef struct t_vm_config
{
  char name[MAX_NAME_LEN];
  char ip[MAX_NAME_LEN];
  char status[MAX_NAME_LEN];
  int vm_id;
  int vm_config_flags;
} t_vm_config;
/*---------------------------------------------------------------------------*/

typedef struct t_enqueue_event
{
  int nb_endp;
  t_eventfull_endp *endp;
} t_enqueue_event;

/*--------------------------------------------------------------------------*/
static t_topo_info *current_topo = NULL;
static int g_not_first_callback_topo;
static int glob_nb_vm_config;
static t_vm_config *glob_vm_config;
static char glob_path[MAX_PATH_LEN];
static char *g_glob_path;
/*--------------------------------------------------------------------------*/

char *get_doors_client_addr(void);




/****************************************************************************/
static void kvm_update_bitem(t_topo_info *topo)
{
  int i,j, endp_type, prev_endp_type;
  t_topo_kvm *cur;
  t_bank_item *bitem;
  t_list_bank_item *eth_bitem;
  for (i=0; i<topo->nb_kvm; i++)
    {
    cur = &(topo->kvm[i]);
    bitem = bank_get_item(bank_type_node, cur->name, 0, NULL);
    if (bitem)
      {
      bitem->pbi.color_choice = topo->kvm[i].color;
      eth_bitem = bitem->head_eth_list;
      while(eth_bitem)
        {
        j = eth_bitem->bitem->num;
        endp_type = topo->kvm[i].eth_table[j].endp_type;
        prev_endp_type = bitem->pbi.pbi_node->eth_tab[j].endp_type;
        if ((endp_type != endp_type_eths) &&
            (endp_type != endp_type_ethv))
          KERR("%s %d", cur->name, endp_type);
        else if ((prev_endp_type != endp_type_eths) &&
                 (prev_endp_type != endp_type_ethv))
          KERR("%s %d", cur->name, prev_endp_type);
        else
          bitem->pbi.pbi_node->eth_tab[j].endp_type = endp_type;
        eth_bitem = eth_bitem->next;
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void cnt_update_bitem(t_topo_info *topo)
{
  int i,j, endp_type, prev_endp_type;
  t_topo_cnt *cur;
  t_bank_item *bitem;
  t_list_bank_item *eth_bitem;
  for (i=0; i<topo->nb_cnt; i++)
    {
    cur = &(topo->cnt[i]);
    bitem = bank_get_item(bank_type_cnt, cur->name, 0, NULL);
    if (bitem) 
      {
      bitem->pbi.pbi_cnt->cnt_evt_ping_ok = topo->cnt[i].ping_ok;
      bitem->pbi.color_choice = topo->cnt[i].color;
      eth_bitem = bitem->head_eth_list;
      while(eth_bitem)
        { 
        j = eth_bitem->bitem->num;
        endp_type = topo->cnt[i].eth_table[j].endp_type;
        prev_endp_type = bitem->pbi.pbi_cnt->eth_tab[j].endp_type;
        if ((endp_type != endp_type_eths) &&
            (endp_type != endp_type_ethv))
          KERR("%s %d", cur->name, endp_type);
        else if ((prev_endp_type != endp_type_eths) &&
                 (prev_endp_type != endp_type_ethv))
          KERR("%s %d", cur->name, prev_endp_type);
        else
          bitem->pbi.pbi_cnt->eth_tab[j].endp_type = endp_type;
        eth_bitem = eth_bitem->next;
        } 
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void c2c_update_bitem(t_topo_info *topo)
{
  int i;
  t_topo_c2c *cur;
  t_bank_item *bitem;
  for (i=0; i<topo->nb_c2c; i++)
    {
    cur = &(topo->c2c[i]);
    bitem = bank_get_item(bank_type_sat, cur->name, 0, NULL);
    if ((bitem) && ((bitem->pbi.endp_type == endp_type_c2cs) ||
                    (bitem->pbi.endp_type == endp_type_c2cv)))
      {
      memcpy(&(bitem->pbi.pbi_sat->topo_c2c), cur, sizeof(t_topo_c2c));
      if ((cur->endp_type != endp_type_c2cs) &&
          (cur->endp_type != endp_type_c2cv))
        KERR("ERROR %s %d", cur->name,  cur->endp_type);
      else
        bitem->pbi.endp_type = cur->endp_type;
      }
    }  
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void tap_update_bitem(t_topo_info *topo)
{
  int i;
  t_topo_tap *cur;
  t_bank_item *bitem;
  for (i=0; i<topo->nb_tap; i++)
    {
    cur = &(topo->tap[i]);
    bitem = bank_get_item(bank_type_sat, cur->name, 0, NULL);
    if ((bitem) && ((bitem->pbi.endp_type == endp_type_taps) ||
                    (bitem->pbi.endp_type == endp_type_tapv)))
      {
      if ((cur->endp_type != endp_type_taps) &&
          (cur->endp_type != endp_type_tapv))
        KERR("ERROR %s %d", cur->name,  cur->endp_type);
      else
        bitem->pbi.endp_type = cur->endp_type;
      }
    }  
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void nat_update_bitem(t_topo_info *topo)
{
  int i;
  t_topo_nat *cur;
  t_bank_item *bitem;
  for (i=0; i<topo->nb_nat; i++)
    {
    cur = &(topo->nat[i]);
    bitem = bank_get_item(bank_type_sat, cur->name, 0, NULL);
    if ((bitem) && ((bitem->pbi.endp_type == endp_type_nats) ||
                    (bitem->pbi.endp_type == endp_type_natv)))
      {
      memcpy(&(bitem->pbi.pbi_sat->topo_nat), cur, sizeof(t_topo_nat));
      if ((cur->endp_type != endp_type_nats) &&
          (cur->endp_type != endp_type_natv))
        KERR("ERROR %s %d", cur->name,  cur->endp_type);
      else
        bitem->pbi.endp_type = cur->endp_type;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void a2b_update_bitem(t_topo_info *topo)
{
  int i;
  t_topo_a2b *cur;
  t_bank_item *bitem;
  for (i=0; i<topo->nb_a2b; i++)
    {
    cur = &(topo->a2b[i]);
    bitem = bank_get_item(bank_type_sat, cur->name, 0, NULL);
    if ((bitem) && (bitem->pbi.endp_type == endp_type_a2b))
      {
      memcpy(&(bitem->pbi.pbi_sat->topo_a2b), cur, sizeof(t_topo_a2b));
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int get_vm_id_from_topo(char *name)
{
  int i, found = 0;
  for (i=0; i< current_topo->nb_kvm; i++)
    if (!strcmp(name, current_topo->kvm[i].name))
      {
      found = current_topo->kvm[i].vm_id;
      break;
      }
  return found;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int start_launch(void *ptr)
{
  char **argv = (char **) ptr;
  execv(argv[0], argv);
  return 0;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void launch_xterm_double_click(char *name_vm, int vm_config_flags)
{
  static char title[2*MAX_NAME_LEN+1];
  static char net[MAX_NAME_LEN];
  static char name[MAX_NAME_LEN];
  static char cmd[2*MAX_PATH_LEN];
  static char xvt[MAX_PATH_LEN];
  static char *argv[] = {xvt, "-T", title, "-e", 
                         "/bin/bash", "-c", cmd, NULL};
  static char *argvnatplug[] = {xvt, "-T", title, "-e", 
                              "/usr/local/bin/cloonix_osh",
                              net, name, NULL};
  memset(cmd, 0, 2*MAX_PATH_LEN);
  memset(title, 0, 2*MAX_NAME_LEN+1);
  memset(name, 0, MAX_NAME_LEN);
  memset(net, 0, MAX_NAME_LEN);
  cloonix_get_xvt(xvt);
  strncpy(name, name_vm, MAX_NAME_LEN);
  strncpy(net, local_get_cloonix_name(), MAX_NAME_LEN);

  if (vm_config_flags & VM_CONFIG_FLAG_NATPLUG)
    {
    snprintf(title, 2*MAX_NAME_LEN, "%s/%s", net, name);
    if (check_before_start_launch(argvnatplug))
      pid_clone_launch(start_launch, NULL, NULL, (void *)(argvnatplug),
                       NULL, NULL, name_vm, -1, 0);
    }
  else
    {
    snprintf(title, 2*MAX_NAME_LEN, "%s/%s", local_get_cloonix_name(), name);
    snprintf(cmd, 2*MAX_PATH_LEN-1,
             "%s/common/agent_dropbear/agent_bin/dropbear_cloonix_ssh %s %s %s",
             get_local_cloonix_tree(),  get_doors_client_addr(),
             get_password(), name);
    if (check_before_start_launch(argv))
      pid_clone_launch(start_launch, NULL, NULL, (void *)(argv),
                       NULL, NULL, name_vm, -1, 0);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_callback_topo(void *data)
{
  t_topo_info *topo = (t_topo_info *) data;
  t_topo_differences *diffs = NULL;
  if ((!current_topo) || (topo_compare(topo, current_topo)))
    {
    diffs = topo_get_diffs(topo, current_topo);
    topo_free_topo(current_topo);
    current_topo = topo_duplicate(topo);
    process_all_diffs(diffs);
    topo_free_diffs(diffs);
    c2c_update_bitem(topo);
    tap_update_bitem(topo);
    nat_update_bitem(topo);
    a2b_update_bitem(topo);
    cnt_update_bitem(topo);
    kvm_update_bitem(topo);
    }
  if (g_not_first_callback_topo == 0)
    {
    g_not_first_callback_topo = 1;
    pid_clone_init();
    }
  update_topo_phy(topo->nb_info_phy, topo->info_phy);
  update_topo_bridges(topo->nb_bridges, topo->bridges);
  topo_free_topo(topo);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void callback_topo(int tid, t_topo_info *topo)
{
  t_topo_info *ntopo = topo_duplicate(topo);
  clownix_timeout_add(1, timer_callback_topo, (void *) ntopo, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void slowperiodic_qcow2_cb(int nb, t_slowperiodic *spic)
{
  set_bulkvm(nb, spic);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void slowperiodic_img_cb(int nb, t_slowperiodic *spic)
{
  set_bulcru(nb, spic);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void slowperiodic_docker_cb(int nb, t_slowperiodic *spic)
{
  set_buldoc(nb, spic);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void slowperiodic_podman_cb(int nb, t_slowperiodic *spic)
{
  set_bulpod(nb, spic);
}
/*--------------------------------------------------------------------------*/



/****************************************************************************/
static void timer_enqueue_eventfull(void *data)
{
  t_enqueue_event *evt = (t_enqueue_event *) data;
  int nb_endp = evt->nb_endp;
  t_eventfull_endp *endp = evt->endp;
  eventfull_has_arrived();
  eventfull_arrival(nb_endp, endp);
  clownix_free(evt->endp, __FUNCTION__);
  clownix_free(evt, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void eventfull_cb(int nb_endp, t_eventfull_endp *endp)
{
  t_enqueue_event *evt;
  int len = nb_endp * sizeof(t_eventfull_endp);;
  evt = (t_enqueue_event *) clownix_malloc(sizeof(t_enqueue_event), 5);
  memset(evt, 0, sizeof(t_enqueue_event));
  evt->endp = (t_eventfull_endp *) clownix_malloc(len, 5);
  memset(evt->endp, 0, len);
  evt->nb_endp = nb_endp;
  memcpy(evt->endp, endp, len);
  clownix_timeout_add(1, timer_enqueue_eventfull, (void *) evt, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void topo_small_event_cb(int tid, char *name, 
                                char *p1, char *p2, int evt)
{
  if ((evt == vm_evt_qmp_conn_ok)       || 
      (evt == vm_evt_qmp_conn_ko)       ||
      (evt == vm_evt_cloonix_ga_ping_ok)||
      (evt == vm_evt_cloonix_ga_ping_ko))
    {
    ping_enqueue_evt(name, evt);
    }
  else
    KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void timer_client_is_connected(void *data)
{
  if (!client_is_connected())
    {
    printf("\nTIMEOUT CONNECTING\n\n");
    exit(-1);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void timer_topo_subscribe(void *data)
{
  client_topo_small_event_sub(0, topo_small_event_cb);
  layout_set_ready_for_send();
  client_req_eventfull(eventfull_cb);
  client_req_slowperiodic(slowperiodic_qcow2_cb,
                          slowperiodic_img_cb,
                          slowperiodic_docker_cb,
                          slowperiodic_podman_cb);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void interface_topo_subscribe(void)
{
  client_topo_sub(0, callback_topo);
  clownix_timeout_add(150, timer_topo_subscribe, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void interface_switch_init(char *path, char *password)
{
  g_not_first_callback_topo = 0;
  glob_nb_vm_config = 0;
  glob_vm_config = NULL;
  g_glob_path = NULL;
  memset(glob_path, 0, MAX_PATH_LEN);
  if (path)
    {
    strncpy(glob_path, path, MAX_PATH_LEN-1);
    g_glob_path = glob_path;
    }
  client_init("graph", g_glob_path, password);
  clownix_timeout_add(300, timer_client_is_connected, NULL, NULL, NULL);
  while(!client_is_connected())
    msg_mngt_loop_once();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_topo_info *get_current_topo(void)
{
  t_topo_info *topo = topo_duplicate(current_topo);
  return topo;
}
/*--------------------------------------------------------------------------*/











