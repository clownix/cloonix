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
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "bank.h"
#include "timeout_start_resp.h"
#include "popup.h"
#include "interface.h"

/****************************************************************************/
static void gene_delete_item(int bank_type, char *name)
{
  t_item_delete_resp *pa;
  pa = (t_item_delete_resp *) clownix_malloc(sizeof(t_item_delete_resp), 12);
  memset(pa, 0, sizeof(t_item_delete_resp));
  pa->bank_type = bank_type;
  strncpy(pa->name, name, MAX_NAME_LEN-1);
  clownix_timeout_add(1,timer_delete_item_resp,(void *)pa,NULL,NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void from_cloonix_switch_create_node(t_topo_kvm *kvm)
{
  t_item_obj_resp *pa;
  pa = (t_item_obj_resp *)clownix_malloc(sizeof(t_item_obj_resp), 21);
  memset(pa, 0, sizeof(t_item_obj_resp));
  pa->kvm = (t_topo_kvm *) clownix_malloc(sizeof(t_topo_kvm), 22);
  memcpy(pa->kvm, kvm, sizeof(t_topo_kvm));
  clownix_timeout_add(1, timer_create_obj_resp, (void *)pa, NULL,NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void from_cloonix_switch_create_c2c(t_topo_c2c *c2c)
{
  t_item_obj_resp *pa;
  pa = (t_item_obj_resp *)clownix_malloc(sizeof(t_item_obj_resp), 21);
  memset(pa, 0, sizeof(t_item_obj_resp));
  pa->c2c = (t_topo_c2c *) clownix_malloc(sizeof(t_topo_c2c), 22);
  memcpy(pa->c2c, c2c, sizeof(t_topo_c2c));
  clownix_timeout_add(1, timer_create_obj_resp, (void *)pa, NULL,NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void from_cloonix_switch_create_d2d(t_topo_d2d *d2d)
{
  t_item_obj_resp *pa;
  pa = (t_item_obj_resp *)clownix_malloc(sizeof(t_item_obj_resp), 21);
  memset(pa, 0, sizeof(t_item_obj_resp));
  pa->d2d = (t_topo_d2d *) clownix_malloc(sizeof(t_topo_d2d), 22);
  memcpy(pa->d2d, d2d, sizeof(t_topo_d2d));
  clownix_timeout_add(1, timer_create_obj_resp, (void *)pa, NULL,NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void from_cloonix_switch_create_a2b(t_topo_a2b *a2b)
{
  t_item_obj_resp *pa;
  pa = (t_item_obj_resp *)clownix_malloc(sizeof(t_item_obj_resp), 21);
  memset(pa, 0, sizeof(t_item_obj_resp));
  pa->a2b = (t_topo_a2b *) clownix_malloc(sizeof(t_topo_a2b), 22);
  memcpy(pa->a2b, a2b, sizeof(t_topo_a2b));
  clownix_timeout_add(1, timer_create_obj_resp, (void *)pa, NULL,NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void from_cloonix_switch_create_sat(t_topo_sat *sat)
{
  t_item_obj_resp *pa;
  pa = (t_item_obj_resp *)clownix_malloc(sizeof(t_item_obj_resp), 21);
  memset(pa, 0, sizeof(t_item_obj_resp));
  pa->sat = (t_topo_sat *) clownix_malloc(sizeof(t_topo_sat), 22);
  memcpy(pa->sat, sat, sizeof(t_topo_sat));
  clownix_timeout_add(1, timer_create_obj_resp, (void *)pa, NULL,NULL);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void from_cloonix_switch_create_lan(char *name)
{
  t_item_lan_resp *pa;
  pa = (t_item_lan_resp *) clownix_malloc(sizeof(t_item_lan_resp), 12);
  memset(pa, 0, sizeof(t_item_lan_resp));
  pa->bank_type = bank_type_lan;
  pa->mutype = mulan_type;
  strncpy(pa->name, name, MAX_NAME_LEN-1);
  clownix_timeout_add(1,timer_create_item_resp,(void *)pa,NULL,NULL);

}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void from_cloonix_switch_create_edge(char *name, int num, char *lan)
{
  t_edge_resp *pa;
  pa = (t_edge_resp *) clownix_malloc(sizeof(t_edge_resp), 12);
  memset(pa, 0, sizeof(t_edge_resp));
  strncpy(pa->name, name, MAX_NAME_LEN-1);
  strncpy(pa->lan, lan, MAX_NAME_LEN-1);
  pa->num = num;
  clownix_timeout_add(1,timer_create_edge_resp,(void *)pa,NULL,NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void from_cloonix_switch_delete_node(char *name)
{
  t_bank_item *node = look_for_node_with_id(name);
  if (!node)
    KOUT("%s", name);
  gene_delete_item(node->bank_type, name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void from_cloonix_switch_delete_sat(char *name)
{
  gene_delete_item(bank_type_sat, name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void from_cloonix_switch_delete_lan(char *lan)
{
  gene_delete_item(bank_type_lan, lan);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void from_cloonix_switch_delete_edge(char *name, int num, char *lan)
{
  t_edge_resp *pa;
  t_bank_item *intf = look_for_eth_with_id(name, num);
  t_bank_item *sat = look_for_sat_with_id(name);
  if ((intf) || (sat))
    {
    pa = (t_edge_resp *) clownix_malloc(sizeof(t_edge_resp), 12);
    memset(pa, 0, sizeof(t_edge_resp));
    strncpy(pa->name, name, MAX_NAME_LEN-1);
    strncpy(pa->lan, lan, MAX_NAME_LEN-1);
    pa->num = num;
    clownix_timeout_add(1,timer_delete_edge_resp,(void *)pa,NULL,NULL);
    }
}
/*--------------------------------------------------------------------------*/






