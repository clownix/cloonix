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
void from_cloonix_switch_create_cnt(t_topo_cnt *cnt)
{
  t_item_obj_resp *pa;
  pa = (t_item_obj_resp *)clownix_malloc(sizeof(t_item_obj_resp), 21);
  memset(pa, 0, sizeof(t_item_obj_resp));
  pa->cnt = (t_topo_cnt *) clownix_malloc(sizeof(t_topo_cnt), 22);
  memcpy(pa->cnt, cnt, sizeof(t_topo_cnt));
  timer_create_obj_resp((void *)pa);
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
  timer_create_obj_resp((void *)pa);
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
  timer_create_obj_resp((void *)pa);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void from_cloonix_switch_create_nat(t_topo_nat *nat)
{
  t_item_obj_resp *pa;
  pa = (t_item_obj_resp *)clownix_malloc(sizeof(t_item_obj_resp), 21);
  memset(pa, 0, sizeof(t_item_obj_resp));
  pa->nat = (t_topo_nat *) clownix_malloc(sizeof(t_topo_nat), 22);
  memcpy(pa->nat, nat, sizeof(t_topo_nat));
  timer_create_obj_resp((void *)pa);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void from_cloonix_switch_create_phy(t_topo_phy *phy)
{
  t_item_obj_resp *pa;
  pa = (t_item_obj_resp *)clownix_malloc(sizeof(t_item_obj_resp), 21);
  memset(pa, 0, sizeof(t_item_obj_resp));
  pa->phy = (t_topo_phy *) clownix_malloc(sizeof(t_topo_phy), 22);
  memcpy(pa->phy, phy, sizeof(t_topo_phy));
  timer_create_obj_resp((void *)pa);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void from_cloonix_switch_create_tap(t_topo_tap *tap)
{
  t_item_obj_resp *pa;
  pa = (t_item_obj_resp *)clownix_malloc(sizeof(t_item_obj_resp), 21);
  memset(pa, 0, sizeof(t_item_obj_resp));
  pa->tap = (t_topo_tap *) clownix_malloc(sizeof(t_topo_tap), 22);
  memcpy(pa->tap, tap, sizeof(t_topo_tap));
  timer_create_obj_resp((void *)pa);
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
  timer_create_obj_resp((void *)pa);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void from_cloonix_switch_create_lan(char *name)
{
  t_item_lan_resp *pa;
  pa = (t_item_lan_resp *) clownix_malloc(sizeof(t_item_lan_resp), 12);
  memset(pa, 0, sizeof(t_item_lan_resp));
  pa->bank_type = bank_type_lan;
  strncpy(pa->name, name, MAX_NAME_LEN-1);
  timer_create_item_resp((void *)pa);
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
  timer_create_edge_resp((void *)pa);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void from_cloonix_switch_delete_node(char *name)
{
  t_bank_item *bitem = look_for_node_with_id(name);
  if (bitem)
    gene_delete_item(bitem->bank_type, name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void from_cloonix_switch_delete_cnt(char *name)
{
  t_bank_item *bitem = look_for_cnt_with_id(name);
  if (bitem)
    gene_delete_item(bitem->bank_type, name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void from_cloonix_switch_delete_sat(char *name)
{
  t_bank_item *bitem = look_for_sat_with_id(name);
  if (bitem)
    gene_delete_item(bitem->bank_type, name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void from_cloonix_switch_delete_lan(char *name)
{
  t_bank_item *bitem = look_for_lan_with_id(name);
  if (bitem)
    gene_delete_item(bitem->bank_type, name);
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






