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

int check_before_start_launch(char **argv);
void set_bulkvm(int nb, t_slowperiodic *slowperiodic);

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

/*--------------------------------------------------------------------------*/
static t_topo_info *current_topo = NULL;
static int g_not_first_callback_topo;
static int glob_nb_vm_config;
static t_vm_config *glob_vm_config;
static char glob_path[MAX_PATH_LEN];
static char *g_glob_path;
/*--------------------------------------------------------------------------*/

char *get_doors_client_addr(void);

void modify_c2c(char *name, char *master_cloonix, char *slave_cloonix);

void layout_set_ready_for_send(void);



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
  static char *argvcisco[] = {xvt, "-T", title, "-e", 
                              "/usr/local/bin/cloonix_osh",
                              net, name, NULL};
  memset(cmd, 0, 2*MAX_PATH_LEN);
  memset(title, 0, 2*MAX_NAME_LEN+1);
  memset(name, 0, MAX_NAME_LEN);
  memset(net, 0, MAX_NAME_LEN);
  cloonix_get_xvt(xvt);
  strncpy(name, name_vm, MAX_NAME_LEN);
  strncpy(net, local_get_cloonix_name(), MAX_NAME_LEN);

  if (vm_config_flags & VM_CONFIG_FLAG_CISCO)
    {
    snprintf(title, 2*MAX_NAME_LEN, "%s/%s", net, name);
    if (check_before_start_launch(argvcisco))
      pid_clone_launch(start_launch, NULL, NULL, (void *)(argvcisco),
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
void callback_topo(int tid, t_topo_info *topo)
{
  t_topo_differences *diffs = NULL;
  if ((!current_topo) || (topo_compare(topo, current_topo)))
    {
    diffs = topo_get_diffs(topo, current_topo);
    topo_free_topo(current_topo);
    current_topo = topo_duplicate(topo);
    process_all_diffs(diffs);
    topo_free_diffs(diffs);
    }
  if (g_not_first_callback_topo == 0)
    {
    g_not_first_callback_topo = 1;
    pid_clone_init();
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timeout_eventfull(void *data)
{
  t_eventfull *eventfull = (t_eventfull *) data;
  if (!eventfull)
    KOUT(" ");
  eventfull_has_arrived();
  eventfull_packets_data(eventfull);
  clownix_free(eventfull->endp, __FUNCTION__);
  clownix_free(eventfull, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void slowperiodic_cb(int nb, t_slowperiodic *spic)
{
  set_bulkvm(nb, spic);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void eventfull_cb(int nb_endp, t_eventfull_endp *endp)
{
  t_eventfull *eventfull;
  int len;
  len = sizeof(t_eventfull);
  eventfull = (t_eventfull *) clownix_malloc(len, 13);
  memset(eventfull, 0, len);

  len = nb_endp * sizeof(t_eventfull_endp);
  eventfull->endp = (t_eventfull_endp *) clownix_malloc(len, 13);
  eventfull->nb_endp  = nb_endp;
  memcpy(eventfull->endp,  endp,  nb_endp  * sizeof(t_eventfull_endp));

//  clownix_timeout_add(1, timeout_eventfull, (void *) eventfull, NULL, NULL);
  timeout_eventfull((void *) eventfull);

}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void topo_small_event_cb(int tid, char *name, 
                                char *p1, char *p2, int evt)
{
  if ((evt == c2c_evt_connection_ok)    ||
      (evt == c2c_evt_connection_ko)    ||
      (evt == vm_evt_qmp_conn_ok)       || 
      (evt == vm_evt_qmp_conn_ko)       ||
      (evt == vm_evt_cloonix_ga_ping_ok)||
      (evt == vm_evt_cloonix_ga_ping_ko))
    ping_enqueue_evt(name, evt);
  else if (evt == c2c_evt_mod_master_slave)
    modify_c2c(name, p1, p2);
  else if ((evt == snf_evt_capture_on)  ||
           (evt == snf_evt_capture_off) ||
           (evt == snf_evt_recpath_change))
    {
    modify_snf(name, evt, p1);
    }
  else
    KOUT(" ");
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void timer_topo_subscribe(void *data)
{
  client_topo_sub(0,callback_topo);
  client_topo_small_event_sub(0, topo_small_event_cb);
  layout_set_ready_for_send();
  client_req_eventfull(eventfull_cb);
  client_req_slowperiodic(slowperiodic_cb);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void timer_layout_subscribe(void *data)
{
  send_layout_event_sub(get_clownix_main_llid(), 0, 1);
  glib_prepare_rx_tx(get_clownix_main_llid());
  clownix_timeout_add(20, timer_topo_subscribe, NULL, NULL, NULL);
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
  clownix_timeout_add(1, timer_layout_subscribe, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_topo_info *get_current_topo(void)
{
  t_topo_info *topo = topo_duplicate(current_topo);
  return topo;
}
/*--------------------------------------------------------------------------*/











