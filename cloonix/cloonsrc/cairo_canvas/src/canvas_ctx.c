/*****************************************************************************/
/*    Copyright (C) 2006-2025 clownix@clownix.net License AGPL-3             */
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
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <libcrcanvas.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doorways_sock.h"
#include "client_clownix.h"
#include "interface.h"
#include "commun_consts.h"
#include "bank.h"
#include "move.h"
#include "popup.h"
#include "cloonix.h"
#include "main_timer_loop.h"
#include "pid_clone.h"
#include "menu_utils.h"
#include "menus.h"
#include "hidden_visible_edge.h"
#include "menu_dialog_kvm.h"
#include "menu_dialog_cnt.h"
#include "menu_dialog_c2c.h"
#include "layout_rpc.h"
#include "layout_topo.h"
#include "cloonix_conf_info.h"
#include "topo.h"

static int g_inhib_timeout_recall_menu;
static int g_right_click_inhib;

#define BRANDTYPE_NB_MAX 3
static void local_canvas_ctx_menu(void);
static void update_brandtype_image(void);

static double g_x_mouse, g_y_mouse;
GtkWidget *get_main_window(void);
extern GtkWidget *gtkwidget_canvas;

t_cloonix_conf_info *get_own_cloonix_conf_info(void);

void topo_get_matrix_inv_transform_point(double *x, double *y);
void put_top_left_icon(GtkWidget *mainwin);

static t_topo_info_phy g_topo_phy[MAX_PHY];
static int g_nb_phy;

static int g_nb_brgs;
static t_topo_bridges g_brgs[MAX_OVS_BRIDGES];

static char g_brandtype_type[MAX_NAME_LEN];
static char g_brandtype_image_zip[MAX_NAME_LEN];
static char g_brandtype_image_cvm[MAX_NAME_LEN];
static char g_brandtype_image_kvm[MAX_NAME_LEN];
/*--------------------------------------------------------------------------*/
static t_slowperiodic g_bulzip[MAX_BULK_FILES];
static t_slowperiodic g_bulcvm[MAX_BULK_FILES];
static t_slowperiodic g_bulkvm[MAX_BULK_FILES];
static int g_nb_bulkvm;
static int g_nb_bulzip;
static int g_nb_bulcvm;



/****************************************************************************/
static void get_txt_for_zip_cvm_kvm(char *xvm)
{
  memset(xvm, 0, MAX_NAME_LEN);
  if (!strcmp(g_brandtype_type, "brandkvm"))
    snprintf(xvm, MAX_NAME_LEN-1, "%s", g_brandtype_image_kvm);
  else if (!strcmp(g_brandtype_type, "brandzip"))
    snprintf(xvm, MAX_NAME_LEN-1, "%s", g_brandtype_image_zip);
  else if (!strcmp(g_brandtype_type, "brandcvm"))
    snprintf(xvm, MAX_NAME_LEN-1, "%s", g_brandtype_image_cvm);
  else
    KOUT("ERROR %s", g_brandtype_type);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void get_txt_for_cnf(char *cnf)
{ 
  memset(cnf, 0, MAX_NAME_LEN);
  if (!strcmp(g_brandtype_type, "brandkvm"))
    snprintf(cnf, MAX_NAME_LEN-1, "kvm config");
  else if (!strcmp(g_brandtype_type, "brandzip"))
    snprintf(cnf, MAX_NAME_LEN-1, "zip config");
  else if (!strcmp(g_brandtype_type, "brandcvm"))
    snprintf(cnf, MAX_NAME_LEN-1, "cvm config");
  else
    KOUT("ERROR %s", g_brandtype_type);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void call_cloonix_interface_lan_create(double x, double y)
{
  double x0=x, y0=y;
  static int num = 1;
  char name[MAX_NAME_LEN];
  sprintf(name, "lan%02d", num++);
  topo_get_matrix_inv_transform_point(&x0, &y0);
  to_cloonix_switch_create_lan(name, x0, y0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void call_cloonix_interface_tap_create(double x, double y)
{
  double x0=x, y0=y;
  static int num = 1;
  char net_name[9];
  char name[MAX_NAME_LEN];
  snprintf(net_name, 8, "%s", get_net_name());
  net_name[7] = 0;
  snprintf(name, IFNAMSIZ-1, "%stap%d", net_name, num++);
  name[IFNAMSIZ-1] = 0;
  topo_get_matrix_inv_transform_point(&x0, &y0);
  to_cloonix_switch_create_sat(name, endp_type_taps, NULL, x0, y0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void call_interface_absorb_phy_create(char *name, double x, double y)
{
  double x0=x, y0=y;
  topo_get_matrix_inv_transform_point(&x0, &y0);
  to_cloonix_switch_create_sat(name, endp_type_phyas, NULL, x0, y0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void call_interface_macvlan_phy_create(char *name, double x, double y)
{
  double x0=x, y0=y;
  topo_get_matrix_inv_transform_point(&x0, &y0);
  to_cloonix_switch_create_sat(name, endp_type_phyms, NULL, x0, y0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void call_cloonix_interface_nat_create(double x, double y)
{
  double x0=x, y0=y;
  topo_get_matrix_inv_transform_point(&x0, &y0);
  to_cloonix_switch_create_sat("nat", endp_type_natv, NULL, x0, y0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void call_cloonix_interface_c2c_create(double x, double y)
{
  t_c2c_req_info c2c_req_info;
  t_custom_c2c *c2c = get_custom_c2c();
  double x0=x, y0=y;
  int rank;
  t_cloonix_conf_info *cnf, *local_cnf;
  cnf = cloonix_cnf_info_get(c2c->dist_cloon, &rank);
  if (!cnf)
    insert_next_warning(cloonix_conf_info_get_names(), 1);
  else
    {
    local_cnf = get_own_cloonix_conf_info();
    memset(&c2c_req_info, 0, sizeof(t_c2c_req_info));
    strncpy(c2c_req_info.name, c2c->name, MAX_NAME_LEN-1);  
    strncpy(c2c_req_info.dist_cloon, c2c->dist_cloon, MAX_NAME_LEN-1);
    strncpy(c2c_req_info.dist_passwd, cnf->passwd, MSG_DIGEST_LEN-1);
    c2c_req_info.loc_udp_ip = local_cnf->c2c_udp_ip; 
    c2c_req_info.c2c_udp_port_low = local_cnf->c2c_udp_port_low;
    c2c_req_info.dist_tcp_ip = cnf->ip;
    c2c_req_info.dist_tcp_port = (uint16_t) (cnf->port & 0xFFFF);
    c2c_req_info.dist_udp_ip = cnf->c2c_udp_ip;;
    topo_get_matrix_inv_transform_point(&x0, &y0);
    to_cloonix_switch_create_sat(c2c->name, endp_type_c2cv,
                                 &(c2c_req_info), x0, y0);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void call_cloonix_interface_a2b_create(double x, double y)
{
  double x0=x, y0=y;
  static int num = 1;
  char name[MAX_NAME_LEN];
  memset(name, 0, MAX_NAME_LEN);
  snprintf(name, MAX_NAME_LEN-1, "a2b%d", num++);
  topo_get_matrix_inv_transform_point(&x0, &y0);
  to_cloonix_switch_create_sat(name, endp_type_a2b, NULL, x0, y0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void call_cloonix_interface_node_create(double x, double y)
{
  int rest, i;
  double tx[MAX_ETH_VM];
  double ty[MAX_ETH_VM];
  double x0=x, y0=y;
  memset(tx, 0, (MAX_ETH_VM) * sizeof(double));
  memset(ty, 0, (MAX_ETH_VM) * sizeof(double));
  for (i=0; i<MAX_ETH_VM; i++)
    {
    rest = i%4;
    if (rest == 0)
      {
      tx[i] = (double) (NODE_DIA * VAL_INTF_POS_NODE);
      ty[i] = (double) (-NODE_DIA * VAL_INTF_POS_NODE);
      }
    if (rest == 1)
      {
      tx[i] = (double) (NODE_DIA * VAL_INTF_POS_NODE);
      ty[i] = (double) (NODE_DIA * VAL_INTF_POS_NODE);
      }
    if (rest == 2)
      {
      tx[i] = (double) (NODE_DIA * VAL_INTF_POS_NODE);
      ty[i] = (double) (NODE_DIA * VAL_INTF_POS_NODE);
      }
    if (rest == 3)
      {
      tx[i] = (double) (-NODE_DIA * VAL_INTF_POS_NODE);
      ty[i] = (double) (NODE_DIA * VAL_INTF_POS_NODE);
      }
    }
  topo_get_matrix_inv_transform_point(&x0, &y0);
  to_cloonix_switch_create_node(0, x0, y0, tx, ty);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void call_cloonix_interface_node_cnt_create(double x, double y) 
{ 
  int rest, i;
  double tx[MAX_ETH_VM];
  double ty[MAX_ETH_VM];
  double x0=x, y0=y;
  memset(tx, 0, (MAX_ETH_VM) * sizeof(double));
  memset(ty, 0, (MAX_ETH_VM) * sizeof(double));
  for (i=0; i<MAX_ETH_VM; i++)
    {
    rest = i%4; 
    if (rest == 0)
      {
      tx[i] = (double) (CNT_DIA * VAL_INTF_POS_NODE);
      ty[i] = (double) (-CNT_DIA * VAL_INTF_POS_NODE);
      }
    if (rest == 1)
      {
      tx[i] = (double) (CNT_DIA * VAL_INTF_POS_NODE);
      ty[i] = (double) (CNT_DIA * VAL_INTF_POS_NODE);
      }
    if (rest == 2)
      {
      tx[i] = (double) (CNT_DIA * VAL_INTF_POS_NODE);
      ty[i] = (double) (CNT_DIA * VAL_INTF_POS_NODE);
      }
    if (rest == 3)
      {
      tx[i] = (double) (-CNT_DIA * VAL_INTF_POS_NODE);
      ty[i] = (double) (CNT_DIA * VAL_INTF_POS_NODE);
      }
    }
  topo_get_matrix_inv_transform_point(&x0, &y0);
  to_cloonix_switch_create_node(1, x0, y0, tx, ty);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void hidden_visible_check_toggled(GtkToggleButton *check, 
                                         t_bank_item *bi)
{
  int val = 0;
  t_list_bank_item *intf;
  if (gtk_toggle_button_get_active(check))
    val = 1;
  hidden_visible_modification(bi, val);
  if ((bi->bank_type == bank_type_node) ||
      (bi->bank_type == bank_type_cnt))
    {
    intf = bi->head_eth_list;
    while (intf)
      {
      hidden_visible_modification(intf->bitem, val);
      intf = intf->next;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static GtkWidget *hidden_visible_check_button(t_bank_item *bi, int is_eth)
{
  GtkWidget *check;
  char label[2*MAX_PATH_LEN];
  memset(label, 0, 2*MAX_PATH_LEN);
  if (is_eth)
    {
    snprintf(label, 2*MAX_PATH_LEN, "%s intf %d", bi->name, bi->num);
    label[MAX_PATH_LEN-1] = 0;
    }
  else
    {
    snprintf(label, MAX_PATH_LEN, "%s", bi->name);
    label[MAX_PATH_LEN-1] = 0;
    }
  check = gtk_check_button_new_with_label(label);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(check), TRUE);
  g_signal_connect(G_OBJECT(check), "toggled", 
                   G_CALLBACK(hidden_visible_check_toggled),
                   (gpointer) bi);
  return check;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_bank_item *get_next_hidden(t_bank_item *prev, int *is_eth)
{
  t_bank_item *cur;
  *is_eth = 0;
  if (prev == NULL)
    cur = get_first_glob_bitem();
  else
    cur = prev->glob_next;
  while (cur)
    {
    if (cur->pbi.hidden_on_graph)
      {
      if (cur->bank_type != bank_type_eth)
        break;
      else
        {
        if (cur->att_node->pbi.hidden_on_graph == 0)
          {
          *is_eth = 1;
          break;
          }
        }
      }
    cur = cur->glob_next;
    }
  return (cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void hidden_visible_cb(void)
{
  int is_eth;
  t_bank_item *prev = NULL;
  t_bank_item *cur;
  GtkWidget *dialog, *check;
  GtkWidget *parent = get_main_window();
  dialog = gtk_message_dialog_new(GTK_WINDOW(parent), 
                                  GTK_DIALOG_DESTROY_WITH_PARENT, 
                                  GTK_MESSAGE_OTHER,
                                  GTK_BUTTONS_CLOSE, 
                                  "Toggle to make visible");
  gtk_window_set_title(GTK_WINDOW(dialog), "Hidden/Visible");

  do
    {
    cur = get_next_hidden(prev, &is_eth);
    if (cur)
      {
      check = hidden_visible_check_button(cur, is_eth);
      gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                         check,FALSE,TRUE,0);
      prev = cur;
      }
    } while (cur);

  gtk_widget_show_all(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
  gtk_widget_show_all(dialog);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rad_stop_cb(GtkWidget *check, gpointer data)
{
  unsigned long val = (unsigned long) data;
  if (!check)
    KOUT(" ");
  request_move_stop_go((int)val);
  if (val)
    send_layout_move_on_off(get_clownix_main_llid(), 8888, 0);
  else
    send_layout_move_on_off(get_clownix_main_llid(), 8888, 1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void stop_go_command(GtkWidget *mn)
{
  GtkWidget *menu, *rad1, *rad2;
  GSList *group = NULL;
  int stopped = get_move_freeze();
  if (!mn)
    KOUT(" ");
  rad1 = gtk_radio_menu_item_new_with_label(group, "stop");
  group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(rad1));
  rad2 = gtk_radio_menu_item_new_with_label(group, "go");
  menu = gtk_menu_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), rad1);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), rad2);
  g_signal_connect(G_OBJECT(rad1),"activate",
                     G_CALLBACK(rad_stop_cb), (gpointer) 1);
  g_signal_connect(G_OBJECT(rad2),"activate",
                     G_CALLBACK(rad_stop_cb), (gpointer) 0);
  if (stopped)
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(rad1), TRUE);
  else
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(rad2), TRUE);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(mn), menu);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void trace_textview_scroll_to_bottom (GtkWidget *textview, char *txt)
{
  GtkTextBuffer *buffer;
  GtkTextIter start_iter;
  GtkTextIter end_iter;
  GtkTextMark *mark;
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW(textview));
  gtk_text_buffer_get_end_iter (buffer, &end_iter);
  gtk_text_buffer_insert (buffer, &end_iter, txt, -1);
  gtk_text_buffer_get_bounds (buffer, &start_iter, &end_iter);
  mark = gtk_text_buffer_get_mark (buffer, "scroll");
  gtk_text_iter_set_line_offset(&end_iter, 0);
  gtk_text_buffer_move_mark (buffer, mark, &end_iter);
  gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(textview), mark);
  gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(textview),
                               mark, 0.0, TRUE, 0.0, 0.0);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void callback_list_commands(int tid, int qty, t_list_commands *list)
{

  GtkWidget *text_view_window,  *swindow, *frame, *textview;
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  char title[MAX_NAME_LEN];
  int i;
  sprintf(title, "LIST CMDS TO CREATE TOPO");
  text_view_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_modal (GTK_WINDOW(text_view_window), FALSE);
  gtk_window_set_title (GTK_WINDOW (text_view_window), title);
  gtk_window_set_default_size(GTK_WINDOW(text_view_window), 300, 200);
  put_top_left_icon(text_view_window);
  swindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swindow),
                                 GTK_POLICY_NEVER,
                                 GTK_POLICY_AUTOMATIC);
  frame = gtk_frame_new ("Cmds");
  gtk_container_add (GTK_CONTAINER (frame), swindow);
  textview = gtk_text_view_new ();
  gtk_container_add (GTK_CONTAINER (swindow), textview);
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW(textview));
  gtk_text_buffer_get_end_iter (buffer, &iter);
  gtk_text_buffer_create_mark (buffer, "scroll", &iter, TRUE);
  gtk_container_add (GTK_CONTAINER (text_view_window), frame);
  gtk_widget_show_all(text_view_window);
  for (i=0; i<qty; i++)
    {
    trace_textview_scroll_to_bottom (textview, list[i].cmd);
    trace_textview_scroll_to_bottom (textview, "\n");
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void cmd_cact(void)
{
  client_list_commands(0, callback_list_commands);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void topo_lay(void)
{
  client_lay_commands(0, callback_list_commands);
}
/*--------------------------------------------------------------------------*/
 
/****************************************************************************/
static void show_old_warnings(void)
{
  stored_warning_trace_textview_create();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timeout_recall_menu(void *data)
{
  local_canvas_ctx_menu();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void lan_act(void)
{
  if (g_right_click_inhib == 0)
    call_cloonix_interface_lan_create(g_x_mouse, g_y_mouse);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void tap_act(void)
{
  if (g_right_click_inhib == 0)
    call_cloonix_interface_tap_create(g_x_mouse, g_y_mouse);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void xvm_act(void)
{
  if (g_right_click_inhib == 0)
    {
    if (!strcmp(g_brandtype_type, "brandkvm"))
      call_cloonix_interface_node_create(g_x_mouse, g_y_mouse);
    else
      call_cloonix_interface_node_cnt_create(g_x_mouse, g_y_mouse);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void c2c_act(void)
{
  if (g_right_click_inhib == 0)
    call_cloonix_interface_c2c_create(g_x_mouse, g_y_mouse);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void a2b_act(void)
{
  if (g_right_click_inhib == 0)
    call_cloonix_interface_a2b_create(g_x_mouse, g_y_mouse);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void nat_act(void)
{
  if (g_right_click_inhib == 0)
    call_cloonix_interface_nat_create(g_x_mouse, g_y_mouse);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void cnf_act(void)
{
  if (g_right_click_inhib == 0)
    {
    if (!strcmp(g_brandtype_type, "brandkvm"))
      menu_choice_kvm();
    else
      menu_choice_cnt();
    clownix_timeout_add(1, timeout_recall_menu, NULL, NULL, NULL);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void c2c_cact(void)
{
  menu_choice_c2c_params();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void macvlan_phy(gpointer data)
{
  unsigned long i = (unsigned long) data;
  call_interface_macvlan_phy_create(g_topo_phy[i].name, g_x_mouse, g_y_mouse);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void absorb_phy(gpointer data)
{
  unsigned long i = (unsigned long) data;
  call_interface_absorb_phy_create(g_topo_phy[i].name, g_x_mouse, g_y_mouse);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void phy_sub_menu(GtkWidget *phy)
{
  unsigned long i;
  GtkWidget *menu_intf = gtk_menu_new();
  GtkWidget *menu, *item, *item_intf;
  if (!phy)
    KOUT(" ");
  for (i=0; i<g_nb_phy; i++)
    {
    menu = gtk_menu_new();
    item_intf = gtk_menu_item_new_with_label(g_topo_phy[i].name);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_intf), item_intf);

    item = gtk_menu_item_new_with_label("absorb");
    g_signal_connect_swapped(G_OBJECT(item), "activate",
                             G_CALLBACK(absorb_phy), (gpointer) i);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("macvlan");
    g_signal_connect_swapped(G_OBJECT(item), "activate",
                             G_CALLBACK(macvlan_phy), (gpointer) i);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item_intf), menu);
    }
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(phy), menu_intf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void other_sub_menu(GtkWidget *other)
{
  if (g_right_click_inhib == 0)
    {
    GtkWidget *phy  = gtk_menu_item_new_with_label("phy");
    GtkWidget *cmd_list = gtk_menu_item_new_with_label("cmd_list");
    GtkWidget *c2c_conf = gtk_menu_item_new_with_label("c2c_conf");
    GtkWidget *stop = gtk_menu_item_new_with_label("Motion");
    GtkWidget *warnings = gtk_menu_item_new_with_label("Previous Warnings");
    GtkWidget *hidden_visible = gtk_menu_item_new_with_label("Hidden/Visible");
    GtkWidget *del = gtk_menu_item_new_with_label("Delete Topo");
    GtkWidget *lst = gtk_menu_item_new_with_label("Layout Topo");
    GtkWidget *menu = gtk_menu_new();
    if (!other)
      KOUT(" ");
    g_signal_connect_swapped(G_OBJECT(cmd_list), "activate",
                                           G_CALLBACK(cmd_cact), NULL);
    g_signal_connect_swapped(G_OBJECT(c2c_conf), "activate",
                                           G_CALLBACK(c2c_cact), NULL);
    g_signal_connect_swapped(G_OBJECT(del), "activate",
                                           G_CALLBACK(topo_delete), NULL);
    g_signal_connect_swapped(G_OBJECT(lst), "activate",
                                           G_CALLBACK(topo_lay), NULL);
    g_signal_connect_swapped (G_OBJECT(warnings), "activate",
                                           G_CALLBACK(show_old_warnings), NULL);
    g_signal_connect_swapped(G_OBJECT(hidden_visible), "activate",
                                           G_CALLBACK(hidden_visible_cb), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), cmd_list);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), c2c_conf);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), del);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), lst);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), warnings);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), hidden_visible);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), stop);
    stop_go_command(stop);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(other), menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), phy);
    phy_sub_menu(phy);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void update_topo_phy(int nb_phy, t_topo_info_phy *phy)
{
  memset(g_topo_phy, 0, MAX_PHY * sizeof(t_topo_info_phy));
  memcpy(g_topo_phy, phy, nb_phy * sizeof(t_topo_info_phy));
  g_nb_phy = nb_phy;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void update_topo_bridges(int nb_bridges, t_topo_bridges *bridges)
{
  memset(g_brgs, 0, MAX_OVS_BRIDGES * sizeof(t_topo_bridges));
  memcpy(g_brgs, bridges, nb_bridges * sizeof(t_topo_bridges));
  g_nb_brgs = nb_bridges;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void brandchoice_cb(GtkWidget *check, gpointer data)
{
  unsigned long uli = (unsigned long) data;
  if (g_right_click_inhib == 0)
    {
    if (uli == 1)
      {
      if (strcmp(g_brandtype_type, "brandkvm"))
        {
        memset(g_brandtype_type, 0, MAX_NAME_LEN);
        strncpy(g_brandtype_type, "brandkvm", MAX_NAME_LEN-1);
        update_brandtype_image();
        clownix_timeout_add(1, timeout_recall_menu, NULL, NULL, NULL);
        }
      }
    else if (uli == 2)
      {
      if (strcmp(g_brandtype_type, "brandcvm"))
        {
        memset(g_brandtype_type, 0, MAX_NAME_LEN);
        strncpy(g_brandtype_type, "brandcvm", MAX_NAME_LEN-1);
        update_brandtype_image();
        clownix_timeout_add(1, timeout_recall_menu, NULL, NULL, NULL);
        }
      }
    else if(uli == 3)
      {
      if (strcmp(g_brandtype_type, "brandzip"))
        {
        memset(g_brandtype_type, 0, MAX_NAME_LEN);
        strncpy(g_brandtype_type, "brandzip", MAX_NAME_LEN-1);
        update_brandtype_image();
        clownix_timeout_add(1, timeout_recall_menu, NULL, NULL, NULL);
        }
      }
    else
      KOUT("ERROR %lu", uli);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *get_brandtype_image(char *type)
{
  char *result = "none";
  if (type != NULL)
    {
    if (!strcmp(type, "brandkvm"))
      result = g_brandtype_image_kvm;
    else if (!strcmp(type, "brandzip"))
      result = g_brandtype_image_zip;
    else if (!strcmp(type, "brandcvm"))
      result = g_brandtype_image_cvm;
    else
      KERR("ERROR %s", type);
    }
  else if (!strcmp(g_brandtype_type, "brandkvm"))
    result = g_brandtype_image_kvm;
  else if (!strcmp(g_brandtype_type, "brandzip"))
    result = g_brandtype_image_zip;
  else if (!strcmp(g_brandtype_type, "brandcvm"))
    result = g_brandtype_image_cvm;
  else
    KERR("ERROR %s", g_brandtype_type);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void set_brandtype_image(char *type, char *image)
{

  if (!strlen(image))
    KERR("ERROR");
  else if (type != NULL)
    {
    if (!strcmp(type, "brandkvm"))
      {
      memset(g_brandtype_image_kvm, 0, MAX_NAME_LEN);
      strncpy(g_brandtype_image_kvm, image, MAX_NAME_LEN-1);
      }
    else if (!strcmp(type, "brandzip"))
      {
      memset(g_brandtype_image_zip, 0, MAX_NAME_LEN);
      strncpy(g_brandtype_image_zip, image, MAX_NAME_LEN-1);
      }
    else if (!strcmp(type, "brandcvm"))
      {
      memset(g_brandtype_image_cvm, 0, MAX_NAME_LEN);
      strncpy(g_brandtype_image_cvm, image, MAX_NAME_LEN-1);
      }
    else
      KERR("ERROR %s", type);
    }
  else
    {
    if (!strcmp(g_brandtype_type, "brandkvm"))
      {
      memset(g_brandtype_image_kvm, 0, MAX_NAME_LEN);
      strncpy(g_brandtype_image_kvm, image, MAX_NAME_LEN-1);
      }
    else if (!strcmp(g_brandtype_type, "brandzip"))
      {
      memset(g_brandtype_image_zip, 0, MAX_NAME_LEN);
      strncpy(g_brandtype_image_zip, image, MAX_NAME_LEN-1);
      }
    else if (!strcmp(g_brandtype_type, "brandcvm"))
      {
      memset(g_brandtype_image_cvm, 0, MAX_NAME_LEN);
      strncpy(g_brandtype_image_cvm, image, MAX_NAME_LEN-1);
      }
    else
      KERR("ERROR %s", g_brandtype_type);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *get_brandtype_type(void)
{
  return g_brandtype_type;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timeout_right_click_inhib(void *data)
{
  if (g_right_click_inhib <= 0)
    KERR("ERROR %d", g_right_click_inhib);
  else
    g_right_click_inhib -= 1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void set_right_click_inhib(void)
{
  clownix_timeout_add(45, timeout_right_click_inhib, NULL, NULL, NULL);
  g_right_click_inhib += 1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static gboolean onpress(GtkWidget *widget,
                        GdkEventButton *event,
                        gpointer data)
{ 
  if (event->type == GDK_BUTTON_PRESS  &&  event->button == 3)
    {
    set_right_click_inhib();
    }
  if (event->type == GDK_BUTTON_PRESS  &&  event->button == 1)
    {
    }
  return TRUE;
} 
/*--------------------------------------------------------------------------*/
  
/*****************************************************************************/
void set_bulkvm(int nb, t_slowperiodic *slowperiodic)
{   
  if (nb<0 || nb >= MAX_BULK_FILES)
    KOUT("%d", nb);
  g_nb_bulkvm = nb;
  memset(g_bulkvm, 0, MAX_BULK_FILES * sizeof(t_slowperiodic));
  memcpy(g_bulkvm, slowperiodic, g_nb_bulkvm * sizeof(t_slowperiodic));
}   
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void set_bulzip(int nb, t_slowperiodic *slowperiodic)
{
  if (nb<0 || nb >= MAX_BULK_FILES)
    KOUT("%d", nb);
  g_nb_bulzip = nb;
  memset(g_bulzip, 0, MAX_BULK_FILES * sizeof(t_slowperiodic));
  memcpy(g_bulzip, slowperiodic, g_nb_bulzip * sizeof(t_slowperiodic));
}
/*--------------------------------------------------------------------------*/
  
/*****************************************************************************/
void set_bulcvm(int nb, t_slowperiodic *slowperiodic)
{
  if (nb<0 || nb >= MAX_BULK_FILES)
    KOUT("%d", nb);     
  g_nb_bulcvm = nb;
  memset(g_bulcvm, 0, MAX_BULK_FILES * sizeof(t_slowperiodic));
  memcpy(g_bulcvm, slowperiodic, g_nb_bulcvm * sizeof(t_slowperiodic));
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void update_brandtype_image(void)
{ 
  int i, found;

  found = 0;
  for (i=0; i<g_nb_bulzip; i++)
    {
    if (!strcmp(get_brandtype_image("brandzip"), g_bulzip[i].name))
      found = 1;
    }
  if (found == 0)
    {
    if (g_nb_bulzip >= 1)
      set_brandtype_image("brandzip", g_bulzip[0].name);
    else
      set_brandtype_image("brandzip", "no-zip-found");
    }

  found = 0;
  for (i=0; i<g_nb_bulcvm; i++)
    {
    if (!strcmp(get_brandtype_image("brandcvm"), g_bulcvm[i].name))
      found = 1;
    }
  if (found == 0)
    {
    if (g_nb_bulcvm >= 1)
      set_brandtype_image("brandcvm", g_bulcvm[0].name);
    else
      set_brandtype_image("brandcvm", "no-cvm-found");
    }

  found = 0;
  for (i=0; i<g_nb_bulkvm; i++)
    {
    if (!strcmp(get_brandtype_image("brandkvm"), g_bulkvm[i].name))
      found = 1;
    }
  if (found == 0)
    {
    if (g_nb_bulkvm >= 1)
      set_brandtype_image("brandkvm", g_bulkvm[0].name);
    else
      set_brandtype_image("brandkvm", "no-kvm-found");
    }
}   
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void img_get(GtkWidget *check, gpointer data)
{
  char *name = (char *) data;
  if (g_inhib_timeout_recall_menu == 0)
    {
    if (strcmp(name, get_brandtype_image(NULL)))
      {
      clownix_timeout_add(1, timeout_recall_menu, NULL, NULL, NULL);
      }
    }
  set_brandtype_image(NULL, name);

}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static GtkWidget *get_bulk_choice(int nb_bul, t_slowperiodic *bul, char *image)
{
  int i, found = 0;
  GtkWidget *el0, *el, *menu = gtk_menu_new();
  GSList *group = NULL;
  gpointer data;
  if (nb_bul > 0)
    {
    for (i=0; i<nb_bul; i++)
      {
      el = gtk_radio_menu_item_new_with_label(group, bul[i].name);
      if (i == 0)
        {
        el0 = el;
        group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(el));
        }
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), el);
      data = (gpointer) bul[i].name;
      g_signal_connect(G_OBJECT(el),"activate",G_CALLBACK(img_get),data);
      if (!strcmp(image, bul[i].name))
        {
        found = 1;
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (el), TRUE);
        }
      }
    if (found == 0)
      {
      strncpy(image, bul[0].name, MAX_NAME_LEN-1);
      gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (el0), TRUE);
      }
    }
  return menu;
}
/*--------------------------------------------------------------------------*/
  
/*****************************************************************************/
static void setup_list_choices(GtkWidget *list)
{
  GtkWidget *menu = NULL;
  if (!strcmp(get_brandtype_type(), "brandzip"))
    menu = get_bulk_choice(g_nb_bulzip, g_bulzip, get_brandtype_image(NULL));
  else if (!strcmp(get_brandtype_type(), "brandcvm"))
    menu = get_bulk_choice(g_nb_bulcvm, g_bulcvm, get_brandtype_image(NULL));
  else if (!strcmp(get_brandtype_type(), "brandkvm"))
    menu = get_bulk_choice(g_nb_bulkvm, g_bulkvm, get_brandtype_image(NULL));
  else
    KOUT("ERROR %s", get_brandtype_type());
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(list), menu);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void local_canvas_ctx_menu(void)
{
  GSList *group = NULL;
  GtkWidget *menu = gtk_menu_new();
  GtkWidget *cnf = gtk_menu_item_new();
  GtkWidget *rad1;
  GtkWidget *rad2;
  GtkWidget *rad3;
  GtkWidget *separator1 = gtk_separator_menu_item_new();
  GtkWidget *separator2 = gtk_separator_menu_item_new();
  GtkWidget *separator3 = gtk_separator_menu_item_new();
  GtkWidget *lan  = gtk_menu_item_new_with_label("lan");
  GtkWidget *nat  = gtk_menu_item_new_with_label("nat");
  GtkWidget *tap  = gtk_menu_item_new_with_label("tap");
  GtkWidget *c2c  = gtk_menu_item_new_with_label("c2c");
  GtkWidget *a2b  = gtk_menu_item_new_with_label("a2b");
  GtkWidget *other= gtk_menu_item_new_with_label("Other");
  GtkWidget *list=  gtk_menu_item_new_with_label("List Other Choices");
  GtkWidget *zip_cvm_kvm = gtk_menu_item_new();
  char xvm_item_txt[MAX_NAME_LEN];
  char xvm_config_item_txt[MAX_NAME_LEN];
  update_brandtype_image();
  rad1 = gtk_radio_menu_item_new_with_label(group, "kvm");
  group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(rad1));
  rad2 = gtk_radio_menu_item_new_with_label(group, "cvm");
  rad3 = gtk_radio_menu_item_new_with_label(group, "zip");
  get_txt_for_zip_cvm_kvm(xvm_item_txt);
  gtk_menu_item_set_label(GTK_MENU_ITEM(zip_cvm_kvm), xvm_item_txt);
  get_txt_for_cnf(xvm_config_item_txt);
  gtk_menu_item_set_label(GTK_MENU_ITEM(cnf), xvm_config_item_txt);
  g_signal_connect_swapped(G_OBJECT(zip_cvm_kvm), "activate",
                           G_CALLBACK(xvm_act), NULL);
  g_signal_connect_swapped(G_OBJECT(cnf), "activate",
                           G_CALLBACK(cnf_act), NULL);
  if (!strcmp(g_brandtype_type, "brandkvm"))
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(rad1), TRUE);
  else if (!strcmp(g_brandtype_type, "brandcvm"))
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(rad2), TRUE);
  else if (!strcmp(g_brandtype_type, "brandzip"))
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(rad3), TRUE);
  else
    KERR("ERROR %s", g_brandtype_type);
  g_signal_connect(G_OBJECT(rad1),"activate",
                   G_CALLBACK(brandchoice_cb), (gpointer) 1);
  g_signal_connect(G_OBJECT(rad2),"activate",
                   G_CALLBACK(brandchoice_cb), (gpointer) 2);
  g_signal_connect(G_OBJECT(rad3),"activate",
                   G_CALLBACK(brandchoice_cb), (gpointer) 3);
  g_signal_connect_swapped(G_OBJECT(lan), "activate",
                          G_CALLBACK(lan_act), NULL);
  g_signal_connect_swapped(G_OBJECT(tap), "activate",
                          G_CALLBACK(tap_act), NULL);
  g_signal_connect_swapped(G_OBJECT(c2c), "activate",
                          G_CALLBACK(c2c_act), NULL);
  g_signal_connect_swapped(G_OBJECT(a2b), "activate",
                          G_CALLBACK(a2b_act), NULL);
  g_signal_connect_swapped(G_OBJECT(nat), "activate",
                          G_CALLBACK(nat_act), NULL);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), zip_cvm_kvm);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), list);
  g_inhib_timeout_recall_menu = 1;
  setup_list_choices(list);
  g_inhib_timeout_recall_menu = 0;
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), cnf);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), rad1);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), rad2);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), rad3);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator1);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), lan);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator2);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), nat);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), tap);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), c2c);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), a2b);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator3);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), other);
  other_sub_menu(other);
  gtk_widget_show_all(menu);
  g_signal_connect(G_OBJECT(menu), "button-press-event",
                   G_CALLBACK(onpress), NULL);
  set_right_click_inhib();
  gtk_menu_popup_at_widget(GTK_MENU(menu), get_gtkwidget_canvas(),
  GDK_GRAVITY_NORTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void canvas_ctx_menu(gdouble x, gdouble y)
{
  g_x_mouse = (double) x;
  g_y_mouse = (double) y;
  local_canvas_ctx_menu();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void canvas_ctx_init(void)
{
  g_inhib_timeout_recall_menu = 0;
  g_right_click_inhib = 0;
  memset(g_brandtype_type, 0, MAX_NAME_LEN);
  memset(g_brandtype_image_kvm, 0, MAX_NAME_LEN);
  memset(g_brandtype_image_zip, 0, MAX_NAME_LEN);
  memset(g_brandtype_image_cvm, 0, MAX_NAME_LEN);
  strncpy(g_brandtype_type, "brandcvm", MAX_NAME_LEN-1);
  strncpy(g_brandtype_image_kvm, "bookworm.qcow2", MAX_NAME_LEN-1);
  strncpy(g_brandtype_image_zip, "zipfrr.zip", MAX_NAME_LEN-1);
  strncpy(g_brandtype_image_cvm, "bookworm0", MAX_NAME_LEN-1);
  memset(g_bulkvm, 0, MAX_BULK_FILES * sizeof(t_slowperiodic));
  memset(g_bulzip, 0, MAX_BULK_FILES * sizeof(t_slowperiodic));
  memset(g_bulcvm, 0, MAX_BULK_FILES * sizeof(t_slowperiodic));
  g_nb_bulkvm = 0;
  g_nb_bulzip = 0;
  g_nb_bulcvm = 0;

}
/*--------------------------------------------------------------------------*/





