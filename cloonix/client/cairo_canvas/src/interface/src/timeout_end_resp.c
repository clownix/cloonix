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
#include "doorways_sock.h"
#include "client_clownix.h"
#include "commun_consts.h"
#include "interface.h"
#include "bank.h"
#include "popup.h"
#include "layout_x_y.h"
#include "timeout_start_resp.h"




/****************************************************************************/
static void create_node_resp(t_topo_kvm *kvm)
{
  int hidden_on_graph, color_choice;
  double x, y;
  double tx[MAX_ETH_VM+MAX_WLAN_VM];
  double ty[MAX_ETH_VM+MAX_WLAN_VM];
  int32_t thidden_on_graph[MAX_ETH_VM+MAX_WLAN_VM];

  get_node_layout_x_y(kvm->name, &color_choice, &x, &y, &hidden_on_graph, 
                      tx, ty, thidden_on_graph);
  bank_node_create(kvm->name, kvm->linux_kernel, kvm->rootfs_used, 
                   kvm->rootfs_backing,  kvm->install_cdrom,
                   kvm->added_cdrom, kvm->added_disk, kvm->nb_eth,
                   kvm->nb_wlan, 
                   color_choice, kvm->vm_id, kvm->vm_config_flags,
                   x, y, hidden_on_graph, tx, ty, thidden_on_graph);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void create_sat_resp(char *name, int type, 
                            t_topo_c2c *c2c, t_topo_snf *snf)
{
  double x, y, xa, ya, xb, yb;
  int hidden;
  get_gene_layout_x_y(bank_type_sat, name, type,
                      &x, &y, &xa, &ya, &xb, &yb, &hidden);
  bank_sat_create(name, type, c2c, snf,
                  x, y, xa, ya, xb, yb, hidden);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void timer_create_obj_resp(void *data)
{
  t_item_obj_resp *pa = (t_item_obj_resp *) data;
  if (pa->kvm)
    {
    create_node_resp(pa->kvm);
    clownix_free(pa->kvm, __FUNCTION__);
    }
  else if (pa->c2c)
    {
    create_sat_resp(pa->c2c->name, endp_type_c2c, pa->c2c, NULL);
    clownix_free(pa->c2c, __FUNCTION__);
    }
  else if (pa->snf)
    {
    create_sat_resp(pa->snf->name, endp_type_snf, NULL, pa->snf);
    clownix_free(pa->snf, __FUNCTION__);
    }
  else if (pa->sat)
    {
    create_sat_resp(pa->sat->name, pa->sat->type, NULL, NULL);
    clownix_free(pa->sat, __FUNCTION__);
    }
  else
    KOUT(" ");
  clownix_free(pa, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void timer_create_item_resp(void *data)
{
  t_item_lan_resp *lan_pa = (t_item_lan_resp *) data;
  double x, y, xa, ya, xb, yb;
  int hidden_on_graph;
  switch(lan_pa->bank_type)
    {
    case bank_type_lan:
      get_gene_layout_x_y(lan_pa->bank_type, lan_pa->name, lan_pa->mutype, 
                          &x, &y,  &xa, &ya, &xb, &yb, &hidden_on_graph);
      bank_lan_create(lan_pa->name,  x, y, hidden_on_graph);
      break;

    default:
      KOUT("%d", lan_pa->bank_type);
    }
  clownix_free(data, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void timer_create_edge_resp(void *data)
{
  t_edge_resp *pa = (t_edge_resp *) data;
  bank_edge_create(pa->name, pa->num, pa->lan);
  clownix_free(pa, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void timer_delete_item_resp(void *data)
{
  t_item_delete_resp *pa = (t_item_delete_resp *) data;
  switch(pa->bank_type)
    {
    case bank_type_node:
      bank_node_delete(pa->name);
      break;
    case bank_type_lan:
      bank_lan_delete(pa->name);
      break;
    case bank_type_sat:
      bank_sat_delete(pa->name);
      break;
    default:
      KOUT("%d", pa->bank_type);
    }

  clownix_free(pa, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void timer_delete_edge_resp(void *data)
{
  t_edge_resp *pa = (t_edge_resp *) data;
  bank_edge_delete(pa->name, pa->num, pa->lan);
  clownix_free(pa, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

