/*****************************************************************************/
/*    Copyright (C) 2006-2024 clownix@clownix.net License AGPL-3             */
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
#include "interface.h"
#include "bank.h"
#include "move.h"
#include "lib_topo.h"



/****************************************************************************/
void process_all_diffs(t_topo_differences *diffs)
{
  t_topo_cnt_chain   *add_cnt  = diffs->add_cnt;
  t_topo_kvm_chain   *add_kvm  = diffs->add_kvm;
  t_topo_c2c_chain   *add_c2c  = diffs->add_c2c;
  t_topo_tap_chain   *add_tap  = diffs->add_tap;
  t_topo_a2b_chain   *add_a2b  = diffs->add_a2b;
  t_topo_nat_chain   *add_nat  = diffs->add_nat;
  t_topo_phy_chain   *add_phy  = diffs->add_phy;
  t_topo_lan_chain   *add_lan  = diffs->add_lan;
  t_topo_edge_chain  *add_edge = diffs->add_edge;

  t_topo_cnt_chain   *del_cnt  = diffs->del_cnt;
  t_topo_kvm_chain   *del_kvm  = diffs->del_kvm;
  t_topo_c2c_chain   *del_c2c  = diffs->del_c2c;
  t_topo_tap_chain   *del_tap  = diffs->del_tap;
  t_topo_a2b_chain   *del_a2b  = diffs->del_a2b;
  t_topo_nat_chain   *del_nat  = diffs->del_nat;
  t_topo_phy_chain   *del_phy  = diffs->del_phy;
  t_topo_lan_chain   *del_lan  = diffs->del_lan;
  t_topo_edge_chain  *del_edge = diffs->del_edge;

  while(add_cnt)
    {
    from_cloonix_switch_create_cnt(&(add_cnt->cnt));
    add_cnt = add_cnt->next;
    }

  while(add_kvm)
    {
    from_cloonix_switch_create_node(&(add_kvm->kvm));
    add_kvm = add_kvm->next;
    }

  while(add_c2c)
    {
    from_cloonix_switch_create_c2c(&(add_c2c->c2c));
    add_c2c = add_c2c->next;
    }

  while(add_tap)
    {
    from_cloonix_switch_create_tap(&(add_tap->tap));
    add_tap = add_tap->next;
    }

  while(add_a2b)
    {
    from_cloonix_switch_create_a2b(&(add_a2b->a2b));
    add_a2b = add_a2b->next;
    }

  while(add_nat)
    {
    from_cloonix_switch_create_nat(&(add_nat->nat));
    add_nat = add_nat->next;
    }

  while(add_phy)
    {
    from_cloonix_switch_create_phy(&(add_phy->phy));
    add_phy = add_phy->next;
    }

  while(add_lan)
    {
    if (look_for_lan_with_id(add_lan->lan) == NULL)
      from_cloonix_switch_create_lan(add_lan->lan);
    add_lan = add_lan->next;
    }

  while(add_edge)
    {
    from_cloonix_switch_create_edge(add_edge->name,add_edge->num,add_edge->lan);
    add_edge = add_edge->next;
    }

  while(del_edge)
    {
    from_cloonix_switch_delete_edge(del_edge->name,del_edge->num,del_edge->lan);
    del_edge = del_edge->next;
    }

  while(del_cnt)
    {
    from_cloonix_switch_delete_cnt(del_cnt->cnt.name);
    del_cnt = del_cnt->next;
    }

  while(del_kvm)
    {
    from_cloonix_switch_delete_node(del_kvm->kvm.name);
    del_kvm = del_kvm->next;
    }

  while(del_c2c)
    {
    from_cloonix_switch_delete_sat(del_c2c->c2c.name);
    del_c2c = del_c2c->next;
    }

  while(del_tap)
    {
    from_cloonix_switch_delete_sat(del_tap->tap.name);
    del_tap = del_tap->next;
    }

  while(del_a2b)
    {
    from_cloonix_switch_delete_sat(del_a2b->a2b.name);
    del_a2b = del_a2b->next;
    }

  while(del_nat)
    {
    from_cloonix_switch_delete_sat(del_nat->nat.name);
    del_nat = del_nat->next;
    }

  while(del_phy)
    {
    from_cloonix_switch_delete_sat(del_phy->phy.name);
    del_phy = del_phy->next;
    }

  while(del_lan)
    {
    from_cloonix_switch_delete_lan(del_lan->lan);
    del_lan = del_lan->next;
    }
}
/*--------------------------------------------------------------------------*/








