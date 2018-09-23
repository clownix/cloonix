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
  char *ptr_p9_host_share = NULL;
  int32_t thidden_on_graph[MAX_ETH_VM+MAX_WLAN_VM];
  int i, vm_config_flags;
  t_custom_vm *cust_vm;
  t_item_node_req *pa = (t_item_node_req *) data;
  get_custom_vm (&cust_vm);
  for (i=0; i<MAX_ETH_VM+MAX_WLAN_VM; i++)
    thidden_on_graph[i] = 0;
  if ((cust_vm->nb_eth > MAX_ETH_VM) || (cust_vm->nb_eth < 1))
    KOUT("%d", cust_vm->nb_eth);
  if ((cust_vm->nb_wlan > MAX_WLAN_VM) || (cust_vm->nb_wlan < 0))
    KOUT("%d", cust_vm->nb_wlan);
  if (cust_vm->has_p9_host_share)
    ptr_p9_host_share = cust_vm->kvm_p9_host_share;
  if (cust_vm->kvm_used_rootfs[0])
    {
    vm_config_flags = get_vm_config_flags(cust_vm);
    set_node_layout_x_y(cust_vm->name, 0, pa->x, pa->y, 0, 
                        pa->tx, pa->ty, thidden_on_graph);
    client_add_vm(0, callback_end, cust_vm->name, cust_vm->nb_eth, 
                  cust_vm->nb_wlan, vm_config_flags, cust_vm->cpu,
                  cust_vm->mem,
                  NULL, cust_vm->kvm_used_rootfs, NULL, NULL, 
                  NULL, ptr_p9_host_share, NULL);
    }
  else
    insert_next_warning("rootfs is empty!", 1);
  clownix_free(pa, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void timer_create_item_req(void *data)
{
  t_item_req *pa = (t_item_req *) data;
  set_gene_layout_x_y(pa->bank_type, pa->name, pa->mutype, pa->x, pa->y, 
                      pa->xa, pa->ya, pa->xb, pa->yb, 0);
  switch(pa->bank_type)
    {
    case bank_type_lan:
      from_cloonix_switch_create_lan(pa->name);
      break;
    case bank_type_sat:
      client_add_sat(0, callback_end, pa->name, pa->mutype, &pa->c2c_req_info);
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
void timer_delete_item_req(void *data)
{
  t_item_req *pa = (t_item_req *) data;
  switch(pa->bank_type)
    {
    case bank_type_node:
      client_del_vm(0, callback_end, pa->name);
      break;
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


