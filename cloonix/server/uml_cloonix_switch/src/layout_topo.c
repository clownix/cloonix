/*****************************************************************************/
/*    Copyright (C) 2006-2022 clownix@clownix.net License AGPL-3             */
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
#include "commun_daemon.h"
#include "rpc_clownix.h"
#include "layout_rpc.h"
#include "cfg_store.h"
#include "event_subscriber.h"
#include "lan_to_name.h"
#include "llid_trace.h"
#include "cnt.h"
#include "ovs_nat.h"
#include "ovs_c2c.h"
#include "ovs_tap.h"
#include "ovs_a2b.h"

#define MAX_MS_OF_INSERT 3600000



/*****************************************************************************/
typedef struct t_layout_sub
{
  int llid;
  int tid;
  int resp_llid;
  int resp_tid;
  struct t_layout_sub *prev;
  struct t_layout_sub *next;
} t_layout_sub;
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_layout_xml g_layout_xml;
/*---------------------------------------------------------------------------*/
static t_layout_sub *g_head_layout_sub;
/*---------------------------------------------------------------------------*/
static int g_node_x_coord[MAX_POLAR_COORD];
static int g_node_y_coord[MAX_POLAR_COORD];
static int g_a2b_x_coord[MAX_POLAR_COORD];
static int g_a2b_y_coord[MAX_POLAR_COORD];

static int g_authorized_to_move;
static int g_forbiden_to_move_by_cli;
static int g_width, g_height, g_cx, g_cy, g_cw, g_ch;



/****************************************************************************/
t_layout_xml *get_layout_xml_chain(void)
{
  return (&g_layout_xml);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void get_layout_main_params(int *go, int *width, int *height, 
                            int *cx, int *cy, int *cw, int *ch)
{
  *go = g_authorized_to_move;
  *width = g_width;
  *height = g_height;
  *cx = g_cx;
  *cy = g_cy;
  *cw = g_cw;
  *ch = g_ch;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int authorized_to_modify_data_bank_layout(int llid, int tid,
                                                 int is_on_off)
{
  int result = 1;
  int beat_count = llid_get_count_beat(llid);
  if (g_head_layout_sub)
    {
    if (tid == 8888)
      {
      if ((!is_on_off) && (!g_authorized_to_move) &&
          (g_forbiden_to_move_by_cli))
        {
        result = 0;
        }
      else if (beat_count < 3)
        {
        result = 0;
        }
      else
        {
        if (llid && (g_head_layout_sub->llid != llid))
          result = 0;
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void layout_kill_all_graphs(void)
{
  t_layout_sub *next, *cur = g_head_layout_sub;
  while(cur)
    {
    next = cur->next;
    llid_trace_free(cur->llid, 0, __FUNCTION__);
    cur = next;
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void make_default_layout_node(t_layout_node *layout, 
                              char *name, int nb_eth)
{
  int i, rest;
  memset(layout, 0, sizeof(t_layout_node));
  strncpy(layout->name, name, MAX_NAME_LEN-1);
  layout->nb_eth = nb_eth;
  layout->x = 100;
  layout->y = 100;
  for (i=0; i<nb_eth; i++)
    {
    rest = i%4;
    if (rest == 0)
      layout->eth[i].y = NODE_DIA * VAL_INTF_POS_NODE;
    if (rest == 1)
      layout->eth[i].x = NODE_DIA * VAL_INTF_POS_NODE;
    if (rest == 2)
      layout->eth[i].y = -NODE_DIA * VAL_INTF_POS_NODE;
    if (rest == 3)
      layout->eth[i].x = -NODE_DIA * VAL_INTF_POS_NODE;
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void make_default_layout_lan(t_layout_lan *layout, char *name)
{
  memset(layout, 0, sizeof(t_layout_lan));
  strncpy(layout->name, name, MAX_NAME_LEN-1);
  layout->x = 20;
  layout->y = 20;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void insert_lan_xml(t_layout_lan_xml *xml)
{
  if (g_layout_xml.lan_xml)
    g_layout_xml.lan_xml->prev = xml;
  xml->next = g_layout_xml.lan_xml;
  g_layout_xml.lan_xml = xml;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void extract_lan_xml(t_layout_lan_xml *xml)
{
  if (xml->prev)
    xml->prev->next = xml->next; 
  if (xml->next)
    xml->next->prev = xml->prev;
  if (xml == g_layout_xml.lan_xml)
    g_layout_xml.lan_xml = xml->next;
  clownix_free(xml, __FILE__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void insert_sat_xml(t_layout_sat_xml *xml)
{
  if (g_layout_xml.sat_xml)
    g_layout_xml.sat_xml->prev = xml;
  xml->next = g_layout_xml.sat_xml;
  g_layout_xml.sat_xml = xml;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void extract_sat_xml(t_layout_sat_xml *xml)
{
  if (xml->prev)
    xml->prev->next = xml->next; 
  if (xml->next)
    xml->next->prev = xml->prev;
  if (xml == g_layout_xml.sat_xml)
    g_layout_xml.sat_xml = xml->next;
  clownix_free(xml, __FILE__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void insert_node_xml(t_layout_node_xml *xml)
{
  if (g_layout_xml.node_xml)
    g_layout_xml.node_xml->prev = xml;
  xml->next = g_layout_xml.node_xml;
  g_layout_xml.node_xml = xml;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void extract_node_xml(t_layout_node_xml *xml)
{
  if (xml->prev)
    xml->prev->next = xml->next; 
  if (xml->next)
    xml->next->prev = xml->prev;
  if (xml == g_layout_xml.node_xml)
    g_layout_xml.node_xml = xml->next;
  clownix_free(xml, __FILE__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_layout_lan_xml *find_lan_xml(char *name)
{
  t_layout_lan_xml *cur = g_layout_xml.lan_xml;
  while(cur)
    {
    if (!strcmp(cur->lan.name, name))
      break;
    cur = cur->next;
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_layout_sat_xml *find_sat_xml(char *name)
{
  t_layout_sat_xml *cur = g_layout_xml.sat_xml;
  while(cur)
    {
    if (!strcmp(cur->sat.name, name))
      break;
    cur = cur->next;
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_layout_node_xml *find_node_xml(char *name)
{
  t_layout_node_xml *cur = g_layout_xml.node_xml;
  while(cur)
    {
    if (!strcmp(cur->node.name, name))
      break;
    cur = cur->next;
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void add_layout_lan(t_layout_lan *lan)
{
  t_layout_lan_xml *cur, *xml;
  cur = find_lan_xml(lan->name);
  if (cur)
    KERR("%s", lan->name);
  else
    {
    xml = (t_layout_lan_xml *) clownix_malloc(sizeof(t_layout_lan_xml), 8);
    memset(xml, 0, sizeof(t_layout_lan_xml));
    memcpy(&(xml->lan), lan, sizeof(t_layout_lan));
    insert_lan_xml(xml);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void add_layout_sat(t_layout_sat *sat)
{
  t_layout_sat_xml *cur, *xml;
  cur = find_sat_xml(sat->name);
  if (cur)
    KERR("%s", sat->name);
  else
    {
    xml = (t_layout_sat_xml *) clownix_malloc(sizeof(t_layout_sat_xml), 10);
    memset(xml, 0, sizeof(t_layout_sat_xml));
    memcpy(&(xml->sat), sat, sizeof(t_layout_sat));
    insert_sat_xml(xml);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void add_layout_node(t_layout_node *node)
{
  t_layout_node_xml *cur, *xml;
  cur = find_node_xml(node->name);
  if (cur)
    KERR("%s", node->name);
  else
    {
    xml = (t_layout_node_xml *) clownix_malloc(sizeof(t_layout_node_xml), 7);
    memset(xml, 0, sizeof(t_layout_node_xml));
    memcpy(&(xml->node), node, sizeof(t_layout_node));
    insert_node_xml(xml);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void update_layout_lan(char *name, double x, double y, int hidden)
{
  t_layout_lan layout;
  t_layout_lan_xml *cur = find_lan_xml(name);
  if (cur)
    {
    cur->lan.x = x;
    cur->lan.y = y;
    cur->lan.hidden_on_graph = hidden;
    }
  else
    {
    make_default_layout_lan(&layout, name);
    add_layout_lan(&layout);
    cur = find_lan_xml(name);
    if (!cur)
      KERR("ERROR %s", name);
    else
      {
      cur->lan.x = x;
      cur->lan.y = y;
      cur->lan.hidden_on_graph = hidden;
      }
    } 
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void update_layout_sat(char *name,
                              double x, double y, 
                              double xa, double ya, 
                              double xb, double yb, 
                              int hidden)
{
  t_layout_sat_xml *cur = find_sat_xml(name);
  if (cur)
    {
    cur->sat.x = x;
    cur->sat.y = y;
    cur->sat.xa = xa;
    cur->sat.ya = ya;
    cur->sat.xb = xb;
    cur->sat.yb = yb;
    cur->sat.hidden_on_graph = hidden;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void update_layout_node(char *name, double x, double y, int hidden,
                               int nb_eth, t_layout_eth *eth)
{
  int i;
  t_layout_node_xml *cur = find_node_xml(name);
  if (!cur)
    KERR("%s", name);
  else
    { 
    cur->node.x = x;
    cur->node.y = y;
    cur->node.hidden_on_graph = hidden;
    for (i=0; i<nb_eth; i++)
      memcpy(&(cur->node.eth[i]), &(eth[i]),
              sizeof(t_layout_eth));
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void send_node(int llid, int tid, t_layout_node_xml *node_xml)
{
  t_layout_node_xml *cur = node_xml;
  while (cur)
    {
    send_layout_node(llid, tid, &(cur->node));
    cur = cur->next;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void send_sat(int llid, int tid, t_layout_sat_xml *sat_xml)
{
  t_layout_sat_xml *cur = sat_xml;
  while (cur)
    {
    send_layout_sat(llid, tid, &(cur->sat));
    cur = cur->next;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void send_lan(int llid, int tid, t_layout_lan_xml *lan_xml)
{
  t_layout_lan_xml *cur = lan_xml;
  while (cur)
    {
    send_layout_lan(llid, tid, &(cur->lan));
    cur = cur->next;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_layout_center_scale(void *data)
{
  int llid = (int) ((unsigned long)data);
  if ((g_cw>50) && (g_ch>50))
    {
    send_layout_center_scale(llid, 0, g_cx, g_cy, g_cw, g_ch);
    }
  send_node(llid, 0, g_layout_xml.node_xml);
  send_sat (llid, 0, g_layout_xml.sat_xml);
  send_lan (llid, 0, g_layout_xml.lan_xml);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void refresh_single_graphs(int llid, int tid)
{
  unsigned long ul_llid = (unsigned long) llid;
  send_layout_move_on_off(llid, tid, g_authorized_to_move);
  if ((g_width > 50) && (g_height>50))
    {
    send_layout_width_height(llid, tid, g_width, g_height);
    }
  clownix_timeout_add(10, timer_layout_center_scale, (void *) ul_llid, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void alloc_layout_sub(int llid, int tid)
{
  t_layout_sub *last, *sub;
  sub = (t_layout_sub *) clownix_malloc(sizeof(t_layout_sub), 7);
  memset(sub, 0, sizeof(t_layout_sub));
  sub->llid = llid;
  sub->tid = tid;
  if (g_head_layout_sub)
    {
    last = g_head_layout_sub;
    while (last->next)
      last = last->next;
    last->next = sub;
    sub->prev = last;
    }
  else
    g_head_layout_sub = sub;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void free_layout_sub(t_layout_sub *sub)
{
  if (sub->next)
    sub->next->prev = sub->prev;
  if (sub->prev)
    sub->prev->next = sub->next;
  if (sub == g_head_layout_sub)
    g_head_layout_sub = sub->next;
  clownix_free(sub, __FUNCTION__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_layout_sub *get_layout_sub(int llid)
{
  t_layout_sub *cur = g_head_layout_sub;
  while(cur)
    {
    if (cur->llid == llid)
      break;
    cur = cur->next;
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_lan(int llid, int tid, t_layout_lan *layout)
{
  t_layout_sub *cur = g_head_layout_sub;
  t_layout_lan_xml *xml;
  if (authorized_to_modify_data_bank_layout(llid, tid, 0))
    {
    while(cur)
      {
      if (llid != cur->llid)
        send_layout_lan(cur->llid, cur->tid, layout);
      cur = cur->next;
      }
    update_layout_lan(layout->name,layout->x,layout->y,layout->hidden_on_graph);
    }
  else
    {
    xml = find_lan_xml(layout->name);
    if (!xml)
      KERR("ERROR %s", layout->name);
    else
      send_layout_lan(llid, tid, &(xml->lan));
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_sat(int llid, int tid, t_layout_sat *layout)
{
  t_layout_sub *cur = g_head_layout_sub;
  t_layout_sat_xml *xml;
  if ((!ovs_nat_exists(layout->name)) &&
      (!ovs_c2c_exists(layout->name)) &&
      (!ovs_a2b_exists(layout->name)) &&
      (!ovs_tap_exists(layout->name)))
    KERR("%s", layout->name);
  else
    {
    if (authorized_to_modify_data_bank_layout(llid, tid, 0))
      {
      while(cur)
        {
        if (llid != cur->llid)
          send_layout_sat(cur->llid, cur->tid, layout);
        cur = cur->next;
        }
      update_layout_sat(layout->name,
                        layout->x, layout->y,
                        layout->xa, layout->ya,
                        layout->xb, layout->yb,
                        layout->hidden_on_graph);
      }
    else
      {
      xml = find_sat_xml(layout->name);
      send_layout_sat(llid, tid, &(xml->sat));
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_node(int llid, int tid, t_layout_node *layout)
{
  t_layout_sub *cur = g_head_layout_sub;
  t_layout_node_xml *xml = find_node_xml(layout->name);
  if (xml) 
    {
    if (authorized_to_modify_data_bank_layout(llid, tid, 0))
      {
      while(cur)
        {
        if (llid != cur->llid)
          send_layout_node(cur->llid, cur->tid, layout);
        cur = cur->next;
        }
      update_layout_node(layout->name, layout->x, layout->y,
                         layout->hidden_on_graph, layout->nb_eth,
                         layout->eth);
      }
    else
      {
      send_layout_node(llid, tid, &(xml->node));
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_event_sub(int llid, int tid, int on)
{
  t_layout_sub *sub = get_layout_sub(llid);
  if (on == 666)
    layout_kill_all_graphs();
  else if (on)
    {
    if (sub == NULL)
      alloc_layout_sub(llid, tid);
    refresh_single_graphs(llid, tid);
    }
  else
    {
    if (sub)
      free_layout_sub(sub);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void layout_modif_node(int llid, int tid, 
                              char *name, int type, int val1, int val2)
{
  int i, nb_eth;
  t_vm *vm;
  char info[MAX_PRINT_LEN];
  t_layout_node layout;
  double real_val1, real_val2;
  t_layout_node_xml *cur;
  cur = find_node_xml(name);
  vm = cfg_get_vm(name);
  if ((vm) || (cnt_name_exists(name, &nb_eth)))
    {
    if (!cur)
      KERR("%s", name);
    else
      {
      memset(&layout, 0, sizeof(t_layout_node));
      memcpy(&layout, &(cur->node), sizeof(t_layout_node));
      }
    switch (type)
      {
      case 0:
        layout.hidden_on_graph = val1;
        for (i=0;i<layout.nb_eth; i++)
          layout.eth[i].hidden_on_graph = val1;
        break;
      case 1:
        real_val1 = (double) val1;
        real_val2 = (double) val2;
        layout.x += real_val1;
        layout.y += real_val2;
        break;
      case 2:
        real_val1 = (double) val1;
        real_val2 = (double) val2;
        layout.x = real_val1;
        layout.y = real_val2;
        break;
      default:
        KOUT("%d", type);
      }
    recv_layout_node(0, 0, &layout);
    sprintf(info, "OK %s %d", name, val1);
    send_status_ok(llid, tid, info);
    }
  else
    {
    sprintf(info, "KO %s not found", name);
    send_status_ko(llid, tid, info);
    KERR("%s", info);
    }
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void layout_modif_eth(int llid, int tid, char *name, int num,
                              int type, int val1, int val2)
{
  char info[MAX_PRINT_LEN];
  double real_val1, real_val2;
  t_vm *vm;
  t_layout_node layout;
  t_layout_node_xml *cur;
  cur = find_node_xml(name);
  vm = cfg_get_vm(name);
  if (vm)
    {
    if (!cur)
      KERR("%s", name);
    else
      {
      memset(&layout, 0, sizeof(t_layout_node));
      memcpy(&layout, &(cur->node), sizeof(t_layout_node));
      }
    switch (type)
      {
      case 0:
        layout.eth[num].hidden_on_graph = val1;
        recv_layout_node(0, 0, &layout);
        sprintf(info, "OK %s %d", name, val1);
        send_status_ok(llid, tid, info);
        break;
      case 1:
      case 2:
        if ((val1<0) || (val1>=314))
          {
          sprintf(info, "KO: 0 <= %d < 314 is false", val1);
          send_status_ko(llid, tid, info);
          }
        else
          {
          real_val1 = (double) g_node_x_coord[val1];
          real_val2 = (double) g_node_y_coord[val1];
          layout.eth[num].x = real_val1;
          layout.eth[num].y = real_val2;
          recv_layout_node(0, 0, &layout);
          sprintf(info, "OK %s %d", name, val1);
          send_status_ok(llid, tid, info);
          }
        break;
      default:
        KOUT("%d", type);
      }
    }
  else
    {
    sprintf(info, "KO");
    send_status_ko(llid, tid, info);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void layout_modif_lan(int llid, int tid, 
                             char *name, int type, int val1, int val2)
{
  char info[MAX_PRINT_LEN];
  t_layout_lan layout;
  double real_val1, real_val2;
  t_layout_lan_xml *cur;
  int lan_num = lan_get_with_name(name);
  cur = find_lan_xml(name);
  if (lan_num)
    {
    if (!cur)
      KERR("%s", name);
    else
      {
      memset(&layout, 0, sizeof(t_layout_lan));
      memcpy(&layout, &(cur->lan), sizeof(t_layout_lan));
      }
    switch (type)
      {
      case 0:
        layout.hidden_on_graph = val1;
        break;
      case 1:
        real_val1 = (double) val1;
        real_val2 = (double) val2;
        layout.x += real_val1;
        layout.y += real_val2;
        break;
      case 2:
        real_val1 = (double) val1;
        real_val2 = (double) val2;
        layout.x = real_val1;
        layout.y = real_val2;
        break;
      default:
        KOUT("%d", type);
      }
    recv_layout_lan(0, 0, &layout);
    sprintf(info, "OK %s %d", name, val1);
    send_status_ok(llid, tid, info);
    }
  else
    {
    sprintf(info, "KO %s not found", name);
    send_status_ko(llid, tid, info);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void layout_modif_sat(int llid, int tid, char *name, int kind,
                             int val1, int val2)
{
  char info[MAX_PRINT_LEN];
  t_layout_sat layout;
  double real_val1, real_val2;
  t_layout_sat_xml *cur;
  cur = find_sat_xml(name);
  if (!cur)
    KERR("%s", name);
  else
    {
    memset(&layout, 0, sizeof(t_layout_sat));
    memcpy(&layout, &(cur->sat), sizeof(t_layout_sat));
    switch (kind)
      {
      case 0:
        layout.hidden_on_graph = val1;
        break;
      case 1:
        real_val1 = (double) val1;
        real_val2 = (double) val2;
        layout.x += real_val1;
        layout.y += real_val2;
        break;
      case 2:
        real_val1 = (double) val1;
        real_val2 = (double) val2;
        layout.x = real_val1;
        layout.y = real_val2;
        break;
      default:
        KOUT("%d", kind);
      }
    recv_layout_sat(0, 0, &layout);
    sprintf(info, "OK %s %d", name, val1);
    send_status_ok(llid, tid, info);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_modif(int llid, int tid, int modif_type, char *name, 
                       int num, int val1, int val2)
{
  if (authorized_to_modify_data_bank_layout(llid, tid, 0))
    {
    switch (modif_type)
      {
      case layout_modif_type_lan_hide:
        layout_modif_lan(llid, tid, name, 0, val1, val2);
        break;
      case layout_modif_type_sat_hide:
        layout_modif_sat(llid, tid, name, 0, val1, val2);
        break;
      case layout_modif_type_eth_hide:
        layout_modif_eth(llid, tid, name, num, 0,  val1, val2);
        break;
      case layout_modif_type_vm_hide:
        layout_modif_node(llid, tid, name, 0, val1, val2);
        break;
  
      case layout_modif_type_lan_xy:
        layout_modif_lan(llid, tid, name, 1, val1, val2);
        break;
      case layout_modif_type_sat_xy:
        layout_modif_sat(llid, tid, name, 1, val1, val2);
        break;
      case layout_modif_type_eth_xy:
        layout_modif_eth(llid, tid, name, num, 1,  val1, val2);
        break;
      case layout_modif_type_vm_xy:
        layout_modif_node(llid, tid, name, 1, val1, val2);
        break;
  
      case layout_modif_type_lan_abs_xy:
        layout_modif_lan(llid, tid, name, 2, val1, val2);
        break;
      case layout_modif_type_sat_abs_xy:
        layout_modif_sat(llid, tid, name, 2, val1, val2);
        break;
      case layout_modif_type_eth_abs_xy:
        layout_modif_eth(llid, tid, name, num, 2,  val1, val2);
        break;
      case layout_modif_type_vm_abs_xy:
        layout_modif_node(llid, tid, name, 2, val1, val2);
        break;
      default:
        KOUT(" ");
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_forbiden_to_move_by_cli(void *data)
{
  g_forbiden_to_move_by_cli = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_move_on_off(int llid, int tid, int on)
{
  t_layout_sub *cur = g_head_layout_sub;
  if (authorized_to_modify_data_bank_layout(llid, tid, 1))
    {
    if (on)
      {
      g_authorized_to_move = 1;
      send_status_ok(llid, tid, "automatic_move");
      }
    else
      {
      g_authorized_to_move = 0;
      clownix_timeout_add(500, timer_forbiden_to_move_by_cli, NULL, NULL, NULL);
      g_forbiden_to_move_by_cli = 1;
      send_status_ok(llid, tid, "manual_move");
      }
    while(cur)
      {
      if (llid != cur->llid)
        send_layout_move_on_off(cur->llid, cur->tid, on);
      cur = cur->next;
      }
    }
  else
    {
    send_layout_move_on_off(llid, tid, g_authorized_to_move);
    send_status_ko(llid, tid, "move");
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_width_height(int llid, int tid, int width, int height)
{
  t_layout_sub *cur = g_head_layout_sub;
  if (authorized_to_modify_data_bank_layout(llid, tid, 0))
    {
    send_status_ok(llid, tid, "width_height");
    g_width = width;
    g_height = height;
    if ((cur) && (g_width > 50) && (g_height>50))
      {
      while(cur)
        {
        if (llid != cur->llid)
          {
          send_layout_width_height(cur->llid, cur->tid, width, height);
          }
        cur = cur->next;
        }
      }
    }
  else
    {
    if ((cur) && (g_width > 50) && (g_height>50))
      send_layout_width_height(llid, tid, g_width, g_height);
    send_status_ko(llid, tid, "width_height");
    }
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void recv_layout_center_scale(int llid, int tid, int x, int y, int w, int h)
{
  t_layout_sub *cur = g_head_layout_sub;
  if (authorized_to_modify_data_bank_layout(llid, tid, 0))
    {
    send_status_ok(llid, tid, "center_scale");
    g_cx = x;
    g_cy = y;
    g_cw = w;
    g_ch = h;
    if ((cur) && (g_cw>50) && (g_ch>50))
      {
      while(cur)
        {
        if (llid != cur->llid)
          send_layout_center_scale(cur->llid, cur->tid, x, y, w, h);
        cur = cur->next;
        }
      }
    }
  else
    {
    if ((g_cw>50) && (g_ch>50))
      send_layout_center_scale(llid, tid, g_cx, g_cy, g_cw, g_ch);
    send_status_ko(llid, tid, "center_scale");
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_zoom(int llid, int tid, int zoom)
{
  t_layout_sub *cur = g_head_layout_sub;
  if (authorized_to_modify_data_bank_layout(llid, tid, 0))
    {
    send_status_ok(llid, tid, "zoom");
    while(cur)
      {
      if (llid != cur->llid)
        send_layout_zoom(cur->llid, cur->tid, zoom);
      cur = cur->next;
      }
    }
  else
    {
    send_status_ko(llid, tid, "zoom");
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_save_params_req(int llid, int tid)
{
  t_layout_sub *cur = g_head_layout_sub;
  if (!cur)
    send_status_ko(llid, tid, "params");
  else 
    {
    send_status_ok(llid, tid, "params");
    cur->resp_llid = llid;
    cur->resp_tid = tid;
    send_layout_save_params_req(cur->llid, cur->tid);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_save_params_resp(int llid, int tid, int on,
                                  int width, int height,
                                  int cx, int cy, int cw, int ch)
{
  t_layout_sub *cur = g_head_layout_sub;
  if (cur)
    {
    if (msg_exist_channel(cur->resp_llid))
      send_layout_save_params_resp(cur->resp_llid, cur->resp_tid, on,
                                   width, height, cx, cy, cw, ch);
    cur->resp_llid = 0;
    cur->resp_tid = 0;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void layout_llid_destroy(int llid)
{
  t_layout_sub *sub = get_layout_sub(llid);
  if (sub)
    free_layout_sub(sub);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void layout_add_vm(char *name, int llid)
{
  int nb_eth;
  t_vm *vm;
  t_layout_node layout;
  vm = cfg_get_vm(name);
  if ((!vm) && (!cnt_name_exists(name, &nb_eth)))
    KERR("%s", name);
  else
    {
    if (vm)
      make_default_layout_node(&layout, name, vm->kvm.nb_tot_eth); 
    else
      make_default_layout_node(&layout, name, nb_eth); 
    add_layout_node(&layout);
    if (!(g_head_layout_sub) ||
         ((g_head_layout_sub) && (g_head_layout_sub->llid != llid)))
      recv_layout_node(0, 0, &layout);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void layout_del_vm(char *name)
{
  t_layout_node_xml *xml;
  xml = find_node_xml(name);
  if (!xml)
    KERR("%s", name);
  else
    extract_node_xml(xml);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void layout_add_sat(char *name, int llid)
{
  t_layout_sat layout;
  memset(&layout, 0, sizeof(t_layout_sat));
  strncpy(layout.name, name, MAX_NAME_LEN-1);
  layout.x = 50;
  layout.y = 50;
  layout.xa = A2B_DIA * VAL_INTF_POS_A2B;
  layout.ya = A2B_DIA * VAL_INTF_POS_A2B;
  layout.xb = -A2B_DIA * VAL_INTF_POS_A2B;
  layout.yb = A2B_DIA * VAL_INTF_POS_A2B;
  add_layout_sat(&layout);
  if (!(g_head_layout_sub) ||
       ((g_head_layout_sub) && (g_head_layout_sub->llid != llid)))
    recv_layout_sat(0, 0, &layout);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void layout_del_sat(char *name)
{
  t_layout_sat_xml *xml;
  xml = find_sat_xml(name);
  if (xml)
    extract_sat_xml(xml);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void layout_add_lan(char *name, int llid)
{
  t_layout_lan layout;
  t_layout_lan_xml *xml;
  if (!lan_get_with_name(name))
    KERR("%s", name);
  else
    {
    xml = find_lan_xml(name);
    if (!xml)
      {
      make_default_layout_lan(&layout, name);
      add_layout_lan(&layout);
      }
    xml = find_lan_xml(name);
    if (!xml)
      KERR("%s", name);
    else
      {
      if (!(g_head_layout_sub) ||
          ((g_head_layout_sub) && (g_head_layout_sub->llid != llid)))
        {
        recv_layout_lan(0, 0, &(xml->lan));
        }
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void layout_del_lan(char *name)
{ 
  t_layout_lan_xml *xml;
  xml = find_lan_xml(name);
  if (!xml)
    KERR("%s", name);
  else
    extract_lan_xml(xml);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int layout_node_solve(double x, double y)
{
  int i, ix, iy;
  ix = (int) x;
  iy = (int) y;
  for (i=0; i<MAX_POLAR_COORD; i++)
    {
    if ((ix >= g_node_x_coord[i]-1) && (ix <= g_node_x_coord[i]+1) &&
        (iy >= g_node_y_coord[i]-1) && (iy <= g_node_y_coord[i]+1))
      {
      break;
      }
    }
  return i;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int layout_a2b_solve(double x, double y)
{
  int i, ix, iy;
  ix = (int) x;
  iy = (int) y;
  for (i=0; i<MAX_POLAR_COORD; i++)
    {
    if ((ix >= g_a2b_x_coord[i]-1) && (ix <= g_a2b_x_coord[i]+1) &&
        (iy >= g_a2b_y_coord[i]-1) && (iy <= g_a2b_y_coord[i]+1))
      {
      break;
      }
    }
  return i;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void layout_topo_init(void)
{
  int i;
  double idx;
  memset(&g_layout_xml, 0, sizeof(t_layout_xml));
  g_head_layout_sub = NULL;
  g_authorized_to_move = 1;
  g_forbiden_to_move_by_cli = 0;
  for (i=0; i<MAX_POLAR_COORD; i++)
    {
    idx = (double) (2*i);
    idx = idx/100;
    g_node_x_coord[i] =  (NODE_DIA * VAL_INTF_POS_NODE * (sin(idx)));
    g_node_y_coord[i] = -(NODE_DIA * VAL_INTF_POS_NODE * (cos(idx)));
    g_a2b_x_coord[i] =  (A2B_DIA * VAL_INTF_POS_A2B * (sin(idx)));
    g_a2b_y_coord[i] = -(A2B_DIA * VAL_INTF_POS_A2B * (cos(idx)));
    }
  g_width = 0;
  g_height = 0;
  g_cx = 0; 
  g_cy = 0; 
  g_cw = 0; 
  g_ch = 0;
}
/*---------------------------------------------------------------------------*/

