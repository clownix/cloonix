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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <asm/types.h>
#include <unistd.h>
#include <string.h>
#include "io_clownix.h"
#include "commun_daemon.h"
#include "rpc_clownix.h"
#include "layout_rpc.h"
#include "cfg_store.h"
#include "lan_to_name.h"
#include "layout_topo.h"
#include "endp_mngt.h"



/*****************************************************************************/
static int can_increment_index(int val)
{
  int result = 1;
  if (val >= MAX_LIST_COMMANDS_QTY)
    {
    KERR("TOO MANY COMMANDS IN LIST");
    result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_add_vm_cmd(int offset, t_list_commands *hlist, 
                            t_topo_kvm *para)
{
  int len = 0;
  int result = offset;
  t_list_commands *list = &(hlist[offset]);
  if (can_increment_index(result))
    {
    len += sprintf(list->cmd + len, 
           "cloonix_cli %s add kvm %s ram=%d cpu=%d dpdk=%d sock=%d hwsim=%d",
            cfg_get_cloonix_name(), para->name, para->mem, para->cpu,
            para->nb_dpdk, para->nb_eth, para->nb_wlan);
    len += sprintf(list->cmd + len, " %s", para->rootfs_input);
    if (para->vm_config_flags & VM_CONFIG_FLAG_PERSISTENT)
      len += sprintf(list->cmd + len, " --persistent");
    if (para->vm_config_flags & VM_CONFIG_FLAG_FULL_VIRT)
      len += sprintf(list->cmd + len, " --fullvirt");
    if (para->vm_config_flags & VM_CONFIG_FLAG_BALLOONING)
      len += sprintf(list->cmd + len, " --balloon");
    if (para->vm_config_flags & VM_CONFIG_FLAG_9P_SHARED)
      len += sprintf(list->cmd + len, " --9p_share=%s", para->p9_host_share);
    len += sprintf(list->cmd + len, " &");
    result += 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_add_tap_cmd(int offset, t_list_commands *hlist, t_endp *endp)
{
  int result = offset;
  t_list_commands *list = &(hlist[offset]);
  if (can_increment_index(result))
    {
    if (endp->endp_type == endp_type_tap)
      sprintf(list->cmd, "cloonix_cli %s add tap %s", 
                         cfg_get_cloonix_name(), endp->name);
    else if (endp->endp_type == endp_type_wif)
      sprintf(list->cmd, "cloonix_cli %s add wif %s", 
                         cfg_get_cloonix_name(), endp->name);
    else if (endp->endp_type == endp_type_raw)
      sprintf(list->cmd, "cloonix_cli %s add raw %s", 
                         cfg_get_cloonix_name(), endp->name);
    else
      KERR("%d", endp->endp_type);
    result += 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_add_c2c_cmd(int offset, t_list_commands *hlist, t_endp *endp) 
{
  int result = offset;
  t_list_commands *list = &(hlist[offset]);
  if (can_increment_index(result))
    {
    sprintf(list->cmd, "cloonix_cli %s add c2c %s %s", 
            cfg_get_cloonix_name(), endp->name, 
            endp->c2c.slave_cloonix);
    result += 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_add_snf_cmd(int offset, t_list_commands *hlist, t_endp *endp)
{
  int result = offset;
  t_list_commands *list = &(hlist[offset]);
  if (can_increment_index(result))
    {
    sprintf(list->cmd, "cloonix_cli %s add snf %s", 
                       cfg_get_cloonix_name(), endp->name);
    result += 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_add_a2b_cmd(int offset, t_list_commands *hlist, t_endp *endp)
{
  int result = offset;
  t_list_commands *list = &(hlist[offset]);
  if (can_increment_index(result))
    {
    sprintf(list->cmd, "cloonix_cli %s add a2b %s",
                       cfg_get_cloonix_name(), endp->name);
    result += 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_add_nat_cmd(int offset, t_list_commands *hlist, t_endp *endp)
{
  int result = offset;
  t_list_commands *list = &(hlist[offset]);
  if (can_increment_index(result))
    {
    sprintf(list->cmd, "cloonix_cli %s add nat %s",
                       cfg_get_cloonix_name(), endp->name);
    result += 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_add_sat_lan_cmd(int offset, t_list_commands *hlist, 
                                 char *name, int num, char *lan)
{
  int result = offset;
  t_list_commands *list = &(hlist[offset]);
  if (can_increment_index(result))
    {
    sprintf(list->cmd, "cloonix_cli %s add lan %s %d %s", 
                        cfg_get_cloonix_name(), name, num, lan);
    result += 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_stop_go_cmd(int offset, t_list_commands *hlist, int go)
{
  int result = offset;
  t_list_commands *list = &(hlist[offset]);
  if (can_increment_index(result))
    {
    if (go)
      sprintf(list->cmd, "cloonix_cli %s cnf lay go", cfg_get_cloonix_name());
    else
      sprintf(list->cmd, "cloonix_cli %s cnf lay stop", cfg_get_cloonix_name());
    result += 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_width_height_cmd(int offset, t_list_commands *hlist, 
                                  int width, int height)
{
  int result = offset;
  t_list_commands *list = &(hlist[offset]);
  if (width && height)
    {
    if (can_increment_index(result))
      {
      sprintf(list->cmd, "cloonix_cli %s cnf lay width_height %d %d", 
                         cfg_get_cloonix_name(), width, height);
      result += 1;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_center_scale_cmd(int offset, t_list_commands *hlist,
                                  int cx, int cy, int cw, int ch)
{
  int result = offset;
  t_list_commands *list = &(hlist[offset]);
  if (cw && ch)
    {
    if (can_increment_index(result))
      {
      sprintf(list->cmd, "cloonix_cli %s cnf lay scale %d %d %d %d", 
                         cfg_get_cloonix_name(), cx, cy, cw, ch);
      result += 1;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_layout_eth(int offset, t_list_commands *hlist, 
                            char *name, int num, t_layout_eth_wlan *eth_wlan)
{
  int result = offset;
  t_list_commands *list = &(hlist[result]);
  if (can_increment_index(result))
    {
    sprintf(list->cmd, "cloonix_cli %s cnf lay abs_xy_eth %s %d %d",
                        cfg_get_cloonix_name(), name, num, 
                        layout_node_solve(eth_wlan->x, eth_wlan->y));
    result += 1;
    if (eth_wlan->hidden_on_graph)
      {
      if (can_increment_index(result))
        {
        list = &(hlist[result]);
        sprintf(list->cmd, "cloonix_cli %s cnf lay hide_eth %s %d 1", 
                           cfg_get_cloonix_name(), name, num);
        result += 1;
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_layout_node(int offset, t_list_commands *hlist, 
                             t_layout_node *node)
{
  int i, result = offset;
  t_list_commands *list = &(hlist[result]);

  if (can_increment_index(result))
    {
    sprintf(list->cmd, "cloonix_cli %s cnf lay abs_xy_kvm %s %d %d",
                       cfg_get_cloonix_name(), node->name, 
                       (int) node->x, (int) node->y);
    result += 1;
    if (node->hidden_on_graph)
      {
      if (can_increment_index(result))
        {
        list = &(hlist[result]);
        sprintf(list->cmd, "cloonix_cli %s cnf lay hide_kvm %s 1", 
                           cfg_get_cloonix_name(), node->name);
        result += 1;
        }
      }
    }
  for (i=0; i < node->nb_eth_wlan; i++)
    result = build_layout_eth(result, hlist, node->name, i, &(node->eth_wlan[i])); 
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_layout_sat(int offset, t_list_commands *hlist, 
                            t_layout_sat *sat)
{
  int result = offset;
  t_list_commands *list = &(hlist[result]);
  if (can_increment_index(result))
    {
    sprintf(list->cmd, "cloonix_cli %s cnf lay abs_xy_sat %s %d %d",
                       cfg_get_cloonix_name(), sat->name, 
                       (int) sat->x, (int) sat->y);
    result += 1;
    if (sat->hidden_on_graph)
      {
      if (can_increment_index(result))
        {
        list = &(hlist[result]);
        sprintf(list->cmd, "cloonix_cli %s cnf lay hide_sat %s 1", 
                           cfg_get_cloonix_name(), sat->name);
        result += 1;
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_layout_lan(int offset, t_list_commands *hlist, 
                            t_layout_lan *lan)
{
  int result = offset;
  t_list_commands *list = &(hlist[result]);
  if (can_increment_index(result))
    {
    sprintf(list->cmd, "cloonix_cli %s cnf lay abs_xy_lan %s %d %d",
                       cfg_get_cloonix_name(), lan->name, 
                       (int) lan->x, (int) lan->y);
    result += 1;
    if (lan->hidden_on_graph)
      {
      if (can_increment_index(result))
        {
        list = &(hlist[result]);
        sprintf(list->cmd, "cloonix_cli %s cnf lay hide_lan %s 1", 
                           cfg_get_cloonix_name(), lan->name);
        result += 1;
        }
      }
   }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int produce_sleep_line(int offset, t_list_commands *hlist, int sec)
{
  int result = offset;
  t_list_commands *list;
  if (can_increment_index(result))
    {
    list = &(hlist[result]);
    sprintf(list->cmd, "sleep %d", sec);
    result += 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int produce_list_vm_cmd(int offset, t_list_commands *hlist,
                               int nb_vm, t_vm *vm) 
{
  int i, result = offset;
  for (i=0; i<nb_vm; i++)
    {
    result = build_add_vm_cmd(result, hlist, &(vm->kvm));
    result = produce_sleep_line(result, hlist, 5);
    vm = vm->next;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int produce_list_sat_cmd(int offset, t_list_commands *hlist,
                                int nb_endp, t_endp *endp)
{
  t_endp *next, *cur = endp;
  int i, result = offset;
  for (i=0; i<nb_endp; i++)
    {
    if (!cur)
      KOUT(" ");
    if ((cur->endp_type == endp_type_tap) ||
        (cur->endp_type == endp_type_raw) ||
        (cur->endp_type == endp_type_wif))
      {
      result = build_add_tap_cmd(result, hlist, cur);
      }
    else if (cur->endp_type == endp_type_c2c)
      {
      if (cur->c2c.local_is_master)
        result = build_add_c2c_cmd(result, hlist, cur);
      }
    else if (cur->endp_type == endp_type_snf)
      {
      result = build_add_snf_cmd(result, hlist, cur);
      }
    else if (cur->endp_type == endp_type_a2b)
      {
      if (cur->num == 0)
        result = build_add_a2b_cmd(result, hlist, cur);
      }
    else if (cur->endp_type == endp_type_nat)
      {
      result = build_add_nat_cmd(result, hlist, cur);
      }
    else if ((cur->endp_type == endp_type_kvm_eth) ||
             (cur->endp_type == endp_type_kvm_wlan))
      {
      }
    else
      KERR("%s %d", cur->name, cur->num); 
    next = endp_mngt_get_next(cur);
    clownix_free(cur, __FILE__);
    cur = next;
    }
  if (cur)
    KOUT(" ");
  result = produce_sleep_line(result, hlist, 5);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int produce_list_lan_cmd(int offset, t_list_commands *hlist,
                                int nb_endp, t_endp *endp)
{
  t_endp *next, *cur = endp;
  int i, j, lan, result = offset;
  for (i=0; i<nb_endp; i++)
    {
    if (!cur)
      KOUT(" ");

    for (j=0; j<MAX_TRAF_ENDPOINT; j++)
      {
      lan = cur->lan_attached[j].lan_num;
      if (lan)
        {
        if (!lan_get_with_num(lan))
          KOUT(" ");
        result = build_add_sat_lan_cmd(result, hlist,
                                       cur->name, cur->num,
                                       lan_get_with_num(lan));
        }
      }
    next = endp_mngt_get_next(cur);
    clownix_free(cur, __FILE__);
    cur = next;
    }
  if (cur)
    KOUT(" ");
  return result;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static int produce_list_canvas_layout_cmd(int offset, t_list_commands *hlist, 
                                          int go, int width, int height, 
                                          int cx, int cy, int cw, int ch)
{
  int result = offset;
  result = build_stop_go_cmd(result, hlist, go);
  result = produce_sleep_line(result, hlist, 1);
  result = build_width_height_cmd(result, hlist, width, height);
  result = produce_sleep_line(result, hlist, 1);
  result = build_center_scale_cmd(result, hlist, cx, cy, cw, ch);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int produce_list_layout_node_cmd(int offset, t_list_commands *hlist,
                                        t_layout_node_xml *node_xml)
{
  int result = offset;
  t_layout_node_xml *cur = node_xml;
  while(cur)
    {
    result = build_layout_node(result, hlist, &(cur->node));
    cur = cur->next;
    }
  return result;
} 
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int produce_list_layout_sat_cmd(int offset, t_list_commands *hlist,
                                       t_layout_sat_xml *sat_xml)    
{
  int result = offset;
  t_layout_sat_xml *cur = sat_xml;
  while(cur)
    {
    result = build_layout_sat(result, hlist, &(cur->sat));
    cur = cur->next;
    }
  return result;

}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int produce_list_layout_lan_cmd(int offset, t_list_commands *hlist,
                                       t_layout_lan_xml *lan_xml)    
{
  int result = offset;
  t_layout_lan_xml *cur = lan_xml;
  while(cur)
    {
    result = build_layout_lan(result, hlist, &(cur->lan));
    cur = cur->next;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int produce_first_lines(int offset, t_list_commands *hlist)
{
  int result = offset;
  t_list_commands *list;
  if (can_increment_index(result))
    {
    list = &(hlist[result]);
    sprintf(list->cmd, "#!/bin/bash");
    result += 1;
    }
  if (can_increment_index(result))
    {
    list = &(hlist[result]);
    sprintf(list->cmd, "cloonix_net %s", cfg_get_cloonix_name());
    result += 1;
    }
  if (can_increment_index(result))
    {
    list = &(hlist[result]);
    sprintf(list->cmd, "sleep 2");
    result += 1;
    }
  if (can_increment_index(result))
    {
    list = &(hlist[result]);
    sprintf(list->cmd, "cloonix_gui %s", cfg_get_cloonix_name());
    result += 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int produce_last_lines(int offset, t_list_commands *hlist)
{
  int result = offset;
  t_list_commands *list;
  if (can_increment_index(result))
    {
    list = &(hlist[result]);
    sprintf(list->cmd, "echo END");
    result += 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int produce_list_commands(t_list_commands *hlist)
{
  int nb_vm, nb_endp, result = 0;
  t_vm  *vm  = cfg_get_first_vm(&nb_vm);
  t_endp *endp;
  int go, width, height, cx, cy, cw, ch;
  t_layout_xml *layout_xml;

  result = produce_first_lines(result, hlist);
  result = produce_list_vm_cmd(result, hlist, nb_vm, vm); 
  endp = endp_mngt_get_first(&nb_endp);
  result = produce_list_sat_cmd(result, hlist, nb_endp, endp);
  endp = endp_mngt_get_first(&nb_endp);
  result = produce_list_lan_cmd(result, hlist, nb_endp, endp);
  get_layout_main_params(&go, &width, &height, &cx, &cy, &cw, &ch);
  result = produce_list_canvas_layout_cmd(result, hlist, go, 
                                               width, height, 
                                               cx, cy, cw, ch);
  layout_xml = get_layout_xml_chain();
  result = produce_list_layout_node_cmd(result, hlist, layout_xml->node_xml);
  result = produce_list_layout_sat_cmd(result, hlist, layout_xml->sat_xml);
  result = produce_list_layout_lan_cmd(result, hlist, layout_xml->lan_xml);
  result = produce_last_lines(result, hlist);
  return result;
}
/*---------------------------------------------------------------------------*/


