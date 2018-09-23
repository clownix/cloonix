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

typedef struct t_pbi_node
{
  char node_kernel[MAX_NAME_LEN];
  char node_rootfs_sod[MAX_PATH_LEN];
  char node_rootfs_backing_file[MAX_PATH_LEN];
  char install_cdrom[MAX_PATH_LEN];
  char added_cdrom[MAX_PATH_LEN];
  char added_disk[MAX_PATH_LEN];
  int  node_vm_id;
  int  node_vm_nb_eth;
  int  node_vm_config_flags;
  int  node_ram;
  int  node_cpu;
  int  node_evt_ping_ok;
  int  node_evt_cga_ping_ok;
  int  node_evt_dtach_launch_ok;
} t_pbi_node;

typedef struct t_pbi_sat
{
  void *snf_cr_item_recpath;
  void *snf_cr_item_onoff;
  t_topo_snf topo_snf;
  t_topo_c2c topo_c2c;
} t_pbi_sat;


typedef struct t_params_bank_item
{
  t_pbi_node  *pbi_node;
  t_pbi_sat *pbi_sat;
  int color_choice;
  int hidden_on_graph;
  int mutype;
  double x0, y0, x1, y1, x2, y2, x, y, dist;
  double tx, ty;
  double line_width;
  int flag;
  int flag_trace;
  int menu_on;
  int grabbed;
  int blink_rx;
  int blink_tx;
  double position_x, position_y;
  double force_x, force_y;
  double velocity_x, velocity_y;
  double mass;
} t_params_bank_item;
/*--------------------------------------------------------------------------*/
enum
{
  eorig_noedge = 0,
  eorig_update,
  eorig_modif,
};
/*--------------------------------------------------------------------------*/

enum
{
  bank_type_min = 0,
  bank_type_all_edges_items,
  bank_type_all_non_edges_items,
  bank_type_node,
  bank_type_eth,
  bank_type_c2c,
  bank_type_snf,
  bank_type_sat,
  bank_type_lan,
  bank_type_edge,
  bank_type_max,
};
/*--------------------------------------------------------------------------*/
struct t_bank_item;
/*--------------------------------------------------------------------------*/
typedef struct t_list_bank_item
  {
  struct t_bank_item *bitem;
  struct t_list_bank_item *prev;
  struct t_list_bank_item *next;
  } t_list_bank_item;
/*--------------------------------------------------------------------------*/
typedef struct t_bank_item
{
  int  bank_type;
  int  button_1_pressed;
  int  button_1_moved;
  int  button_1_double_click;
  int  spicy_gtk_pid;
  int  dtach_pid;
  int  qmonitor_pid;
  int  wireshark_pid;
  long long abs_beat_eua_timeout;
  int ref_eua_timeout;
  char tag[4];
  char name[MAX_PATH_LEN];
  char lan[MAX_PATH_LEN];
  int  num;
  int  nb_edges;
  void *cr_item;
  void *root;
  t_params_bank_item pbi;
  struct t_bank_item *att_node;
  struct t_bank_item *att_eth;
  struct t_bank_item *att_lan;
  t_list_bank_item *head_eth_list;
  t_list_bank_item *head_edge_list;
  struct t_bank_item *prev;
  struct t_bank_item *next;
  struct t_bank_item *glob_prev;
  struct t_bank_item *glob_next;
  struct t_bank_item *group_next;
  struct t_bank_item *group_head;
} t_bank_item;
/*--------------------------------------------------------------------------*/


int sat_is_a_a2b(char *name);

/****************************************************************************/
t_bank_item *bank_get_item(int bank_type, char *name, int num, char *lan);
/*--------------------------------------------------------------------------*/
void attached_edge_update_all(t_bank_item *bitem);
/*--------------------------------------------------------------------------*/
void enter_item_surface(t_bank_item *bitem);
void leave_item_surface(t_bank_item *bitem);
void leave_item_surface_action(t_bank_item *bitem);
/*--------------------------------------------------------------------------*/
void selectioned_flip_flop(t_bank_item *bitem);
void selectioned_mouse_button_1_press(t_bank_item *bitem, double x, double y);
int is_selectable(t_bank_item *bitem);
/*--------------------------------------------------------------------------*/
t_bank_item *get_currently_in_item_surface(void);
void init_bank_item(void);
int get_nb_total_items(void);
t_bank_item *get_first_glob_bitem(void);
/*--------------------------------------------------------------------------*/
t_bank_item *look_for_node_with_id(char *name);
void look_for_lan_and_del_all(void);
t_bank_item *look_for_lan_with_id(char *name);
int is_first_brtcl_on_canvas(void);
t_bank_item *look_for_sat_with_id(char *name);
t_bank_item *look_for_eth_with_id(char *name, int num);
/*--------------------------------------------------------------------------*/
void bank_node_create(char *name, char *kernel, char *rootfs_used,
                      char *rootfs_backing, char *install_cdrom,
                      char *added_cdrom, char *added_disk, int nb_eth,
                      int nb_wlan, 
                      int color_choice, int vm_id, int vm_config_flags,
                      double x, double y, int hidden_on_graph,
                      double *tx, double *ty, int32_t *thidden_on_graph);
/*--------------------------------------------------------------------------*/
void bank_edge_create(char *name, int num, char *lan); 
void bank_sat_create(char *name, int mutype, 
                     t_topo_c2c *c2c, t_topo_snf *snf,
                     double x, double y, 
                     double xa, double ya, 
                     double xb, double yb, 
                     int hidden_on_graph);
void bank_lan_create(char *name,  double x, double y, int hidden_on_graph);
/*--------------------------------------------------------------------------*/
void bank_edge_delete(char *name, int num, char *lan);
void bank_node_delete(char *name);
void bank_sat_delete(char *name);
void bank_lan_delete(char *name);
/*--------------------------------------------------------------------------*/
int get_max_edge_nb_per_item(void);
t_list_bank_item *get_head_connected_groups(void);
/*--------------------------------------------------------------------------*/
t_bank_item *get_head_lan(void);
/*--------------------------------------------------------------------------*/
void refresh_all_connected_groups(void);
/*--------------------------------------------------------------------------*/
int bank_get_qmonitor_pid(char *name);
void bank_set_qmonitor_pid(char *name, int val);
int bank_get_dtach_pid(char *name);
void bank_set_dtach_pid(char *name, int val);
int bank_get_spicy_gtk_pid(char *name);
void bank_set_spicy_gtk_pid(char *name, int val);
int bank_get_wireshark_pid(char *name);
void bank_set_wireshark_pid(char *name, int val);
/*--------------------------------------------------------------------------*/

void modify_c2c(char *name, char *master_cloonix, char *slave_cloonix);
void modify_snf(char *name, int evt, char *path);

int is_a_nat(t_bank_item *bitem);
int is_a_c2c(t_bank_item *bitem);
int is_a_a2b(t_bank_item *bitem);
int is_a_snf(t_bank_item *bitem);





