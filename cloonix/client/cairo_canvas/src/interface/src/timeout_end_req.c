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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "layout_rpc.h"
#include "doorways_sock.h"
#include "client_clownix.h"
#include "commun_consts.h"
#include "interface.h"
#include "bank.h"
#include "popup.h"
#include "layout_x_y.h"
#include "timeout_start_req.h"
#include "make_layout.h"
#include "cloonix.h"
#include "lib_topo.h"
#include "menu_dialog_kvm.h"
#include "menu_dialog_cnt.h"

char *get_brandtype_image(char *type);
char *get_brandtype_type(void);

static t_topo_info *topo_info = NULL;
static int topo_count = 0;

/*****************************************************************************/
static void callback_end(int tid, int status, char *info)
{
  char err[MAX_PRINT_LEN];
  if (status)
    {
    sprintf(err, "KO: %s\n", info);
    insert_next_warning(err, 1);
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void topo_info_update(t_topo_info *topo)
{
  topo_free_topo(topo_info);
  topo_info = topo;
  topo_count++;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void timer_create_item_node_req(void *data)
{
  int32_t thidden_on_graph[MAX_ETH_VM];
  int i, vm_config_flags, natplug = 0;
  t_custom_vm *cust_vm;
  t_custom_cnt *cust_cnt;
  t_topo_cnt cust_topo_cnt;
  t_item_node_req *pa = (t_item_node_req *) data;
  if (pa->is_cnt == 0)
    {
    get_custom_vm(&cust_vm);
    for (i=0; i<MAX_ETH_VM; i++)
      thidden_on_graph[i] = 0;
    vm_config_flags = get_vm_config_flags(cust_vm, &natplug);
    set_node_layout_x_y(cust_vm->name, pa->x, pa->y, 0, 
                        pa->tx, pa->ty, thidden_on_graph);
    client_add_vm(0, callback_end, cust_vm->name,
                  cust_vm->nb_tot_eth, cust_vm->eth_tab,
                  vm_config_flags, natplug, cust_vm->cpu, cust_vm->mem,
                  NULL, get_brandtype_image(NULL), NULL, NULL, NULL);
    }
  else if ((strcmp(get_brandtype_type(), "brandzip")) &&
           (strcmp(get_brandtype_type(), "brandcvm")))
    KERR("ERROR %s", get_brandtype_type());
  else
    {
    get_custom_cnt(&cust_cnt);
    memset(&cust_topo_cnt, 0, sizeof(t_topo_cnt));
    strncpy(cust_topo_cnt.brandtype, get_brandtype_type(), MAX_NAME_LEN-1);
    strncpy(cust_topo_cnt.name, cust_cnt->name, MAX_NAME_LEN-1);
    strncpy(cust_topo_cnt.startup_env, cust_cnt->startup_env, MAX_PATH_LEN-1);
    strncpy(cust_topo_cnt.vmount, cust_cnt->vmount, MAX_SIZE_VMOUNT-1);
    if (!strcmp(get_brandtype_type(), "brandzip"))
      {
      cust_topo_cnt.is_persistent = cust_cnt->is_persistent_zip;
      cust_topo_cnt.is_privileged = cust_cnt->is_privileged_zip;
      }
    else
      {
      cust_topo_cnt.is_persistent = cust_cnt->is_persistent_cvm;
      cust_topo_cnt.is_privileged = cust_cnt->is_privileged_cvm;
      }
    cust_topo_cnt.nb_tot_eth = cust_cnt->nb_tot_eth;
    memcpy(cust_topo_cnt.eth_table, cust_cnt->eth_table,
           cust_topo_cnt.nb_tot_eth*sizeof(t_eth_table));
    strncpy(cust_topo_cnt.image, get_brandtype_image(NULL), MAX_PATH_LEN-1);
    client_add_cnt(0, callback_end, &cust_topo_cnt);
    }
  clownix_free(pa, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void timer_create_item_req(void *data)
{
  t_item_req *pa = (t_item_req *) data;
  t_c2c_req_info *c2c = &(pa->c2c_req_info); 
  set_gene_layout_x_y(pa->bank_type, pa->name, pa->x, pa->y, 
                      pa->xa, pa->ya, pa->xb, pa->yb, 0);
  switch(pa->bank_type)
    {
    case bank_type_lan:
      from_cloonix_switch_create_lan(pa->name);
      break;
    case bank_type_sat:
      if (pa->endp_type == endp_type_c2cv)
        {
        if ((!strlen(pa->name)) || (!strlen(c2c->dist_cloon)))
          KOUT(" ");
        client_add_c2c(0, callback_end, pa->name, c2c->loc_udp_ip,
                       c2c->dist_cloon, c2c->dist_tcp_ip,
                       c2c->dist_tcp_port, c2c->dist_passwd,
                       c2c->dist_udp_ip, c2c->c2c_udp_port_low);
        }
      else if (pa->endp_type == endp_type_a2b)
        client_add_a2b(0, callback_end, pa->name);
      else if (pa->endp_type == endp_type_taps)
        client_add_tap(0, callback_end, pa->name);
      else if (pa->endp_type == endp_type_natv)
        client_add_nat(0, callback_end, pa->name);
      else if ((pa->endp_type == endp_type_phyas) ||
               (pa->endp_type == endp_type_phyms))
        {
        if (strlen(pa->name) == 0)
          KERR("%d %d", pa->bank_type, pa->endp_type);
        else
          {
          if (pa->endp_type == endp_type_phyas)
            client_add_phy(0, callback_end, pa->name, endp_type_phyas);
          else if (pa->endp_type == endp_type_phyms)
            client_add_phy(0, callback_end, pa->name, endp_type_phyms);
          else
            KOUT("ERROR %s %d", pa->name, pa->endp_type);
          }
        }
      else
        KERR("%d %s %d", pa->bank_type, pa->name, pa->endp_type);
      break;
    default:
      KOUT("%d", pa->bank_type);
    }
  clownix_free(pa, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void timer_create_edge_req(void *data)
{
  t_edge_req *pa = (t_edge_req *) data;
  client_add_lan_endp(0, callback_end, pa->name, pa->num, pa->lan);
  clownix_free(pa, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void timer_delete_edge_req(void *data)
{
  t_edge_req *pa = (t_edge_req *) data;
  client_del_lan_endp(0, callback_end, pa->name, pa->num, pa->lan);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void timer_dyn_snf_req(void *data)
{
  t_dyn_snf_req *pa = (t_dyn_snf_req *) data;
  client_add_snf(0, callback_end, pa->name, pa->num, pa->on);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void timer_delete_item_req(void *data)
{
  t_item_req *pa = (t_item_req *) data;
  switch(pa->bank_type)
    {
    case bank_type_node:
    case bank_type_cnt:
    case bank_type_sat:
      client_del_sat(0, callback_end, pa->name);
      break;
    case bank_type_lan:
      from_cloonix_switch_delete_lan(pa->name);
      break;
    default:
      KOUT("%d", pa->bank_type);
    }
  clownix_free(pa, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/


