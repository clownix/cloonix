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
#include <sys/socket.h>
#include <sys/types.h>
#include <math.h>
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "xml_layout.h"
#include "layout_rpc.h"
/*---------------------------------------------------------------------------*/
enum 
  {
  bnd_min = 0,
  bnd_layout_move_on_off,
  bnd_layout_width_height,
  bnd_layout_center_scale,
  bnd_layout_save_params_req,
  bnd_layout_save_params_resp,
  bnd_layout_zoom,
  bnd_layout_lan,
  bnd_layout_sat,
  bnd_layout_node,
  bnd_layout_modif,
  bnd_layout_event_sub,
  bnd_max,
  };

/*---------------------------------------------------------------------------*/
static char *g_sndbuf;
/*---------------------------------------------------------------------------*/
static char bound_list[bnd_max][MAX_CLOWNIX_BOUND_LEN];
/*---------------------------------------------------------------------------*/
static t_llid_tx g_llid_tx;
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void double2int(int *xe, int *xd, double x)
{
  double txe, txd;
  txe = trunc(x);
  txd = x - txe;
  *xe = (int) txe;
  *xd = (int) (txd * 1000000);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void int2double(int xe, int xd, double *x)
{
  double txe, txd;
  txe = (double) xe;
  txd = (double) xd;
  txd /= 1000000;
  *x = txe + txd;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void my_msg_mngt_tx(int llid, int len, char *buf)
{
  if (msg_exist_channel(llid))
    {
    if (!g_llid_tx)
      KOUT(" ");
    buf[len] = 0;
    g_llid_tx(llid, len + 1, buf);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void extract_boundary(char *input, char *output)
{
  int bound_len;
  if (input[0] != '<')
    KOUT("%s\n", input);
  bound_len = strcspn(input, ">");
  if (bound_len >= MAX_CLOWNIX_BOUND_LEN)
    KOUT("%s\n", input);
  if (bound_len < MIN_CLOWNIX_BOUND_LEN)
    KOUT("%s\n", input);
  memcpy(output, input, bound_len);
  output[bound_len] = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int get_bnd_arrival_event(char *bound)
{
  int i, result = 0;
  for (i=bnd_min; i<bnd_max; i++) 
    {
    if (!strcmp(bound, bound_list[i]))
      {
      result = i;
      break;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_layout_move_on_off(int llid, int tid, int on)
{
  int len;
  len = sprintf(g_sndbuf, LAYOUT_MOVE_ON_OFF, tid, on);
  my_msg_mngt_tx(llid, len, g_sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_layout_width_height(int llid, int tid, int width, int height)
{
  int len;
  len = sprintf(g_sndbuf, LAYOUT_WIDTH_HEIGHT, tid, width, height);
  my_msg_mngt_tx(llid, len, g_sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_layout_center_scale(int llid, int tid, int x, int y, int w, int h)
{
  int len;
  len = sprintf(g_sndbuf, LAYOUT_CENTER_SCALE, tid, x, y, w, h);
  my_msg_mngt_tx(llid, len, g_sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_layout_save_params_req(int llid, int tid)
{
  int len;
  len = sprintf(g_sndbuf, LAYOUT_SAVE_PARAMS_REQ, tid);
  my_msg_mngt_tx(llid, len, g_sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_layout_save_params_resp (int llid, int tid, int on, 
                                   int width, int height,
                                   int cx, int cy, int cw, int ch)
{
  int len;
  len = sprintf(g_sndbuf, LAYOUT_SAVE_PARAMS_RESP, tid, on, width, height,
                                                        cx, cy, cw, ch);
  my_msg_mngt_tx(llid, len, g_sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_layout_zoom(int llid, int tid, int zoom)
{
  int len;
  len = sprintf(g_sndbuf, LAYOUT_ZOOM, tid, zoom);
  my_msg_mngt_tx(llid, len, g_sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_layout_event_sub(int llid, int tid, int on)
{
  int len;
  len = sprintf(g_sndbuf, LAYOUT_EVENT_SUB, tid, on);
  my_msg_mngt_tx(llid, len, g_sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_layout_modif(int llid, int tid, int modif_type, char *name, int num,
                       int val1, int val2)
{
  int len = 0;
  if ((modif_type <= layout_modif_type_min) ||
      (modif_type >= layout_modif_type_max))
    KOUT("%d", modif_type);
  if ((!name) || (!strlen(name)) || (strlen(name)>= MAX_PATH_LEN))
    KOUT(" ");
  len = sprintf(g_sndbuf, LAYOUT_MODIF, tid, modif_type, name, num, val1, val2);
  my_msg_mngt_tx(llid, len, g_sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_layout_lan(int llid, int tid, t_layout_lan *layout_lan)
{
  int len = 0;
  int xe, xd, ye, yd;
  if (!layout_lan)
    KERR(" ");
  else
    {
    if (!strlen(layout_lan->name))
      KOUT(" ");
    double2int(&xe, &xd, layout_lan->x);
    double2int(&ye, &yd, layout_lan->y);
    len = sprintf(g_sndbuf, LAYOUT_LAN, tid, layout_lan->name, 
                                           xe, xd, ye, yd, 
                                           layout_lan->hidden_on_graph);
    my_msg_mngt_tx(llid, len, g_sndbuf);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_layout_sat(int llid, int tid, t_layout_sat *layout_sat)
{
  int len = 0;
  int xe, xd, ye, yd;
  int xae, xad, yae, yad;
  int xbe, xbd, ybe, ybd;

  if (!layout_sat)
    KERR(" ");
  else
    {
    if (!strlen(layout_sat->name))
      KOUT(" ");
    double2int(&xe, &xd, layout_sat->x);
    double2int(&ye, &yd, layout_sat->y);
    double2int(&xae, &xad, layout_sat->xa);
    double2int(&yae, &yad, layout_sat->ya);
    double2int(&xbe, &xbd, layout_sat->xb);
    double2int(&ybe, &ybd, layout_sat->yb);
    len = sprintf(g_sndbuf, LAYOUT_SAT, tid, layout_sat->name, 
                                           layout_sat->mutype,
                                           xe, xd, ye, yd,
                                           xae, xad, yae, yad,
                                           xbe, xbd, ybe, ybd,
                                           layout_sat->hidden_on_graph);
    my_msg_mngt_tx(llid, len, g_sndbuf);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_layout_node(int llid, int tid, t_layout_node *layout_node)
{
  int i, len = 0;
  int xe, xd, ye, yd;
  if (!layout_node)
    KERR(" ");
  else
    {
    if (!strlen(layout_node->name))
      KOUT(" ");
    double2int(&xe, &xd, layout_node->x);
    double2int(&ye, &yd, layout_node->y);
    len = sprintf(g_sndbuf, LAYOUT_NODE_O, tid,
                                         layout_node->name, 
                                         xe, xd, ye, yd,
                                         layout_node->hidden_on_graph,
                                         layout_node->color,
                                         layout_node->nb_eth_wlan);
    for (i=0; i<layout_node->nb_eth_wlan; i++)
      {
      double2int(&xe, &xd, layout_node->eth_wlan[i].x);
      double2int(&ye, &yd, layout_node->eth_wlan[i].y);
      len += sprintf(g_sndbuf+len, LAYOUT_INTF, xe, xd, ye, yd,
                                              layout_node->eth_wlan[i].hidden_on_graph);
      }
    len += sprintf(g_sndbuf+len, LAYOUT_NODE_C);
    my_msg_mngt_tx(llid, len, g_sndbuf);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void helper_get_eth(int nb, t_layout_eth_wlan *eth_wlan, char *msg) 
{
  int i;
  char *ptr = msg;
  int xe, xd, ye, yd;
  if ((nb < 0) || (nb > MAX_ETH_VM+MAX_WLAN_VM))
    KOUT("%d", nb);
  for (i=0; i<nb; i++)
    {
    ptr = strstr(ptr, "<eth_xyh>");
    if (!ptr)
      KOUT("%s", msg);
    if (sscanf(ptr, LAYOUT_INTF, &xe, &xd, &ye, &yd, 
                                 &eth_wlan[i].hidden_on_graph) != 5)
      KOUT("%s", msg);
    int2double(xe, xd, &(eth_wlan[i].x));
    int2double(ye, yd, &(eth_wlan[i].y));
    ptr = strstr(ptr, "</eth_xyh>");
    if (!ptr)
      KOUT("%s", msg);
    }
  ptr = strstr(ptr, "<eth_xyh>");
  if (ptr)
    KOUT("%s", msg);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void dispatcher(int llid, int bnd_evt, char *msg)
{
  int tid, type, val1, val2, nb;
  static char name[MAX_PATH_LEN];
  static t_layout_lan layout_lan;
  static t_layout_sat layout_sat;
  static t_layout_node layout_node;
  int on, width, height, xe, xd, ye, yd, x, y, w, h;
  int xea, xda, yea, yda, xeb, xdb, yeb, ydb;
  switch(bnd_evt)
    {
    case bnd_layout_move_on_off:
      if (sscanf(msg, LAYOUT_MOVE_ON_OFF, &tid, &val1) != 2)
        KOUT("%s", msg);
      recv_layout_move_on_off(llid, tid, val1);
      break;

   case bnd_layout_width_height:
      if (sscanf(msg, LAYOUT_WIDTH_HEIGHT, &tid, &val1, &val2) != 3)
        KOUT("%s", msg);
      recv_layout_width_height(llid, tid, val1, val2);
      break;

   case bnd_layout_center_scale:
      if (sscanf(msg, LAYOUT_CENTER_SCALE, &tid, &x, &y, &w, &h) != 5)
        KOUT("%s", msg);
      recv_layout_center_scale(llid, tid, x, y, w, h);
      break;

   case bnd_layout_save_params_req:
      if (sscanf(msg, LAYOUT_SAVE_PARAMS_REQ, &tid) != 1)
        KOUT("%s", msg);
      recv_layout_save_params_req(llid, tid);
      break;

   case bnd_layout_save_params_resp:
      if (sscanf(msg, LAYOUT_SAVE_PARAMS_RESP, &tid, &on, 
                                               &width, &height, 
                                               &x, &y, &w, &h) != 8)
        KOUT("%s", msg);
      recv_layout_save_params_resp(llid,tid,on,width,height,x,y,w,h);
      break;

   case bnd_layout_zoom:
      if (sscanf(msg, LAYOUT_ZOOM, &tid, &val1) != 2)
        KOUT("%s", msg);
      recv_layout_zoom(llid, tid, val1);
      break;

    case bnd_layout_event_sub:
      if (sscanf(msg, LAYOUT_EVENT_SUB, &tid, &val1) != 2)
        KOUT("%s", msg);
      recv_layout_event_sub(llid, tid,  val1);
      break;

    case bnd_layout_modif:
      if (sscanf(msg, LAYOUT_MODIF, &tid, &type, name, &nb,
                                    &val1, &val2) != 6)
        KOUT("%s", msg);
      recv_layout_modif(llid, tid, type, name, nb, val1, val2);
      break;

    case bnd_layout_lan:
      memset(&layout_lan, 0, sizeof(t_layout_lan));
      if (sscanf(msg, LAYOUT_LAN, &tid, layout_lan.name,
                                        &xe, &xd, &ye, &yd,
                                        &(layout_lan.hidden_on_graph)) != 7) 
        KOUT("%s", msg);
      int2double(xe, xd, &(layout_lan.x));
      int2double(ye, yd, &(layout_lan.y));
      recv_layout_lan(llid, tid, &layout_lan);
      break;

    case bnd_layout_sat:
      memset(&layout_sat, 0, sizeof(t_layout_sat));
      if (sscanf(msg, LAYOUT_SAT, &tid, layout_sat.name,
                                        &layout_sat.mutype,
                                        &xe, &xd, &ye, &yd,
                                        &xea, &xda, &yea, &yda,
                                        &xeb, &xdb, &yeb, &ydb,
                                        &(layout_sat.hidden_on_graph)) != 16) 
        KOUT("%s", msg);
      int2double(xe, xd, &(layout_sat.x));
      int2double(ye, yd, &(layout_sat.y));
      int2double(xea, xda, &(layout_sat.xa));
      int2double(yea, yda, &(layout_sat.ya));
      int2double(xeb, xdb, &(layout_sat.xb));
      int2double(yeb, ydb, &(layout_sat.yb));
      recv_layout_sat(llid, tid, &layout_sat);
      break;

    case bnd_layout_node:
      memset(&layout_node, 0, sizeof(t_layout_node));
      if (sscanf(msg, LAYOUT_NODE_O, &tid, layout_node.name,
                                     &xe, &xd, &ye, &yd,
                                     &(layout_node.hidden_on_graph),
                                     &(layout_node.color),
                                    &(layout_node.nb_eth_wlan)) != 9)
        KOUT("%s", msg);
      int2double(xe, xd, &(layout_node.x));
      int2double(ye, yd, &(layout_node.y));
      helper_get_eth(layout_node.nb_eth_wlan, layout_node.eth_wlan, msg); 
      recv_layout_node(llid, tid, &layout_node);
      break;

    default:
      KOUT(" ");
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int doors_io_layout_decoder (int llid, int len, char *str_rx)
{
  int result = -1;
  int bnd_arrival_event;
  char bound[MAX_CLOWNIX_BOUND_LEN];
  if (len != ((int) strlen(str_rx) + 1))
    KOUT(" ");
  extract_boundary(str_rx, bound);
  bnd_arrival_event = get_bnd_arrival_event(bound);
  if (bnd_arrival_event)
    {
    dispatcher(llid, bnd_arrival_event, str_rx);
    result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void doors_io_layout_xml_init(t_llid_tx llid_tx)
{
  g_llid_tx = llid_tx;
  if (!g_llid_tx)
    KOUT(" ");
  g_sndbuf = get_bigbuf();
  memset (bound_list, 0, bnd_max * MAX_CLOWNIX_BOUND_LEN);
  extract_boundary (LAYOUT_MOVE_ON_OFF, bound_list[bnd_layout_move_on_off]);
  extract_boundary (LAYOUT_WIDTH_HEIGHT, bound_list[bnd_layout_width_height]);
  extract_boundary (LAYOUT_CENTER_SCALE, bound_list[bnd_layout_center_scale]);
  extract_boundary (LAYOUT_ZOOM, bound_list[bnd_layout_zoom]);
  extract_boundary (LAYOUT_EVENT_SUB, bound_list[bnd_layout_event_sub]);
  extract_boundary (LAYOUT_LAN, bound_list[bnd_layout_lan]);
  extract_boundary (LAYOUT_SAT, bound_list[bnd_layout_sat]);
  extract_boundary (LAYOUT_NODE_O, bound_list[bnd_layout_node]);
  extract_boundary (LAYOUT_MODIF, bound_list[bnd_layout_modif]);
  extract_boundary (LAYOUT_SAVE_PARAMS_RESP, bound_list[bnd_layout_save_params_resp]);
  extract_boundary (LAYOUT_SAVE_PARAMS_REQ, bound_list[bnd_layout_save_params_req]);

}
/*---------------------------------------------------------------------------*/
