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
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <libcrcanvas.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
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
#include "menu_dialog_c2c.h"
#include "menu_dialog_a2b.h"
#include "menu_dialog_lan.h"
#include "menu_dialog_tap.h"
#include "menu_dialog_snf.h"
#include "layout_rpc.h"
#include "layout_topo.h"
#include "cloonix_conf_info.h"



static double g_x_mouse, g_y_mouse;
GtkWidget *get_main_window(void);

void topo_get_matrix_inv_transform_point(double *x, double *y);
void put_top_left_icon(GtkWidget *mainwin);

/****************************************************************************/
static void call_cloonix_interface_lan_create(double x, double y)
{
  double x0=x, y0=y;
  t_custom_lan *c = get_custom_lan();
  topo_get_matrix_inv_transform_point(&x0, &y0);
  to_cloonix_switch_create_lan(c->name, x0, y0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void call_cloonix_interface_tap_create(double x, double y)
{
  double x0=x, y0=y;
  t_custom_tap *c = get_custom_tap();
  topo_get_matrix_inv_transform_point(&x0, &y0);
  to_cloonix_switch_create_sat(c->name, c->mutype, NULL, x0, y0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void call_cloonix_interface_snf_create(double x, double y)
{
  double x0=x, y0=y;
  t_custom_snf *c = get_custom_snf();
  topo_get_matrix_inv_transform_point(&x0, &y0);
  to_cloonix_switch_create_sat(c->name, endp_type_snf,NULL,x0,y0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void call_cloonix_interface_nat_create(double x, double y)
{
  double x0=x, y0=y;
  topo_get_matrix_inv_transform_point(&x0, &y0);
  to_cloonix_switch_create_sat("nat", endp_type_nat, NULL, x0, y0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void call_cloonix_interface_a2b_create(double x, double y)
{
  double x0=x, y0=y;
  t_custom_a2b *c = get_custom_a2b();
  topo_get_matrix_inv_transform_point(&x0, &y0);
  to_cloonix_switch_create_sat(c->name, endp_type_a2b, NULL, x0, y0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void call_cloonix_interface_c2c_create(double x, double y)
{
  t_custom_c2c *c2c = get_custom_c2c();
  double x0=x, y0=y;
  t_cloonix_conf_info *cnf;
  cnf = cloonix_conf_info_get(c2c->c2c_req_info.cloonix_slave);
  if (!cnf)
    insert_next_warning(cloonix_conf_info_get_names(), 1);
  else
    {
    c2c->c2c_req_info.ip_slave = cnf->ip;
    c2c->c2c_req_info.port_slave = cnf->port;
    strncpy(c2c->c2c_req_info.passwd_slave, cnf->passwd, MSG_DIGEST_LEN-1);
    topo_get_matrix_inv_transform_point(&x0, &y0);
    to_cloonix_switch_create_sat(c2c->name, endp_type_c2c, 
                                 &(c2c->c2c_req_info), x0, y0);
    }
}
/*--------------------------------------------------------------------------*/



/****************************************************************************/
static void call_cloonix_interface_node_create(double x, double y)
{
  int rest, i;
  double tx[MAX_ETH_VM+MAX_WLAN_VM];
  double ty[MAX_ETH_VM+MAX_WLAN_VM];
  double x0=x, y0=y;
  memset(tx, 0, (MAX_ETH_VM+MAX_WLAN_VM) * sizeof(double));
  memset(ty, 0, (MAX_ETH_VM+MAX_WLAN_VM) * sizeof(double));
  for (i=0; i<MAX_ETH_VM+MAX_WLAN_VM; i++)
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
  to_cloonix_switch_create_node(x0, y0, tx, ty);
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
  if (bi->bank_type == bank_type_node) 
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
static void hidden_visible_cb(GtkWidget *mn)
{
  int is_eth;
  t_bank_item *prev = NULL;
  t_bank_item *cur;
  GtkWidget *dialog, *check;
  GtkWidget *parent = get_main_window();
  if (mn)
    KOUT(" ");
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
static void topo_list(GtkWidget *mn)
{
  client_list_commands(0, callback_list_commands);
}
/*--------------------------------------------------------------------------*/
 
/****************************************************************************/
static void show_old_warnings(GtkWidget *mn)
{
  if (mn)
    KOUT(" ");
  stored_warning_trace_textview_create();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void lan_act(GtkWidget *mn)
{
  (void) mn;
  call_cloonix_interface_lan_create(g_x_mouse, g_y_mouse);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void tap_act(GtkWidget *mn)
{
  (void) mn;
  call_cloonix_interface_tap_create(g_x_mouse, g_y_mouse);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void kvm_act(GtkWidget *mn)
{
  (void) mn;
  call_cloonix_interface_node_create(g_x_mouse, g_y_mouse);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void snf_act(GtkWidget *mn)
{
  (void) mn;
  call_cloonix_interface_snf_create(g_x_mouse, g_y_mouse);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void c2c_act(GtkWidget *mn)
{
  (void) mn;
  call_cloonix_interface_c2c_create(g_x_mouse, g_y_mouse);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void nat_act(GtkWidget *mn)
{
  (void) mn;
  call_cloonix_interface_nat_create(g_x_mouse, g_y_mouse);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void a2b_act(GtkWidget *mn)
{
  (void) mn;
  call_cloonix_interface_a2b_create(g_x_mouse, g_y_mouse);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void kvm_cact(GtkWidget *mn)
{
  (void) mn;
  menu_choice_kvm();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void lan_cact(GtkWidget *mn)
{
  (void) mn;
  menu_choice_lan_params();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void tap_cact(GtkWidget *mn)
{
  (void) mn;
  menu_choice_tap_params();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void snf_cact(GtkWidget *mn)
{
  (void) mn;
  menu_choice_snf_params();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void a2b_cact(GtkWidget *mn)
{
  (void) mn;
  menu_choice_a2b_params();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void c2c_cact(GtkWidget *mn)
{
  (void) mn;
  menu_choice_c2c_params();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void canvas_ctx_menu(gdouble x, gdouble y)
{
  GtkWidget *menu = gtk_menu_new();
  GtkWidget *del = gtk_menu_item_new_with_label("Delete Topo");
  GtkWidget *lst = gtk_menu_item_new_with_label("List cmds Topo");


  GtkWidget *lan = gtk_menu_item_new_with_label("lan");
  GtkWidget *kvm = gtk_menu_item_new_with_label("kvm");
  GtkWidget *tap = gtk_menu_item_new_with_label("tap");
  GtkWidget *snf = gtk_menu_item_new_with_label("snf");
  GtkWidget *c2c = gtk_menu_item_new_with_label("c2c");
  GtkWidget *nat = gtk_menu_item_new_with_label("nat");
  GtkWidget *a2b = gtk_menu_item_new_with_label("a2b");

  GtkWidget *lan_conf = gtk_menu_item_new_with_label("lan_conf");
  GtkWidget *kvm_conf = gtk_menu_item_new_with_label("kvm_conf");
  GtkWidget *tap_conf = gtk_menu_item_new_with_label("tap_conf");
  GtkWidget *snf_conf = gtk_menu_item_new_with_label("snf_conf");
  GtkWidget *c2c_conf = gtk_menu_item_new_with_label("c2c_conf");
  GtkWidget *a2b_conf = gtk_menu_item_new_with_label("a2b_conf");

  GtkWidget *stop = gtk_menu_item_new_with_label("Motion");
  GtkWidget *warnings = gtk_menu_item_new_with_label("Previous Warnings");
  GtkWidget *hidden_visible = gtk_menu_item_new_with_label("Hidden/Visible");
  GtkWidget *separator1 = gtk_separator_menu_item_new();
  GtkWidget *separator2 = gtk_separator_menu_item_new();

  g_x_mouse = (double) x;
  g_y_mouse = (double) y;

  g_signal_connect_swapped(G_OBJECT(del), "activate",
                               (GCallback)topo_delete, NULL);
  g_signal_connect_swapped(G_OBJECT(lst), "activate",
                               (GCallback)topo_list, NULL);

  g_signal_connect_swapped(G_OBJECT(kvm), "activate",
                          (GCallback)kvm_act, NULL);
  g_signal_connect_swapped(G_OBJECT(lan), "activate",
                          (GCallback)lan_act, NULL);
  g_signal_connect_swapped(G_OBJECT(tap), "activate",
                          (GCallback)tap_act, NULL);
  g_signal_connect_swapped(G_OBJECT(snf), "activate",
                          (GCallback)snf_act, NULL);
  g_signal_connect_swapped(G_OBJECT(c2c), "activate",
                          (GCallback)c2c_act, NULL);
  g_signal_connect_swapped(G_OBJECT(nat), "activate",
                          (GCallback)nat_act, NULL);
  g_signal_connect_swapped(G_OBJECT(a2b), "activate",
                          (GCallback)a2b_act, NULL);
  g_signal_connect_swapped(G_OBJECT(lan_conf), "activate",
                          (GCallback)lan_cact, NULL);
  g_signal_connect_swapped(G_OBJECT(kvm_conf), "activate",
                          (GCallback)kvm_cact, NULL);
  g_signal_connect_swapped(G_OBJECT(tap_conf), "activate",
                          (GCallback)tap_cact, NULL);
  g_signal_connect_swapped(G_OBJECT(snf_conf), "activate",
                          (GCallback)snf_cact, NULL);
  g_signal_connect_swapped(G_OBJECT(c2c_conf), "activate",
                          (GCallback)c2c_cact, NULL);
  g_signal_connect_swapped(G_OBJECT(a2b_conf), "activate",
                          (GCallback)a2b_cact, NULL);
  g_signal_connect_swapped (G_OBJECT(warnings), "activate",
                               (GCallback)show_old_warnings, NULL);
  g_signal_connect_swapped(G_OBJECT(hidden_visible), "activate",
                               (GCallback)hidden_visible_cb, NULL);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), lan);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), kvm);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), tap);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), snf);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), c2c);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), nat);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), a2b);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator1);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), lan_conf);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), tap_conf);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), snf_conf);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), kvm_conf);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), c2c_conf);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), a2b_conf);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator2);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), del);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), lst);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), warnings);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), hidden_visible);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), stop);
  stop_go_command(stop);
  gtk_widget_show_all(menu);
  gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL);
}
/*--------------------------------------------------------------------------*/






