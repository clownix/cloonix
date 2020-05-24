/*****************************************************************************/
/*    Copyright (C) 2006-2020 clownix@clownix.net License AGPL-3             */
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
#include <math.h>
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "bank.h"
#include "bank_item.h"
#include "external_bank.h"
#include "commun_consts.h"
#include "popup.h"
#include "eventfull_eth.h"
#include "hidden_visible_edge.h"

typedef struct t_edge_update_all
{
  int bank_type;
  char name[MAX_PATH_LEN];
  int num;
} t_edge_update_all;


void snf_add_capture_on(t_bank_item *bitem);
void topo_bitem_hide(t_bank_item *bitem);
void topo_bitem_show(t_bank_item *bitem);


static t_bank_item *currently_in_item_surface;

static void attached_obj_associations_delete(t_bank_item *bitem);

/****************************************************************************/
int is_a_snf(t_bank_item *bitem)
{
  int result;
  if ((bitem->pbi.mutype == endp_type_tap) ||
      (bitem->pbi.mutype == endp_type_phy) ||
      (bitem->pbi.mutype == endp_type_pci) ||
      (bitem->pbi.mutype == endp_type_wif) ||
      (bitem->pbi.mutype == endp_type_c2c) ||
      (bitem->pbi.mutype == endp_type_nat) ||
      (bitem->pbi.mutype == endp_type_a2b))
    {
    result = 0;
    }
  else if (bitem->pbi.mutype == endp_type_snf)
    {
    result = 1;
    }
  else
    KOUT("%d", bitem->pbi.mutype);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int is_a_nat(t_bank_item *bitem)
{
  int result;
  if ((bitem->pbi.mutype == endp_type_tap) ||
      (bitem->pbi.mutype == endp_type_phy) ||
      (bitem->pbi.mutype == endp_type_pci) ||
      (bitem->pbi.mutype == endp_type_wif) ||
      (bitem->pbi.mutype == endp_type_c2c) ||
      (bitem->pbi.mutype == endp_type_snf) ||
      (bitem->pbi.mutype == endp_type_a2b))
    {
    result = 0;
    }
  else if (bitem->pbi.mutype == endp_type_nat)
    {
    result = 1;
    }
  else
    KOUT("%d", bitem->pbi.mutype);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int is_a_c2c(t_bank_item *bitem)
{
  int result;
  if ((bitem->pbi.mutype == endp_type_tap) ||
      (bitem->pbi.mutype == endp_type_phy) ||
      (bitem->pbi.mutype == endp_type_pci) ||
      (bitem->pbi.mutype == endp_type_wif) ||
      (bitem->pbi.mutype == endp_type_snf) ||
      (bitem->pbi.mutype == endp_type_nat) ||
      (bitem->pbi.mutype == endp_type_a2b))
    {
    result = 0;
    }
  else if (bitem->pbi.mutype == endp_type_c2c)
    {
    result = 1;
    }
  else
    KOUT("%d", bitem->pbi.mutype);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int is_a_a2b(t_bank_item *bitem)
{
  int result;
  if ((bitem->pbi.mutype == endp_type_tap) ||
      (bitem->pbi.mutype == endp_type_phy) ||
      (bitem->pbi.mutype == endp_type_pci) ||
      (bitem->pbi.mutype == endp_type_wif) ||
      (bitem->pbi.mutype == endp_type_snf) ||
      (bitem->pbi.mutype == endp_type_nat) ||
      (bitem->pbi.mutype == endp_type_c2c))
    {
    result = 0;
    }
  else if (bitem->pbi.mutype == endp_type_a2b)
    {
    result = 1;
    }
  else
    KOUT("%d", bitem->pbi.mutype);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_bank_item *get_currently_in_item_surface(void)
{
  return currently_in_item_surface;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void centralized_item_z_pos(t_bank_item *bitem)
{
  t_bank_item *cur;
  if (currently_in_item_surface != bitem)
    topo_cr_item_set_z(bitem);
  if (bitem->bank_type == bank_type_edge)
    {
    if ((!bitem->att_lan) || (!bitem->att_eth))
      KOUT(" ");
    topo_cr_item_set_z(bitem->att_lan);
    topo_cr_item_set_z(bitem->att_eth);
    }
  cur = currently_in_item_surface;
  if (cur)
    {
    if ((cur->bank_type <= bank_type_min) || 
        (cur->bank_type >= bank_type_max)) 
      KOUT(" ");
    topo_cr_item_set_z(cur);
    if (cur->bank_type == bank_type_edge)
      {
      if ((!cur->att_lan) || (!cur->att_eth))
        KOUT(" ");
      topo_cr_item_set_z(cur->att_lan);
      topo_cr_item_set_z(cur->att_eth);
      }
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void get_object_mass(t_bank_item *bitem)
{
  switch(bitem->bank_type)
    {
    case bank_type_node:
      bitem->pbi.mass = (double) 200;
    break;
    case bank_type_eth:
    case bank_type_lan:
      bitem->pbi.mass = (double) 10;
    break;
    case bank_type_sat:
      bitem->pbi.mass = (double) 80;
    break;
    default:
      bitem->pbi.mass = (double) 10;
    break;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_bank_item *centralized_item_creation(int bank_type, char *name,
                                              char *lan, int num, 
                                              t_bank_item *bnode, 
                                              t_bank_item *beth, 
                                              t_bank_item *blan, 
                                              double x0, double y0,
                                              int hidden_on_graph,
                                              int mutype,
                                              double x1, double y1,
                                              double x2, double y2,
                                              double dist, int eorig)
{
  t_bank_item *bitem;
  bank_add_item(bank_type, name, lan, num, beth, blan, eorig);
  bitem = bank_get_item(bank_type, name, num, lan); 
  bitem->pbi.line_width = 1;
  bitem->pbi.hidden_on_graph = hidden_on_graph;
  bitem->pbi.mutype = mutype;
  bitem->pbi.x0 = x0;
  bitem->pbi.y0 = y0;
  bitem->pbi.x1 = x1;
  bitem->pbi.y1 = y1;
  bitem->pbi.x2 = x2;
  bitem->pbi.y2 = y2;
  bitem->pbi.dist = dist;
  get_object_mass(bitem);
  if (bitem->bank_type == bank_type_node) 
    eventfull_obj_create(name);
  else if (bitem->bank_type == bank_type_sat)
    eventfull_obj_create(name);
  return (bitem);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void centralized_item_deletion(t_bank_item *bitem, int eorig)
{
  leave_item_action_surface(bitem);
  if (bitem->bank_type == bank_type_node) 
    eventfull_obj_delete(bitem->name);
  else if (bitem->bank_type == bank_type_sat)
    eventfull_obj_delete(bitem->name);
  topo_remove_cr_item(bitem);
  bank_del_item(bitem, eorig);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_list_bank_item *find_bitem(t_bank_item *bedge, t_bank_item *bitem) 
{
  t_list_bank_item *cur = bitem->head_edge_list;
  while(cur && (cur->bitem != bedge))
    cur = cur->next;
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int test_edge_type(t_bank_item *bitem)
{
  int result = -1;
  if (bitem->bank_type == bank_type_edge)
    result = 0;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int test_edge_endpoint(t_bank_item *bitem)
{
  int result = -1;
  if ((bitem->bank_type == bank_type_eth) ||
      (bitem->bank_type == bank_type_sat) ||
      (bitem->bank_type == bank_type_lan))
    result = 0;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void  attached_edge_associations_delete(t_bank_item *bitem, int eorig)
{
  t_list_bank_item *list_elem;
  if (test_edge_type(bitem))
    KOUT(" ");
  list_elem = find_bitem(bitem, bitem->att_eth); 
  if (list_elem->next)
    list_elem->next->prev = list_elem->prev;
  if (list_elem->prev)
    list_elem->prev->next = list_elem->next;
  if (list_elem == bitem->att_eth->head_edge_list)
    bitem->att_eth->head_edge_list = list_elem->next;
  clownix_free(list_elem, __FUNCTION__);
  list_elem = find_bitem(bitem, bitem->att_lan); 
  if (list_elem->next)
    list_elem->next->prev = list_elem->prev;
  if (list_elem->prev)
    list_elem->prev->next = list_elem->next;
  if (list_elem == bitem->att_lan->head_edge_list)
    bitem->att_lan->head_edge_list = list_elem->next;
  clownix_free(list_elem, __FUNCTION__);
  centralized_item_deletion(bitem, eorig);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
/*                            update_edge                                   */
/*--------------------------------------------------------------------------*/
static void update_edge(t_bank_item *bitem)
{
  t_bank_item *beth, *blan;
  if (test_edge_type(bitem))
    KOUT(" ");
  if (!bitem->pbi.grabbed)
    {
    beth = bitem->att_eth;
    blan = bitem->att_lan;
    attached_edge_associations_delete(bitem, eorig_update);
    add_new_edge(beth, blan, eorig_update);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void attached_edge_update_list(t_bank_item *bitem)
{
  t_list_bank_item *next, *cur = bitem->head_edge_list;
  if (test_edge_endpoint(bitem))
    KOUT("%d", bitem->bank_type);
  while (cur)
    {
    next = cur->next;
    update_edge(cur->bitem);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void attached_obj_associations_delete(t_bank_item *bitem)
{
  t_list_bank_item *next_eth, *eth = bitem->head_eth_list;
  t_list_bank_item *next, *cur = bitem->head_edge_list;
  selectioned_item_delete(bitem);
  while (eth)
    {
    next_eth = eth->next;
    attached_obj_associations_delete(eth->bitem);
    clownix_free(eth, __FUNCTION__);
    eth = next_eth;
    }
  while (cur)
    {
    next = cur->next;
    attached_edge_associations_delete(cur->bitem, eorig_modif);
    cur = next;
    }
  centralized_item_deletion(bitem, eorig_noedge);
}
/*--------------------------------------------------------------------------*/



/****************************************************************************/
static void attached_associations_delete(t_bank_item *bitem)
{
  switch (bitem->bank_type)
    {
    case bank_type_edge:
      attached_edge_associations_delete(bitem, eorig_modif);
      break;

    case bank_type_node:
    case bank_type_sat:
    case bank_type_eth:
    case bank_type_lan:
      attached_obj_associations_delete(bitem);
      break;

    default:
      KOUT(" ");
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void attached_edge_associations_create(t_bank_item *bitem)
{
  t_bank_item *beth, *blan;
  t_list_bank_item *new_item;
  if (test_edge_type(bitem))
    KOUT(" ");
  blan  = bitem->att_lan;
  beth = bitem->att_eth;
  new_item = clownix_malloc(sizeof(t_list_bank_item),20);
  memset(new_item, 0, sizeof(t_list_bank_item));
  new_item->bitem = bitem;
  new_item->next = beth->head_edge_list;
  if (new_item->next)
    new_item->next->prev = new_item;
  beth->head_edge_list = new_item;
  new_item = clownix_malloc(sizeof(t_list_bank_item),20);
  memset(new_item, 0, sizeof(t_list_bank_item));
  new_item->bitem = bitem;
  new_item->next = blan->head_edge_list;
  if (new_item->next)
    new_item->next->prev = new_item;
  blan->head_edge_list = new_item;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void local_attached_edge_update_all(t_bank_item *bitem)
{
  t_list_bank_item *cur, *next;
  if ((bitem->bank_type == bank_type_node))
    {
    cur = bitem->head_eth_list;
    while (cur)
      {
      attached_edge_update_list(cur->bitem);
      cur = cur->next;
      }
    }
  else 
  if ((bitem->bank_type == bank_type_sat)      ||
      (bitem->bank_type == bank_type_eth))
    {
    attached_edge_update_list(bitem);
    }
  else 
  if (bitem->bank_type == bank_type_lan) 
    {
    cur = bitem->head_edge_list;
    while (cur)
      {
      next = cur->next;
      attached_edge_update_list(cur->bitem->att_eth);
      cur = next;
      }
    }
  else
    KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timeout_attached_edge_update_all (void *data)
{
  t_edge_update_all *eua = (t_edge_update_all *) data;
  t_bank_item *bitem;
  bitem = bank_get_item(eua->bank_type, eua->name, eua->num, NULL); 
  clownix_free(data, __FUNCTION__);
  if (bitem)
    {
    bitem->abs_beat_eua_timeout = 0;
    bitem->ref_eua_timeout = 0;
    local_attached_edge_update_all(bitem);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void attached_edge_update_all(t_bank_item *bitem)
{
  t_edge_update_all *eua;
  if (bitem->abs_beat_eua_timeout == 0)
    {
    eua = (t_edge_update_all *) clownix_malloc(sizeof(t_edge_update_all), 5);
    eua->bank_type = bitem->bank_type;
    strncpy(eua->name, bitem->name, MAX_PATH_LEN); 
    eua->num = bitem->num;
    clownix_timeout_add(1, timeout_attached_edge_update_all, (void *) eua, 
                        &(bitem->abs_beat_eua_timeout), 
                        &(bitem->ref_eua_timeout)); 
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void write_item_name(t_bank_item *bitem)
{
  double dx = 0, dy;
  char name[MAX_NAME_LEN];
  if (bitem->bank_type == bank_type_sat) 
    {
    if (is_a_snf(bitem))
      {
      dy = -16;
      dx = -13;
      }
    else
      {
      dy = -7;
      dx = -10;
      }
    }
  else if (bitem->bank_type == bank_type_lan) 
    {
    dy = -5;
    dx = -10;
    }
  else
    {
    dy = -6;
    dx = -3;
    }
  switch(bitem->bank_type)
    {
    case bank_type_eth:
      if (!bitem->att_node)
        KOUT(" ");
      if (bitem->att_node->pbi.mutype == endp_type_a2b)
        {
        if (bitem->num == 0)
          sprintf(name, "a");
        else if (bitem->num == 1)
          sprintf(name, "b");
        else
          KERR("%s %d", bitem->att_node->name, bitem->num);
        }
      else
        sprintf(name, "%d", bitem->num);
      break;
    default:
      strcpy(name, bitem->name);
      break;
    }
  topo_cr_item_text(bitem, bitem->pbi.x0 + dx, 
                    bitem->pbi.y0 + dy, name);
  if ((bitem->bank_type == bank_type_sat) && (is_a_snf(bitem)))
    snf_add_capture_on(bitem);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void write_node_name(t_bank_item *bitem)
{
  double x0,y0;
  double dx, dy;
  x0 = bitem->pbi.x0;
  y0 = bitem->pbi.y0;
  dy = -10;
  dx = -NODE_DIA/2 + 12;
  topo_cr_item_text(bitem, x0 + dx, y0 + dy, bitem->name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void delete_bitem(t_bank_item *bitem)
{
  switch (bitem->bank_type)
    {
    case bank_type_node:
    case bank_type_sat:
    case bank_type_lan:
    case bank_type_eth:
      hidden_visible_del_bitem(bitem);
      break;
    case bank_type_edge:
      break;
    default:
      KOUT("%d",bitem->bank_type);
      break;
    }
  attached_associations_delete(bitem);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void add_new_edge(t_bank_item *bi_eth, t_bank_item *bi_lan, int eorig)
{
  int bank_type;
  t_bank_item *bi_edge;
  t_bank_item *bitem;
  double x0, y0, x1, y1, dist;
  int mutype = mutype_none;
  if ((!bi_lan) || (!bi_eth) || 
      (bi_lan->bank_type != bank_type_lan))
    KOUT(" ");
  if ((bi_eth->pbi.hidden_on_graph) || 
      (bi_lan->pbi.hidden_on_graph))
    {
    hidden_visible_save_edge(bi_eth, bi_lan);
    return;
    }
  x0 = bi_eth->pbi.x0;
  y0 = bi_eth->pbi.y0;
  x1 = bi_lan->pbi.x0;
  y1 = bi_lan->pbi.y0;

  if (bi_eth->bank_type == bank_type_eth)
    {
    if (!bi_eth->att_node)
      KOUT(" ");
    topo_get_matrix_transform_point(bi_eth->att_node, &x0, &y0);
    topo_get_matrix_transform_point(bi_eth, &x0, &y0);
    bitem = bi_eth->att_node;
    if (bitem && bitem->pbi.pbi_node)
      {
      }
    }
  else if (bi_eth->bank_type == bank_type_sat) 
    {
    topo_get_matrix_transform_point(bi_eth, &x0, &y0);
    }
  else
    KOUT("%d", bi_eth->bank_type);

  topo_get_matrix_transform_point(bi_lan, &x1, &y1);

  dist = sqrt((x1-x0)*(x1-x0) + (y1-y0)*(y1-y0));
  if (dist < INTF_DIA)
    dist = INTF_DIA;
  if (bi_lan->bank_type == bank_type_lan)
    { 
    if ((bi_eth->bank_type == bank_type_eth) || 
        (bi_eth->bank_type == bank_type_c2c) ||
        (bi_eth->bank_type == bank_type_snf) ||
        (bi_eth->bank_type == bank_type_sat))
      bank_type = bank_type_edge;
    else
      KOUT("%d", bi_eth->bank_type);
    bi_lan->pbi.mutype = mutype;
    }
  else
    KOUT("%d", bi_eth->bank_type);
  bi_edge = centralized_item_creation(bank_type, bi_eth->name,
                                      bi_lan->name, bi_eth->num,
                                      NULL, bi_eth, bi_lan,
                                      x0, y0, 0, 0, x1, y1, 0, 0, dist, 
                                      eorig);

  if (bi_edge->att_eth != bi_eth)
    KOUT(" ");
  if (bi_edge->att_lan != bi_lan)
    KOUT(" ");
  topo_add_cr_item_to_canvas(bi_edge, NULL);
  topo_get_absolute_coords(bi_edge);
  centralized_item_z_pos(bi_edge);
  attached_edge_associations_create(bi_edge);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
int add_new_lan(char *name, double x, double y, int hidden_on_graph)
{
  int result = 0;
  t_bank_item *bitem;
  if (bank_get_item(bank_type_lan, name, 0, NULL))
    result = -1;
  else
    {
    bitem = centralized_item_creation(bank_type_lan, name, NULL, 0, 
                                      NULL, NULL, NULL, 
                                      x, y, hidden_on_graph, 0,
                                      0, 0, 0, 0, 0,
                                      eorig_noedge);
    topo_add_cr_item_to_canvas(bitem, NULL);
    write_item_name(bitem);
    topo_get_absolute_coords(bitem);
    centralized_item_z_pos(bitem);
    if (bitem->pbi.hidden_on_graph)
      topo_bitem_hide(bitem);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int add_new_sat(char *name, int mutype, t_topo_c2c *c2c,
                double x, double y, int hidden_on_graph)
{
  int result = 0;
  t_bank_item *bitem;
  int bank_type;
  bank_type = bank_type_sat;
  bitem = bank_get_item(bank_type, name, 0, NULL);
  if (bitem)
    {
    if (is_a_snf(bitem))
      snf_add_capture_on(bitem);
    insert_next_warning("Address already exists", 1);
    result = -1;
    }
  if (!result)
    {
    bitem = centralized_item_creation(bank_type, name, NULL, 0,
                                      NULL, NULL, NULL,
                                      x, y, hidden_on_graph, mutype,
                                      0, 0, 0, 0, 0,
                                      eorig_noedge);

    bitem->pbi.pbi_sat =
    (t_pbi_sat *) clownix_malloc(sizeof(t_pbi_sat), 14);
    memset(bitem->pbi.pbi_sat, 0, sizeof(t_pbi_sat));
    if (c2c)
      memcpy(&(bitem->pbi.pbi_sat->topo_c2c), c2c, sizeof(t_topo_c2c));
    topo_add_cr_item_to_canvas(bitem, NULL);
    write_item_name(bitem);
    topo_get_absolute_coords(bitem);
    centralized_item_z_pos(bitem);
    if (bitem->pbi.hidden_on_graph)
      topo_bitem_hide(bitem);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void modify_c2c(char *name, char *master_cloonix, char *slave_cloonix)
{
  t_bank_item *bitem = look_for_sat_with_id(name);
  if ((bitem) && is_a_c2c(bitem))
    {
    strncpy(bitem->pbi.pbi_sat->topo_c2c.master_cloonix, 
            master_cloonix, MAX_NAME_LEN);
    strncpy(bitem->pbi.pbi_sat->topo_c2c.slave_cloonix, 
            slave_cloonix, MAX_NAME_LEN);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void modify_snf(char *name, int evt)
{
  t_bank_item *bitem = look_for_sat_with_id(name);
  if ((bitem) && (is_a_snf(bitem)))
    {
    if (evt == snf_evt_capture_on)
      bitem->pbi.pbi_sat->snf_capture_on = 1;
    else if (evt == snf_evt_capture_off)
      bitem->pbi.pbi_sat->snf_capture_on = 0;
    else
      KOUT("%d ", evt);
    snf_add_capture_on(bitem);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int add_new_eth(char *name, int num, int mutype, 
                 double x, double y, int hidden_on_graph)
{
  int result = 0;
  t_bank_item *bnode, *bitem;
  t_list_bank_item *lst_eth;
  int bank_type = bank_type_eth;
  double x0, y0;
  
  bnode = look_for_node_with_id(name);
  if (!bnode)
     {
     bnode = look_for_sat_with_id(name);
     if (bnode->pbi.mutype != endp_type_a2b)
       KOUT("%s", name);
     }
  if ((num <0) || (num > MAX_SOCK_VM+MAX_WLAN_VM))
    KOUT(" ");
  if ((!bnode) || (bank_get_item(bank_type, name, num, NULL)))
    result = -1;
  else
    {
    x0 = bnode->pbi.x0;
    y0 = bnode->pbi.y0;
    bitem = centralized_item_creation(bank_type, name, NULL, num, 
                                      bnode, NULL, NULL,
                                      x0, y0, hidden_on_graph, 0, 
                                      x, y, 0, 0,  0,
                                      eorig_noedge);
    bitem->pbi.mutype = mutype;
    bitem->att_node = bnode;
    lst_eth=(t_list_bank_item *)clownix_malloc(sizeof(t_list_bank_item), 12);
    memset(lst_eth, 0, sizeof(t_list_bank_item));
    lst_eth->bitem = bitem;
    lst_eth->next = bnode->head_eth_list;
    if (bnode->head_eth_list)
      bnode->head_eth_list->prev = lst_eth;
    bnode->head_eth_list = lst_eth;
    topo_add_cr_item_to_canvas(bitem, bnode);
    topo_get_absolute_coords(bitem);
    modif_position_eth(bitem, x0+x, y0+y);
    bnode->num++;
    write_item_name(bitem);
    if (bitem->pbi.hidden_on_graph)
      topo_bitem_hide(bitem);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int add_new_node(char *name, char *kernel, char *rootfs_used,
                 char *rootfs_backing,  char *install_cdrom,     
                 char *added_cdrom, char *added_disk,
                 double x, double y, int hidden_on_graph,
                 int color_choice, int vm_id, int vm_config_flags,
                 int nb_tot_eth, t_eth_table *eth_tab)
{
  int result = 0;
  t_bank_item *bitem;
  if (bank_get_item(bank_type_node, name,  0, NULL)) 
    result = -1;
  else
    {
    bitem = centralized_item_creation(bank_type_node, name, NULL, 0, 
                                      NULL, NULL, NULL, 
                                      x, y, hidden_on_graph, 0,
                                      0, 0, 0, 0, 0,
                                      eorig_noedge);
    bitem->pbi.pbi_node = (t_pbi_node *) clownix_malloc(sizeof(t_pbi_node), 14);
    memset(bitem->pbi.pbi_node, 0, sizeof(t_pbi_node));
    strncpy(bitem->pbi.pbi_node->node_kernel, kernel, MAX_NAME_LEN-1);
    strncpy(bitem->pbi.pbi_node->node_rootfs_sod, rootfs_used, MAX_PATH_LEN-1);
    strncpy(bitem->pbi.pbi_node->node_rootfs_backing_file,
                                 rootfs_backing, MAX_PATH_LEN-1);
    strncpy(bitem->pbi.pbi_node->install_cdrom, install_cdrom, MAX_PATH_LEN-1); 
    strncpy(bitem->pbi.pbi_node->added_cdrom, added_cdrom, MAX_PATH_LEN-1); 
    strncpy(bitem->pbi.pbi_node->added_disk, added_disk, MAX_PATH_LEN-1); 
    bitem->pbi.color_choice = color_choice;
    bitem->pbi.pbi_node->node_vm_id = vm_id;
    bitem->pbi.pbi_node->nb_tot_eth = nb_tot_eth;
    memcpy(bitem->pbi.pbi_node->eth_tab, eth_tab,
           nb_tot_eth * sizeof(t_eth_table));
    bitem->pbi.pbi_node->node_vm_config_flags = vm_config_flags;
    bitem->pbi.flag = flag_qmp_conn_ko;
    topo_add_cr_item_to_canvas(bitem, NULL);
    write_node_name(bitem);
    topo_get_absolute_coords(bitem);
    centralized_item_z_pos(bitem);
    if (bitem->pbi.hidden_on_graph)
      topo_bitem_hide(bitem);
    }
  return result;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void enter_item_surface(t_bank_item *bitem)
{
  if (!currently_in_item_surface)
    {
    currently_in_item_surface = bitem;
    centralized_item_z_pos(bitem);
    bitem->pbi.grabbed = 1;
    }
  else if (bitem != currently_in_item_surface)
    {
    leave_item_surface();
    currently_in_item_surface = bitem;
    centralized_item_z_pos(bitem);
    bitem->pbi.grabbed = 1;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void leave_item_action_surface(t_bank_item *bitem)
{
  if (bitem == currently_in_item_surface)
    leave_item_surface();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void leave_item_surface(void)
{
  if (currently_in_item_surface)
    {
    if (currently_in_item_surface->pbi.menu_on == 0)
      currently_in_item_surface->pbi.grabbed = 0;
    centralized_item_z_pos(currently_in_item_surface);
    currently_in_item_surface = NULL;
    }
}
/*--------------------------------------------------------------------------*/


