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
#include "ovs.h"
#include "cnt.h"
#include "kvm.h"
#include "ovs_c2c.h"
#include "ovs_a2b.h"
#include "ovs_tap.h"
#include "ovs_phy.h"
#include "ovs_nat.h"



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

/***************************************************************************/
static void eth_tab_to_str(char *out, int nb_tot_eth, t_eth_table *eth_tab)
{
  int i;
  memset(out, 0, MAX_NAME_LEN);
  for (i=0 ; i < nb_tot_eth; i++)
    {
    if (i > MAX_ETH_VM)
      KOUT("%d", nb_tot_eth); 
    if(eth_tab[i].endp_type == endp_type_eths)
      out[i] = 's';
    else if(eth_tab[i].endp_type == endp_type_ethv)
      out[i] = 'v';
    else
      KOUT("%d %d", i, nb_tot_eth); 
    }
}
/*-------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_add_vm_cmd(int offset, t_list_commands *hlist, 
                            t_topo_kvm *kvm)
{
  int i, len = 0;
  int result = offset;
  t_list_commands *list = &(hlist[offset]);
  char eth_desc[MAX_NAME_LEN];
  unsigned char *mc;
  if (can_increment_index(result))
    {
    eth_tab_to_str(eth_desc, kvm->nb_tot_eth, kvm->eth_table);
    len += sprintf(list->cmd + len, 
           "cloonix_cli %s add kvm %s ram=%d cpu=%d eth=%s",
            cfg_get_cloonix_name(), kvm->name, kvm->mem, kvm->cpu, eth_desc);
    len += sprintf(list->cmd + len, " %s", kvm->rootfs_input);
    if (kvm->vm_config_flags & VM_CONFIG_FLAG_PERSISTENT)
      len += sprintf(list->cmd + len, " --persistent");
    if (kvm->vm_config_flags & VM_CONFIG_FLAG_FULL_VIRT)
      len += sprintf(list->cmd + len, " --fullvirt");
    if (kvm->vm_config_flags & VM_CONFIG_FLAG_NO_REBOOT)
      len += sprintf(list->cmd + len, " --no_reboot");
    if (kvm->vm_config_flags & VM_CONFIG_FLAG_WITH_PXE)
      len += sprintf(list->cmd + len, " --with_pxe");
    if (kvm->vm_config_flags & VM_CONFIG_FLAG_BALLOONING)
      len += sprintf(list->cmd + len, " --balloon");
    if (kvm->vm_config_flags & VM_CONFIG_FLAG_INSTALL_CDROM)
      len += sprintf(list->cmd + len, " --install_cdrom=%s",
                                         kvm->install_cdrom);
    if (kvm->vm_config_flags & VM_CONFIG_FLAG_ADDED_CDROM)
      len += sprintf(list->cmd + len, " --added_cdrom=%s",
                                         kvm->added_cdrom);
    if (kvm->vm_config_flags & VM_CONFIG_FLAG_ADDED_DISK)
      len += sprintf(list->cmd + len, " --added_disk=%s",
                                         kvm->added_disk);
    for (i=0; i < kvm->nb_tot_eth; i++)
      {
      if (kvm->eth_table[i].randmac == 0)
        {
        mc = kvm->eth_table[i].mac_addr;
	len += sprintf(list->cmd + len,
        " --mac_addr=eth%d:%02x:%02x:%02x:%02x:%02x:%02x", i,
                           mc[0]&0xff, mc[1]&0xff, mc[2]&0xff,
                           mc[3]&0xff, mc[4]&0xff, mc[5]&0xff);
        }
      }
    result += 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_add_cnt_cmd(int offset, t_list_commands *hlist,
                             t_topo_cnt *cnt)
{
  int i, len = 0;
  int result = offset;
  t_list_commands *list = &(hlist[offset]);
  char eth_desc[MAX_NAME_LEN];
  char shortbrand[MAX_NAME_LEN];
  unsigned char *mc;
  if (can_increment_index(result))
    {
    eth_tab_to_str(eth_desc, cnt->nb_tot_eth, cnt->eth_table);
    if (!strcmp(cnt->brandtype, "crun"))
      strcpy(shortbrand, "cru");
    else
      KOUT("ERROR %s", cnt->brandtype);
    len += sprintf(list->cmd + len, "cloonix_cli %s add %s %s eth=%s %s",
            cfg_get_cloonix_name(), shortbrand, cnt->name, eth_desc, cnt->image);
    if (cnt->is_persistent)
      len += sprintf(list->cmd + len, " --persistent");
    for (i=0; i < cnt->nb_tot_eth; i++)
      {
      if (cnt->eth_table[i].randmac == 0)
        {
        mc = cnt->eth_table[i].mac_addr;
        len += sprintf(list->cmd + len,
        " --mac_addr=eth%d:%02x:%02x:%02x:%02x:%02x:%02x", i,
                           mc[0]&0xff, mc[1]&0xff, mc[2]&0xff,
                           mc[3]&0xff, mc[4]&0xff, mc[5]&0xff);
        }
      }
    result += 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static int build_add_sat_cmd(int offset, t_list_commands *hlist,
                             char *sat_type, char *name)
{
  int len = 0, result = offset;;
  t_list_commands *list = &(hlist[offset]);
  if (can_increment_index(result))
    {
    len += sprintf(list->cmd + len, "cloonix_cli %s add %s %s", 
                   cfg_get_cloonix_name(), sat_type, name);
    result += 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_add_c2c_cmd(int offset, t_list_commands *hlist, 
                             char *name, char *dist_cloon)
{
  int len = 0, result = offset;;
  t_list_commands *list = &(hlist[offset]);
  if (can_increment_index(result))
    {
    len += sprintf(list->cmd + len, "cloonix_cli %s add c2c %s %s",
                   cfg_get_cloonix_name(), name, dist_cloon);
    result += 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_add_lan_kvm_cmd(int offset, t_list_commands *hlist)
{
  t_ethv_cnx *cur = get_first_head_ethv();
  int result = offset;
  t_list_commands *list;
  while(cur)
    {
    if (cur->attached_lan_ok)
      {
      if (can_increment_index(result))
        {
        list = &(hlist[result]);
        sprintf(list->cmd, "cloonix_cli %s add lan %s %d %s",
        cfg_get_cloonix_name(), cur->name, cur->num, cur->lan);
        result += 1;
        }
      }
    cur = cur->next;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_add_lan_cnt_cmd(int offset, t_list_commands *hlist)
{
  int i, nb;
  t_cnt *cur = cnt_get_first_cnt(&nb);
  int result = offset;
  t_list_commands *list;
  while(cur)
    {
    for (i=0; i<cur->cnt.nb_tot_eth; i++)
      {
      if (cur->att_lan[i].lan_attached_ok)
        {
        if (can_increment_index(result))
          {
          list = &(hlist[result]);
          sprintf(list->cmd, "cloonix_cli %s add lan %s %d %s",
          cfg_get_cloonix_name(), cur->cnt.name, i, cur->att_lan[i].lan);
          result += 1;
          }
        }
      }
    cur = cur->next;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_add_lan_tap_cmd(int offset, t_list_commands *hlist)
{
  int nb;
  t_ovs_tap *cur = ovs_tap_get_first(&nb);
  int result = offset;
  t_list_commands *list;
  while(cur)
    {
    if (strlen(cur->lan))
      {
      if (can_increment_index(result))
        {
        list = &(hlist[result]);
        sprintf(list->cmd, "cloonix_cli %s add lan %s 0 %s",
        cfg_get_cloonix_name(), cur->name, cur->lan);
        result += 1;
        }
      }
    cur = cur->next;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_add_lan_phy_cmd(int offset, t_list_commands *hlist)
{
  int nb;
  t_ovs_phy *cur = ovs_phy_get_first(&nb);
  int result = offset;
  t_list_commands *list;
  while(cur)
    {
    if (strlen(cur->lan))
      {
      if (can_increment_index(result))
        {
        list = &(hlist[result]);
        sprintf(list->cmd, "cloonix_cli %s add lan %s 0 %s",
        cfg_get_cloonix_name(), cur->name, cur->lan);
        result += 1;
        }
      }
    cur = cur->next;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_add_lan_nat_cmd(int offset, t_list_commands *hlist)
{
  int nb;
  t_ovs_nat *cur = ovs_nat_get_first(&nb);
  int result = offset;
  t_list_commands *list;
  while(cur)
    {
    if (strlen(cur->lan))
      {
      if (can_increment_index(result))
        {
        list = &(hlist[result]);
        sprintf(list->cmd, "cloonix_cli %s add lan %s 0 %s",
        cfg_get_cloonix_name(), cur->name, cur->lan);
        result += 1;
        }
      }
    cur = cur->next;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_add_lan_c2c_cmd(int offset, t_list_commands *hlist)
{
  int nb;
  t_ovs_c2c *cur = ovs_c2c_get_first(&nb);
  int result = offset;
  t_list_commands *list;
  while(cur)
    {
    if (strlen(cur->topo.lan))
      {
      if (can_increment_index(result))
        {
        list = &(hlist[result]);
        sprintf(list->cmd, "cloonix_cli %s add lan %s 0 %s",
        cfg_get_cloonix_name(), cur->name, cur->topo.lan);
        result += 1;
        }
      } 
    cur = cur->next;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_add_lan_a2b_cmd(int offset, t_list_commands *hlist)
{
  int i, nb;
  t_ovs_a2b *cur = ovs_a2b_get_first(&nb);
  int result = offset;
  t_list_commands *list;
  while(cur)
    {
    for (i=0; i<2; i++)
      {
      if (strlen(cur->side[i].lan))
        {
        if (can_increment_index(result))
          {
          list = &(hlist[result]);
          sprintf(list->cmd, "cloonix_cli %s add lan %s %d %s",
          cfg_get_cloonix_name(), cur->name, i, cur->side[i].lan);
          result += 1;
          }
        } 
      } 
    cur = cur->next;
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
                            char *name, int num, t_layout_eth *eth)
{
  int result = offset;
  t_list_commands *list = &(hlist[result]);
  if (can_increment_index(result))
    {
    sprintf(list->cmd, "cloonix_cli %s cnf lay abs_xy_eth %s %d %d",
                        cfg_get_cloonix_name(), name, num, 
                        layout_node_solve(name, eth->x, eth->y));
    result += 1;
    if (eth->hidden_on_graph)
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
  for (i=0; i < node->nb_eth; i++)
    result = build_layout_eth(result, hlist, node->name, i, &(node->eth[i])); 
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
    if (sat->is_a2b)
      {
      if (can_increment_index(result))
        {
        list = &(hlist[result]);
        sprintf(list->cmd, "cloonix_cli %s cnf lay abs_xy_eth %s 0 %d",
                            cfg_get_cloonix_name(), sat->name, 
                            layout_a2b_solve(sat->xa, sat->ya));
        result += 1;
        }
      if (can_increment_index(result))
        {
        list = &(hlist[result]);
        sprintf(list->cmd, "cloonix_cli %s cnf lay abs_xy_eth %s 1 %d",
                            cfg_get_cloonix_name(), sat->name, 
                            layout_a2b_solve(sat->xb, sat->yb));
        result += 1;
        }
      }
    }
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
  result = build_width_height_cmd(result, hlist, width, height);
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
int produce_list_commands(t_list_commands *hlist, int is_layout)
{
  int i, nb_vm, nb_cnt, result = 0;
  int nb_nat, nb_a2b, nb_c2c, nb_tap, nb_phy;
  t_vm  *vm  = cfg_get_first_vm(&nb_vm);
  t_cnt *cnt = cnt_get_first_cnt(&nb_cnt);
  int go, width, height, cx, cy, cw, ch;
  t_layout_xml *layout_xml;
  t_ovs_nat *nat = ovs_nat_get_first(&nb_nat);
  t_ovs_a2b *a2b = ovs_a2b_get_first(&nb_a2b);
  t_ovs_c2c *c2c = ovs_c2c_get_first(&nb_c2c);
  t_ovs_tap *tap = ovs_tap_get_first(&nb_tap);
  t_ovs_phy *phy = ovs_phy_get_first(&nb_phy);

  if (is_layout == 0)
    {
    for (i=0; i<nb_vm; i++)
      {
      result = build_add_vm_cmd(result, hlist, &(vm->kvm));
      vm = vm->next;
      }
    for (i=0; i<nb_cnt; i++)
      {
      result = build_add_cnt_cmd(result, hlist, &(cnt->cnt));
      cnt = cnt->next;
      }
    while(tap)
      {
      result = build_add_sat_cmd(result, hlist, "tap", tap->name);
      tap = tap->next;
      }
    while(phy)
      {
      result = build_add_sat_cmd(result, hlist, "phy", phy->name);
      phy = phy->next;
      }
    while(nat)
      {
      result = build_add_sat_cmd(result, hlist, "nat", nat->name);
      nat = nat->next;
      }
    while(a2b)
      {
      result = build_add_sat_cmd(result, hlist, "a2b", a2b->name);
      a2b = a2b->next;
      }
    while(c2c)
      {
      result = build_add_c2c_cmd(result, hlist, c2c->topo.name,
                                 c2c->topo.dist_cloon);
      c2c = c2c->next;
      }
    result = build_add_lan_kvm_cmd(result, hlist);
    result = build_add_lan_cnt_cmd(result, hlist);
    result = build_add_lan_tap_cmd(result, hlist);
    result = build_add_lan_phy_cmd(result, hlist);
    result = build_add_lan_nat_cmd(result, hlist);
    result = build_add_lan_a2b_cmd(result, hlist);
    result = build_add_lan_c2c_cmd(result, hlist);
    }
  else
    {
    get_layout_main_params(&go, &width, &height, &cx, &cy, &cw, &ch);
    layout_xml = get_layout_xml_chain();
    result = produce_list_layout_node_cmd(result, hlist, layout_xml->node_xml);
    result = produce_list_layout_lan_cmd(result, hlist, layout_xml->lan_xml);
    result = produce_list_layout_sat_cmd(result, hlist, layout_xml->sat_xml);
    result = produce_list_canvas_layout_cmd(result, hlist, go, width, height, 
                                                              cx, cy, cw, ch);
    }
  return result;
}
/*---------------------------------------------------------------------------*/


