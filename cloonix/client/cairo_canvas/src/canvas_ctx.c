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



static double g_x_mouse, g_y_mouse;
GtkWidget *get_main_window(void);

t_cloonix_conf_info *get_own_cloonix_conf_info(void);

void topo_get_matrix_inv_transform_point(double *x, double *y);
void put_top_left_icon(GtkWidget *mainwin);

static t_topo_info_phy g_topo_phy[MAX_PHY];
static int g_nb_phy;

static int g_nb_brgs;
static t_topo_bridges g_brgs[MAX_OVS_BRIDGES];

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
  snprintf(net_name, 8, "%s", local_get_cloonix_name());
  net_name[7] = 0;
  snprintf(name, IFNAMSIZ-1, "%stap%d", net_name, num++);
  name[IFNAMSIZ-1] = 0;
  topo_get_matrix_inv_transform_point(&x0, &y0);
  to_cloonix_switch_create_sat(name, endp_type_tapv, NULL, x0, y0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
/*
static void call_cloonix_interface_phy_create(char *name, double x, double y)
{
  double x0=x, y0=y;
  topo_get_matrix_inv_transform_point(&x0, &y0);
  to_cloonix_switch_create_sat(name, endp_type_phy, NULL, x0, y0);
}
*/
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
    snprintf(label, MAX_PATH_LEN, bi->name);
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
                     (GCallback)rad_stop_cb, (gpointer) 1);
  g_signal_connect(G_OBJECT(rad2),"activate",
                     (GCallback)rad_stop_cb, (gpointer) 0);
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
static void lan_act(void)
{
  call_cloonix_interface_lan_create(g_x_mouse, g_y_mouse);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void tap_act(void)
{
  call_cloonix_interface_tap_create(g_x_mouse, g_y_mouse);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void kvm_act(void)
{
  call_cloonix_interface_node_create(g_x_mouse, g_y_mouse);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void cnt_act(void)
{
  call_cloonix_interface_node_cnt_create(g_x_mouse, g_y_mouse);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void c2c_act(void)
{
  call_cloonix_interface_c2c_create(g_x_mouse, g_y_mouse);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void a2b_act(void)
{
  call_cloonix_interface_a2b_create(g_x_mouse, g_y_mouse);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void nat_act(void)
{
  call_cloonix_interface_nat_create(g_x_mouse, g_y_mouse);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void kvm_cact(void)
{
  menu_choice_kvm();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void cnt_cact(void)
{
  menu_choice_cnt();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void c2c_cact(void)
{
  menu_choice_c2c_params();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
/*
static void cphy(gpointer data)
{
  unsigned long i = (unsigned long) data;
  call_cloonix_interface_phy_create(g_topo_phy[i].name, g_x_mouse, g_y_mouse);
}
*/
/*--------------------------------------------------------------------------*/

/****************************************************************************/
/*
static void phy_sub_menu(GtkWidget *phy)
{
  unsigned long i;
  GtkWidget *menu = gtk_menu_new();
  GtkWidget *item;
  if (!phy)
    KOUT(" ");
  for (i=0; i<g_nb_phy; i++)
    {
    item = gtk_menu_item_new_with_label(g_topo_phy[i].name);
    g_signal_connect_swapped(G_OBJECT(item), "activate",
                             (GCallback) cphy, (gpointer) i);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    }
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(phy), menu);
}
*/
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void other_sub_menu(GtkWidget *other)
{
  GtkWidget *c2c_conf = gtk_menu_item_new_with_label("c2c_conf");
  GtkWidget *stop = gtk_menu_item_new_with_label("Motion");
  GtkWidget *warnings = gtk_menu_item_new_with_label("Previous Warnings");
  GtkWidget *hidden_visible = gtk_menu_item_new_with_label("Hidden/Visible");
  GtkWidget *del = gtk_menu_item_new_with_label("Delete Topo");
  GtkWidget *lst = gtk_menu_item_new_with_label("Layout Topo");
  GtkWidget *menu = gtk_menu_new();
  if (!other)
    KOUT(" ");
  g_signal_connect_swapped(G_OBJECT(c2c_conf), "activate",
                                         (GCallback)c2c_cact, NULL);
  g_signal_connect_swapped(G_OBJECT(del), "activate",
                                         (GCallback)topo_delete, NULL);
  g_signal_connect_swapped(G_OBJECT(lst), "activate",
                                         (GCallback)topo_lay, NULL);
  g_signal_connect_swapped (G_OBJECT(warnings), "activate",
                                         (GCallback)show_old_warnings, NULL);
  g_signal_connect_swapped(G_OBJECT(hidden_visible), "activate",
                                         (GCallback)hidden_visible_cb, NULL);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), c2c_conf);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), del);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), lst);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), warnings);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), hidden_visible);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), stop);
  stop_go_command(stop);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(other), menu);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int format_phy_info(char *name, char *txt)
{
  int i, found = 0, ln = 0;
  int flags;
  for (i=0; i<g_nb_phy; i++)
    {
    if (!strcmp(name, g_topo_phy[i].name))
      {
      found = 1;
      flags = g_topo_phy[i].flags;
      ln += sprintf(txt+ln, "Name:  %s\n", g_topo_phy[i].name);
      ln += sprintf(txt+ln, "index: %d\n", g_topo_phy[i].index);
      ln += sprintf(txt+ln, "drv:   %s\n", g_topo_phy[i].drv);
      ln += sprintf(txt+ln, "pci:   %s\n", g_topo_phy[i].pci);
      ln += sprintf(txt+ln, "mac:   %s\n", g_topo_phy[i].mac);
      ln += sprintf(txt+ln, "vendor:   %s\n", g_topo_phy[i].vendor);
      ln += sprintf(txt+ln, "device:   %s\n", g_topo_phy[i].device);
      ln += sprintf(txt+ln, "flags: 0x%X\n", flags);
      if (flags & IFF_UP)
        ln += sprintf(txt+ln, "UP ");
      if (flags & IFF_RUNNING)
        ln += sprintf(txt+ln, "RUNNING ");
      if (flags & IFF_PROMISC)
        ln += sprintf(txt+ln, "PROMISC ");
      }
    }
  if (found == 0)
    ln += sprintf(txt, "Data not found");
  return ln;
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
void canvas_ctx_menu(gdouble x, gdouble y)
{
  GtkWidget *menu = gtk_menu_new();
  GtkWidget *separator1 = gtk_separator_menu_item_new();
  GtkWidget *lan  = gtk_menu_item_new_with_label("lan");
  GtkWidget *kvm  = gtk_menu_item_new_with_label("kvm");
  GtkWidget *cnt  = gtk_menu_item_new_with_label("cnt");
  GtkWidget *nat  = gtk_menu_item_new_with_label("nat");
  GtkWidget *tap  = gtk_menu_item_new_with_label("tap");
  GtkWidget *c2c  = gtk_menu_item_new_with_label("c2c");
  GtkWidget *a2b  = gtk_menu_item_new_with_label("a2b");
//  GtkWidget *phy  = gtk_menu_item_new_with_label("phy");
  GtkWidget *cmd  = gtk_menu_item_new_with_label("cmd_list");
  GtkWidget *kvm_conf = gtk_menu_item_new_with_label("Kvm_conf");
  GtkWidget *cnt_conf = gtk_menu_item_new_with_label("Cnt_conf");
  GtkWidget *other= gtk_menu_item_new_with_label("Other");
  g_x_mouse = (double) x;
  g_y_mouse = (double) y;
  g_signal_connect_swapped(G_OBJECT(kvm), "activate",
                          (GCallback)kvm_act, NULL);
  g_signal_connect_swapped(G_OBJECT(cnt), "activate",
                          (GCallback)cnt_act, NULL);
  g_signal_connect_swapped(G_OBJECT(lan), "activate",
                          (GCallback)lan_act, NULL);
  g_signal_connect_swapped(G_OBJECT(tap), "activate",
                          (GCallback)tap_act, NULL);
  g_signal_connect_swapped(G_OBJECT(c2c), "activate",
                          (GCallback)c2c_act, NULL);
  g_signal_connect_swapped(G_OBJECT(a2b), "activate",
                          (GCallback)a2b_act, NULL);
  g_signal_connect_swapped(G_OBJECT(nat), "activate",
                          (GCallback)nat_act, NULL);
  g_signal_connect_swapped(G_OBJECT(kvm_conf), "activate",
                          (GCallback)kvm_cact, NULL);
  g_signal_connect_swapped(G_OBJECT(cnt_conf), "activate",
                          (GCallback)cnt_cact, NULL);
  g_signal_connect_swapped(G_OBJECT(cmd), "activate",
                          (GCallback)cmd_cact, NULL);

  gtk_menu_shell_append(GTK_MENU_SHELL(menu), lan);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), kvm);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), cnt);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), nat);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), tap);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), c2c);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), a2b);
//  gtk_menu_shell_append(GTK_MENU_SHELL(menu), phy);
//  phy_sub_menu(phy);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), kvm_conf);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), cnt_conf);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator1);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), cmd);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), other);
  other_sub_menu(other);
  gtk_widget_show_all(menu);
  gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL);
}
/*--------------------------------------------------------------------------*/






