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
  t_topo_kvm_chain   *add_kvm = diffs->add_kvm;
  t_topo_kvm_chain   *del_kvm = diffs->del_kvm;
  t_topo_c2c_chain   *add_c2c = diffs->add_c2c;
  t_topo_c2c_chain   *del_c2c = diffs->del_c2c;
  t_topo_snf_chain   *add_snf = diffs->add_snf;
  t_topo_snf_chain   *del_snf = diffs->del_snf;
  t_topo_sat_chain   *add_sat = diffs->add_sat;
  t_topo_sat_chain   *del_sat = diffs->del_sat;
  t_topo_lan_chain   *add_lan = diffs->add_lan;
  t_topo_lan_chain   *del_lan = diffs->del_lan;
  t_topo_edge_chain *add_edge = diffs->add_edge;
  t_topo_edge_chain *del_edge = diffs->del_edge;

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

  while(add_snf)
    {
    from_cloonix_switch_create_snf(&(add_snf->snf));
    add_snf = add_snf->next;
    }

  while(add_sat)
    {
    from_cloonix_switch_create_sat(&(add_sat->sat));
    add_sat = add_sat->next;
    }

  while(add_lan)
    {
    if (look_for_lan_with_id(add_lan->lan) == NULL)
      {
      from_cloonix_switch_create_lan(add_lan->lan);
      }
    add_lan = add_lan->next;
    }

  while(add_edge)
    {
    from_cloonix_switch_create_edge(add_edge->name, add_edge->num, add_edge->lan);
    add_edge = add_edge->next;
    }

  while(del_edge)
    {
    from_cloonix_switch_delete_edge(del_edge->name, del_edge->num, del_edge->lan);
    del_edge = del_edge->next;
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

  while(del_snf)
    {
    from_cloonix_switch_delete_sat(del_snf->snf.name);
    del_snf = del_snf->next;
    }

  while(del_sat)
    {
    from_cloonix_switch_delete_sat(del_sat->sat.name);
    del_sat = del_sat->next;
    }

  while(del_lan)
    {
    from_cloonix_switch_delete_lan(del_lan->lan);
    del_lan = del_lan->next;
    }
}
/*--------------------------------------------------------------------------*/








