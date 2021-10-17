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
enum
{
  layout_modif_type_min = 0,

  layout_modif_type_lan_hide,
  layout_modif_type_sat_hide,
  layout_modif_type_eth_hide,
  layout_modif_type_vm_hide,

  layout_modif_type_lan_xy,
  layout_modif_type_sat_xy,
  layout_modif_type_eth_xy,
  layout_modif_type_vm_xy,

  layout_modif_type_lan_abs_xy,
  layout_modif_type_sat_abs_xy,
  layout_modif_type_eth_abs_xy,
  layout_modif_type_vm_abs_xy,

  layout_modif_type_max,
};
/*---------------------------------------------------------------------------*/
typedef struct t_layout_eth_wlan
{
  double x;
  double y;
  int hidden_on_graph;
} t_layout_eth_wlan;
/*--------------------------------------------------------------------------*/
typedef struct t_layout_sat
{
  char name[MAX_NAME_LEN];
  double x;
  double y;
  double xa;
  double ya;
  double xb;
  double yb;
  int hidden_on_graph;
} t_layout_sat;
/*--------------------------------------------------------------------------*/
typedef struct t_layout_lan
{
  char name[MAX_NAME_LEN];
  double x;
  double y;
  int hidden_on_graph;
} t_layout_lan;
/*--------------------------------------------------------------------------*/
typedef struct t_layout_node
{
  char name[MAX_NAME_LEN];
  double x;
  double y;
  int hidden_on_graph;
  int color;
  int nb_eth_wlan;
  t_layout_eth_wlan eth_wlan[MAX_DPDK_VM];
} t_layout_node;
/*--------------------------------------------------------------------------*/
enum {
  stop_go_move_graph_unknown = 0,
  stop_go_move_graph_stop,
  stop_go_move_graph_move,
};
/*--------------------------------------------------------------------------*/
typedef struct t_layout_lan_xml
{
  t_layout_lan lan;
  struct t_layout_lan_xml *prev;
  struct t_layout_lan_xml *next;
} t_layout_lan_xml;
/*--------------------------------------------------------------------------*/
typedef struct t_layout_sat_xml
{
  t_layout_sat sat;
  struct t_layout_sat_xml *prev;
  struct t_layout_sat_xml *next;
} t_layout_sat_xml;
/*--------------------------------------------------------------------------*/
typedef struct t_layout_node_xml
{
  t_layout_node node;
  struct t_layout_node_xml *prev;
  struct t_layout_node_xml *next;
} t_layout_node_xml;
/*--------------------------------------------------------------------------*/
typedef struct t_layout_xml
{
  t_layout_node_xml *node_xml;
  t_layout_sat_xml *sat_xml;
  t_layout_lan_xml *lan_xml;
} t_layout_xml;
/*--------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
void doors_io_layout_xml_init(t_llid_tx llid_tx);
int  doors_io_layout_decoder(int llid, int len, char *str_rx);
/*---------------------------------------------------------------------------*/
void send_layout_zoom(int llid, int tid, int zoom);
void recv_layout_zoom(int llid, int tid, int zoom);

void send_layout_move_on_off(int llid, int tid, int on);
void recv_layout_move_on_off(int llid, int tid, int on);
void send_layout_width_height(int llid, int tid, int width, int height);
void recv_layout_width_height(int llid, int tid, int width, int height);
void send_layout_center_scale(int llid, int tid, int x, int y, int w, int h);
void recv_layout_center_scale(int llid, int tid, int x, int y, int w, int h);

void send_layout_save_params_req(int llid, int tid);
void recv_layout_save_params_req(int llid, int tid);
void send_layout_save_params_resp(int llid, int tid, int on, 
                                  int width, int height,
                                  int cx, int cy, int cw, int ch);
void recv_layout_save_params_resp(int llid, int tid, int on, 
                                  int width, int height,
                                  int cx, int cy, int cw, int ch);

void send_layout_move_on_off_resp(int llid, int tid, int on);
void recv_layout_move_on_off_resp(int llid, int tid, int on);
void send_layout_width_height_resp(int llid, int tid, int width, int height);
void recv_layout_width_height_resp(int llid, int tid, int width, int height);
void send_layout_center_scale_resp(int llid, int tid, int x, int y, int w, int h);
void recv_layout_center_scale_resp(int llid, int tid, int x, int y, int w, int h);



void send_layout_lan(int llid, int tid, t_layout_lan *layout_lan);
void recv_layout_lan(int llid, int tid, t_layout_lan *layout_lan);
void send_layout_sat(int llid, int tid, t_layout_sat *layout_sat);
void recv_layout_sat(int llid, int tid, t_layout_sat *layout_sat);
void send_layout_node(int llid, int tid, t_layout_node *layout_node);
void recv_layout_node(int llid, int tid, t_layout_node *layout_node);
void send_layout_event_sub(int llid, int tid, int on);
void recv_layout_event_sub(int llid, int tid, int on);
void send_layout_modif(int llid, int tid, int modif_type, char *name, int num, 
                       int val1, int val2);
void recv_layout_modif(int llid, int tid, int modif_type, char *name, int num, 
                       int val1, int val2);

void send_zoom_in_out_canvas(int llid, int tid, int in, int val);
void recv_zoom_in_out_canvas(int llid, int tid, int in, int val);
void event_set_main_window_coords(int llid, int tid, 
                                  int x, int y, int w, int h);

/*****************************************************************************/








