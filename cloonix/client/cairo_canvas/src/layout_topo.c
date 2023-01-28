/*****************************************************************************/
/*    Copyright (C) 2006-2023 clownix@clownix.net License AGPL-3             */
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
#include "layout_rpc.h"
#include "layout_topo.h"
#include "layout_x_y.h"
#include "main_timer_loop.h"
#include "bank.h"
#include "move.h"
#include "hidden_visible_edge.h"
#include "external_bank.h"
#include "make_layout.h"
#include "doorways_sock.h"
#include "client_clownix.h"
#include "topo.h"
#include "layout_topo.h"

static int g_x_cnt_coord[MAX_POLAR_COORD];
static int g_y_cnt_coord[MAX_POLAR_COORD];
static int g_x_node_coord[MAX_POLAR_COORD];
static int g_y_node_coord[MAX_POLAR_COORD];
static int g_x_a2b_coord[MAX_POLAR_COORD];
static int g_y_a2b_coord[MAX_POLAR_COORD];

static int g_ready_for_send;


static long long glob_abs_beat; 
static int glob_ref;

/*****************************************************************************/
static t_layout_xml *glob_layout_xml;
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void recv_layout_move_on_off(int llid, int tid, int on)
{
  if (on)
    request_move_stop_go(0);
  else
    request_move_stop_go(1);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_width_height(int llid, int tid, int width, int height)
{
  topo_set_window_resize(width, height);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_zoom(int llid, int tid, int zoom)
{
  if (zoom < 0)
    topo_zoom_in_out_canvas(0, -zoom);
  else if (zoom > 0)
    topo_zoom_in_out_canvas(1, zoom);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_center_scale(int llid, int tid, int x, int y, int w, int h)
{
  topo_set_center_scale(x, y, w, h);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_save_params_req(int llid, int tid)
{
  int on, width, height, cx, cy, cw, ch;
  topo_get_save_params(&on, &width, &height, &cx, &cy, &cw, &ch);
  send_layout_save_params_resp(llid, tid, on, width, height, cx, cy, cw, ch);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_save_params_resp(int llid, int tid, int on,
                                  int width, int height,
                                  int cx, int cy, int cw, int ch)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void free_layout_xml_node(t_layout_node_xml *node_xml)
{
  t_layout_node_xml *next, *cur = node_xml;
  while(cur)
    {
    next = cur->next;
    clownix_free(cur, __FUNCTION__);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_layout_xml_sat(t_layout_sat_xml *sat_xml)
{
  t_layout_sat_xml *next, *cur = sat_xml;
  while(cur)
    {
    next = cur->next;
    clownix_free(cur, __FUNCTION__);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_layout_xml_lan(t_layout_lan_xml *lan_xml)
{
  t_layout_lan_xml *next, *cur = lan_xml;
  while(cur)
    {
    next = cur->next;
    clownix_free(cur, __FUNCTION__);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void free_layout_xml_struct(t_layout_xml *layout_xml)
{
  if (layout_xml)
    {
    free_layout_xml_node(layout_xml->node_xml);
    free_layout_xml_sat(layout_xml->sat_xml);
    free_layout_xml_lan(layout_xml->lan_xml);
    clownix_free(layout_xml, __FUNCTION__);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void insert_lan_xml(t_layout_lan_xml *xml)
{
  if (glob_layout_xml->lan_xml)
    glob_layout_xml->lan_xml->prev = xml;
  xml->next = glob_layout_xml->lan_xml;
  glob_layout_xml->lan_xml = xml;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void extract_lan_xml(t_layout_lan_xml *xml)
{
  if (xml->prev)
    xml->prev->next = xml->next; 
  if (xml->next)
    xml->next->prev = xml->prev;
  if (xml == glob_layout_xml->lan_xml)
    glob_layout_xml->lan_xml = xml->next;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void insert_sat_xml(t_layout_sat_xml *xml)
{
  if (glob_layout_xml->sat_xml)
    glob_layout_xml->sat_xml->prev = xml;
  xml->next = glob_layout_xml->sat_xml;
  glob_layout_xml->sat_xml = xml;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void extract_sat_xml(t_layout_sat_xml *xml)
{
  if (xml->prev)
    xml->prev->next = xml->next; 
  if (xml->next)
    xml->next->prev = xml->prev;
  if (xml == glob_layout_xml->sat_xml)
    glob_layout_xml->sat_xml = xml->next;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void insert_node_xml(t_layout_node_xml *xml)
{
  if (glob_layout_xml->node_xml)
    glob_layout_xml->node_xml->prev = xml;
  xml->next = glob_layout_xml->node_xml;
  glob_layout_xml->node_xml = xml;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void extract_node_xml(t_layout_node_xml *xml)
{
  if (xml->prev)
    xml->prev->next = xml->next; 
  if (xml->next)
    xml->next->prev = xml->prev;
  if (xml == glob_layout_xml->node_xml)
    glob_layout_xml->node_xml = xml->next;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_layout_lan_xml *find_lan_xml(char *name)
{
  t_layout_lan_xml *cur = glob_layout_xml->lan_xml;
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
  t_layout_sat_xml *cur = glob_layout_xml->sat_xml;
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
  t_layout_node_xml *cur = glob_layout_xml->node_xml;
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
  t_bank_item *bitem;
  t_layout_lan_xml *cur, *xml;
  t_layout_lan *acur;
  xml = (t_layout_lan_xml *) clownix_malloc(sizeof(t_layout_lan_xml), 7);
  memset(xml, 0, sizeof(t_layout_lan_xml));
  memcpy(&(xml->lan), lan, sizeof(t_layout_lan));
  cur = find_lan_xml(lan->name);
  if (cur)
    {
    extract_lan_xml(cur);
    bitem = look_for_lan_with_id(lan->name);
    if (bitem)
      {
      acur = make_layout_lan(bitem);
      modif_position_layout(bitem, lan->x - acur->x,
                                   lan->y - acur->y);
      hidden_visible_modification(bitem, lan->hidden_on_graph);
      }
    clownix_free(cur, __FUNCTION__);
    }
  insert_lan_xml(xml);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void add_layout_sat(t_layout_sat *sat)
{
  t_bank_item *bitem, *eth_bitem;
  t_layout_sat_xml *cur, *xml;
  t_layout_sat *acur;
  xml = (t_layout_sat_xml *) clownix_malloc(sizeof(t_layout_sat_xml), 7);
  memset(xml, 0, sizeof(t_layout_sat_xml));
  memcpy(&(xml->sat), sat, sizeof(t_layout_sat));
  cur = find_sat_xml(sat->name);
  if (cur)
    {
    extract_sat_xml(cur);
    bitem = look_for_sat_with_id(sat->name);
    if (bitem)
      {
      acur = make_layout_sat(bitem);
      modif_position_layout(bitem, sat->x - acur->x,
                                   sat->y - acur->y);

      if (bitem->pbi.endp_type == endp_type_a2b)
        {
        eth_bitem = look_for_eth_with_id(sat->name, 0);
        if (!eth_bitem)
            KOUT("%s", sat->name);
        modif_position_eth(eth_bitem, sat->xa + sat->x,
                                      sat->ya + sat->y);
        eth_bitem->pbi.hidden_on_graph = sat->hidden_on_graph;
        eth_bitem = look_for_eth_with_id(sat->name, 1);
        if (!eth_bitem)
            KOUT("%s", sat->name);
        modif_position_eth(eth_bitem, sat->xb + sat->x,
                                      sat->yb + sat->y);
        eth_bitem->pbi.hidden_on_graph = sat->hidden_on_graph;
        }
      hidden_visible_modification(bitem, sat->hidden_on_graph);
      }
    clownix_free(cur, __FUNCTION__);
    }
  insert_sat_xml(xml);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void add_layout_node(t_layout_node *node)
{
  int i;
  t_bank_item *bitem, *eth_bitem;
  t_layout_node_xml *cur, *xml;
  t_layout_node *acur;
  xml = (t_layout_node_xml *) clownix_malloc(sizeof(t_layout_node_xml), 7);
  memset(xml, 0, sizeof(t_layout_node_xml));
  memcpy(&(xml->node), node, sizeof(t_layout_node));
  cur = find_node_xml(node->name);
  if (cur)
    {
    bitem = look_for_node_with_id(node->name);
    if (!bitem)
      bitem = look_for_cnt_with_id(node->name);
    if (bitem)
      {
      acur = make_layout_node(bitem);
      modif_position_layout(bitem, node->x - acur->x, 
                                   node->y - acur->y);
      for (i=0; i<node->nb_eth; i++)
        {
        eth_bitem = look_for_eth_with_id(node->name, i);
        if (!eth_bitem)
          KOUT(" ");
        modif_position_eth(eth_bitem,
                           node->eth[i].x + node->x, 
                           node->eth[i].y + node->y);
        eth_bitem->pbi.hidden_on_graph = node->eth[i].hidden_on_graph;
        }
      hidden_visible_modification(bitem, node->hidden_on_graph);
      }
    extract_node_xml(cur);
    clownix_free(cur, __FUNCTION__);
    }
  insert_node_xml(xml);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_lan(int llid, int tid, t_layout_lan *layout)
{
  add_layout_lan(layout);
  set_gene_layout_x_y(bank_type_lan, layout->name,
                      layout->x, layout->y, 0,0,0,0,
                      layout->hidden_on_graph);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_sat(int llid, int tid, t_layout_sat *layout)
{
  add_layout_sat(layout);
  set_gene_layout_x_y(bank_type_sat, layout->name,
                      layout->x, layout->y, 
                      layout->xa, layout->ya, 
                      layout->xb, layout->yb, 
                      layout->hidden_on_graph);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_node(int llid, int tid, t_layout_node *layout)
{
  int i;
  double tx[MAX_ETH_VM];
  double ty[MAX_ETH_VM];
  int hidden_on_graph[MAX_ETH_VM];
  add_layout_node(layout);
  for (i=0; i<MAX_ETH_VM; i++)
    {
    if (i<layout->nb_eth)
      {
      tx[i] = layout->eth[i].x;
      ty[i] = layout->eth[i].y;
      hidden_on_graph[i] = layout->eth[i].hidden_on_graph;
      }
    else
      {
      tx[i] = 0;
      ty[i] = 0;
      hidden_on_graph[i] = 0;
      }
    }
  set_node_layout_x_y(layout->name, 
                      layout->x, layout->y, 
                      layout->hidden_on_graph,
                      tx, ty, hidden_on_graph);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_event_sub(int llid, int tid, int on)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void clean_layout_xml(void)
{
  free_layout_xml_struct(glob_layout_xml);
  clownix_timeout_del(glob_abs_beat, glob_ref, __FILE__, __LINE__);
  layout_topo_init();
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void layout_round_node_eth_coords(double *x, double *y)
{
  int i, ix, iy, result = 0;
  for (i=0; (!result) && (i<MAX_POLAR_COORD); i++)
    {
    ix = (int) *x;
    iy = (int) *y;
    if ((ix >= g_x_node_coord[i]-1) && (ix <= g_x_node_coord[i]+1) && 
        (iy >= g_y_node_coord[i]-1) && (iy <= g_y_node_coord[i]+1)) 
      {
      result = 1;
      *x = (double) ix;
      *y = (double) iy;
      }
    }
  if (result == 0)
    {
    *x = 0;
    *y = 0;
    KERR("KO %g %g   %d %d", *x, *y, ix, iy);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void layout_round_cnt_eth_coords(double *x, double *y)
{
  int i, ix, iy, result = 0;
  for (i=0; (!result) && (i<MAX_POLAR_COORD); i++)
    {
    ix = (int) *x;
    iy = (int) *y;
    if ((ix >= g_x_cnt_coord[i]-1) && (ix <= g_x_cnt_coord[i]+1) &&
        (iy >= g_y_cnt_coord[i]-1) && (iy <= g_y_cnt_coord[i]+1))
      {
      result = 1;
      *x = (double) ix;
      *y = (double) iy;
      }
    }
  if (result == 0)
    {
    *x = 0;
    *y = 0;
    KERR("KO %g %g   %d %d", *x, *y, ix, iy);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void layout_round_a2b_eth_coords(double *x, double *y)
{
  int i, ix, iy, result = 0;
  for (i=0; (!result) && (i<MAX_POLAR_COORD); i++)
    {
    ix = (int) *x;
    iy = (int) *y;
    if ((ix >= g_x_a2b_coord[i]-1) && (ix <= g_x_a2b_coord[i]+1) &&
        (iy >= g_y_a2b_coord[i]-1) && (iy <= g_y_a2b_coord[i]+1))
      {
      result = 1;
      *x = (double) ix;
      *y = (double) iy;
      }
    }
  if (result == 0)
    KERR("KO %g %g   %d %d", *x, *y, ix, iy);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static int diff_sat(t_layout_sat_xml *layout_xml, t_layout_sat *layout)
{
  int result = 0;
  if (fabs (layout_xml->sat.x - layout->x) > 1)
    result = 1;
  if (fabs (layout_xml->sat.y - layout->y) > 1)
    result = 1;
  if (fabs (layout_xml->sat.xa - layout->xa) > 1)
    result = 1;
  if (fabs (layout_xml->sat.ya - layout->ya) > 1)
    result = 1;
  if (fabs (layout_xml->sat.xb - layout->xb) > 1)
    result = 1;
  if (fabs (layout_xml->sat.yb - layout->yb) > 1)
    result = 1;
  if (layout_xml->sat.hidden_on_graph != layout->hidden_on_graph)
    result = 1;
  return result;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static int diff_lan(t_layout_lan_xml *layout_xml, t_layout_lan *layout)
{
  int result = 0;
  if (fabs (layout_xml->lan.x - layout->x) > 1)
    result = 1;
  if (fabs (layout_xml->lan.y - layout->y) > 1)
    result = 1;
  if (layout_xml->lan.hidden_on_graph != layout->hidden_on_graph)
    result = 1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int diff_eth(t_layout_eth *layout_xml, t_layout_eth *layout)
{
  int result = 0;
  if (fabs (layout_xml->x - layout->x) > 1)
    result = 1;
  if (fabs (layout_xml->y - layout->y) > 1)
    result = 1;
  if (layout_xml->hidden_on_graph != layout->hidden_on_graph)
    result = 1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int diff_node(t_layout_node_xml *layout_xml, t_layout_node *layout)
{
  int i, result = 0;
  t_layout_eth *layout_eth_xml;
  t_layout_eth *layout_eth;
  if (!result)
    {
    if (fabs (layout_xml->node.x - layout->x) > 1)
      result = 1;
    }
  if (!result)
    {
    if (fabs (layout_xml->node.y - layout->y) > 1)
      result = 1;
    }
  if (!result)
    {
    if (layout_xml->node.hidden_on_graph != layout->hidden_on_graph)
      result = 1;
    }
  if (!result)
    {
    for (i=0; (!result) && (i<layout->nb_eth); i++)
      {
      layout_eth_xml = &(layout_xml->node.eth[i]);
      layout_eth = &(layout->eth[i]);
      if (diff_eth(layout_eth_xml, layout_eth))
        {
        result = 1;
        }
      }
    }

  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_synchro(void *data)
{
  t_layout_node_xml *layout_node_xml;
  t_layout_sat_xml *layout_sat_xml;
  t_layout_lan_xml *layout_lan_xml;
  t_layout_node *layout_node;
  t_layout_sat *layout_sat;
  t_layout_lan *layout_lan;
  t_bank_item *cur;

  cur = get_first_glob_bitem();
  while (cur)
    {
    switch (cur->bank_type)
      {
      case bank_type_cnt:
      case bank_type_node:
        layout_node_xml = find_node_xml(cur->name);
        layout_node = make_layout_node(cur);
        if ((!layout_node_xml) ||
            (diff_node(layout_node_xml, layout_node)))
          {
          layout_send_layout_node(layout_node);
          }
        break;
      case bank_type_eth:
        break;
      case bank_type_lan:
        layout_lan_xml = find_lan_xml(cur->name);
        layout_lan = make_layout_lan(cur);
        if ((!layout_lan_xml) ||
            (diff_lan(layout_lan_xml, layout_lan)))
          {
          layout_send_layout_lan(layout_lan);
          }
        break;

      case bank_type_sat:
        layout_sat_xml = find_sat_xml(cur->name);
        layout_sat = make_layout_sat(cur);
        if ((!layout_sat_xml) ||
            (diff_sat(layout_sat_xml, layout_sat)))
          {
          layout_send_layout_sat(layout_sat);
          }
        break;


      default:
        KOUT(" ");
      }
    cur = cur->glob_next;
    }
  glib_prepare_rx_tx(get_clownix_main_llid());
  clownix_timeout_add(500, timer_synchro, NULL, &glob_abs_beat, &glob_ref);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void layout_send_layout_node(t_layout_node *layout)
{
  add_layout_node(layout);
  send_layout_node(get_clownix_main_llid(), 8888, layout);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void layout_send_layout_lan(t_layout_lan *layout)
{
  add_layout_lan(layout);
  send_layout_lan(get_clownix_main_llid(), 8888, layout);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void layout_send_layout_sat(t_layout_sat *layout)
{
  add_layout_sat(layout);
  send_layout_sat(get_clownix_main_llid(), 8888, layout);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void layout_set_ready_for_send(void)
{
  g_ready_for_send = 1;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int layout_get_ready_for_send(void)
{
  return g_ready_for_send;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void layout_topo_init(void)
{
  int i;
  double idx;
  g_ready_for_send = 0;
  glob_layout_xml = (t_layout_xml *) clownix_malloc(sizeof(t_layout_xml), 7);
  memset(glob_layout_xml, 0, sizeof(t_layout_xml));
  clownix_timeout_add(500, timer_synchro, NULL, &glob_abs_beat, &glob_ref);
  for (i=0; i<MAX_POLAR_COORD; i++)
    {
    idx = (double) (2*i);
    idx = idx/100;
    g_x_cnt_coord[i] =  (CNT_NODE_DIA * VAL_INTF_POS_NODE * (sin(idx)));
    g_y_cnt_coord[i] = -(CNT_NODE_DIA * VAL_INTF_POS_NODE * (cos(idx)));
    g_x_node_coord[i] =  (NODE_DIA * VAL_INTF_POS_NODE * (sin(idx)));
    g_y_node_coord[i] = -(NODE_DIA * VAL_INTF_POS_NODE * (cos(idx)));
    g_x_a2b_coord[i] =  (A2B_DIA * VAL_INTF_POS_A2B * (sin(idx)));
    g_y_a2b_coord[i] = -(A2B_DIA * VAL_INTF_POS_A2B * (cos(idx)));
    }
}
/*---------------------------------------------------------------------------*/



