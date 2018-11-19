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
#include "menu_dialog_lan.h"
#include "menu_dialog_tap.h"
#include "menu_dialog_snf.h"
#include "menu_dialog_c2c.h"
#include "menu_dialog_a2b.h"
#include "bdplot.h"


GtkWidget *get_main_window(void);

static char g_sav_whole[MAX_PATH_LEN];
static char g_sav_derived[MAX_PATH_LEN];

/****************************************************************************/
void call_cloonix_interface_edge_delete(t_bank_item *bitem)
{
  t_bank_item *intf = bitem->att_eth;
  t_bank_item *lan = bitem->att_lan;
  if (lan->bank_type == bank_type_lan)
    {
    if (intf->bank_type == bank_type_eth) 
      to_cloonix_switch_delete_edge(intf->name, intf->num, lan->name);
    else if (intf->bank_type == bank_type_sat) 
      to_cloonix_switch_delete_edge(intf->name, intf->num, lan->name);
    else
      KOUT(" ");
    }
  else
    KOUT(" ");
}
/*--------------------------------------------------------------------------*/



/****************************************************************************/
static void display_info(char *title, char *text)
{
  GtkWidget *window, *scrolled_win, *textview;
  GtkTextIter iter;
  GtkTextBuffer *buffer;
  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_modal (GTK_WINDOW(window), FALSE);
  gtk_window_set_title(GTK_WINDOW (window), title);
  gtk_container_set_border_width(GTK_CONTAINER(window), 0);
  gtk_window_set_default_size(GTK_WINDOW(window), 300, 200);
  scrolled_win = gtk_scrolled_window_new(NULL, NULL);
  textview = gtk_text_view_new();
  buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
  gtk_text_buffer_get_end_iter(buffer, &iter);
  gtk_text_buffer_insert(buffer, &iter, "\n", -1);
  gtk_text_buffer_get_end_iter(buffer, &iter);
  gtk_text_buffer_insert(buffer, &iter, text, -1);
  gtk_text_buffer_get_end_iter(buffer, &iter);
  gtk_text_buffer_insert(buffer, &iter, "\n", -1);
  gtk_container_add(GTK_CONTAINER(scrolled_win), textview);
  gtk_container_add(GTK_CONTAINER(window), scrolled_win);
  gtk_widget_show_all (window);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void menu_hidden(GtkWidget *mn, t_item_ident *pm)
{
  t_bank_item *bitem;
  bitem = bank_get_item(pm->bank_type, pm->name, pm->num, pm->lan);
  if (bitem)
    {
    bitem->pbi.menu_on = 0;
    leave_item_surface_action(bitem);
    }
  differed_clownix_free((void *) pm);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void end_cb_node_reboot(int tid, int status, char *err)
{
  if (status)
    insert_next_warning(err, 1);
  else
    insert_next_warning("SEND REBOOT VM OK", 1);

}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void end_cb_node_sav_rootfs(int tid, int status, char *err)
{
  if (status)
    insert_next_warning(err, 1);
  else
    insert_next_warning("END SAVING OF VM OK", 1);

}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void node_sav_rootfs(GtkWidget *mn, t_item_ident *pm)
{
  int response, save_whole;
  char *tmp;
  char name[MAX_NAME_LEN];
  GtkWidget *entry_rootfs, *parent, *dialog;
  save_whole = pm->joker_param;
  memset(name, 0, MAX_NAME_LEN);
  strncpy(name, pm->name, MAX_NAME_LEN-1);
  parent = get_main_window();
  dialog = gtk_dialog_new_with_buttons ("Path of file to save",
                                        GTK_WINDOW (parent),
                                        GTK_DIALOG_MODAL,
                                        "OK",GTK_RESPONSE_ACCEPT,
                                        NULL);
  gtk_window_set_default_size(GTK_WINDOW(dialog), 300, 20);
  entry_rootfs = gtk_entry_new();
  if (save_whole)
    gtk_entry_set_text(GTK_ENTRY(entry_rootfs), g_sav_whole);
  else
    gtk_entry_set_text(GTK_ENTRY(entry_rootfs), g_sav_derived);
  gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                     entry_rootfs, TRUE, TRUE, 0);
  gtk_widget_show_all(dialog);
  response = gtk_dialog_run(GTK_DIALOG (dialog));
  if (response == GTK_RESPONSE_ACCEPT)
    {
    tmp = (char *) gtk_entry_get_text(GTK_ENTRY(entry_rootfs));
    if (save_whole)
      {
      memset(g_sav_whole, 0, MAX_PATH_LEN);
      strncpy(g_sav_whole, tmp, MAX_PATH_LEN-1);
      }
    else
      {
      memset(g_sav_derived, 0, MAX_PATH_LEN);
      strncpy(g_sav_derived, tmp, MAX_PATH_LEN-1);
      }
    if (tmp && strlen(tmp))
      {
      client_sav_vm(0, end_cb_node_sav_rootfs, name, save_whole, tmp);
      }
    }
  gtk_widget_destroy(dialog);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void node_qreboot_vm(GtkWidget *mn, t_item_ident *pm)
{
  client_reboot_vm(0, end_cb_node_reboot, pm->name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void node_sav_whole(GtkWidget *mn, t_item_ident *pm)
{
  pm->joker_param = 1;
  node_sav_rootfs(mn, pm);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void node_sav_derived(GtkWidget *mn, t_item_ident *pm)
{
  pm->joker_param = 0;
  node_sav_rootfs(mn, pm);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void node_item_info(GtkWidget *mn, t_item_ident *pm)
{
  t_bank_item *bitem;
  static char title[MAX_TITLE];
  static char text[MAX_TEXT];
  int is_persistent, is_backed, is_inside_cloonix;
  int has_p9_host_share, is_cisco, has_vhost_vsock, is_uefi; 
  int vm_config_flags, has_install_cdrom, has_added_cdrom;
  int is_full_virt, has_no_reboot, has_added_disk, len = 0;
  bitem = look_for_node_with_id(pm->name);
  if (bitem)
    {
    sprintf(title, "%s", bitem->name);
    if (bitem->bank_type == bank_type_node)
      len += sprintf(text + len, "\t\tQEMU_KVM");

    vm_config_flags = bitem->pbi.pbi_node->node_vm_config_flags;
    is_persistent = vm_config_flags & VM_CONFIG_FLAG_PERSISTENT;
    is_full_virt  = vm_config_flags & VM_CONFIG_FLAG_FULL_VIRT;
    has_vhost_vsock = vm_config_flags & VM_CONFIG_FLAG_VHOST_VSOCK;
    is_uefi = vm_config_flags & VM_CONFIG_FLAG_UEFI;
    is_backed   = vm_config_flags & VM_FLAG_DERIVED_BACKING;
    is_inside_cloonix = vm_config_flags & VM_FLAG_IS_INSIDE_CLOONIX;
    has_install_cdrom = vm_config_flags & VM_CONFIG_FLAG_INSTALL_CDROM;
    has_no_reboot = vm_config_flags & VM_CONFIG_FLAG_NO_REBOOT;
    has_added_cdrom = vm_config_flags & VM_CONFIG_FLAG_ADDED_CDROM;
    has_added_disk = vm_config_flags & VM_CONFIG_FLAG_ADDED_DISK;
    is_cisco = vm_config_flags & VM_CONFIG_FLAG_CISCO;
    has_p9_host_share = vm_config_flags & VM_CONFIG_FLAG_9P_SHARED;

    if (is_cisco)
      len += sprintf(text + len, "\n\t\tCISCO");
    if (has_p9_host_share)
      len += sprintf(text + len, "\n\t\tP9_HOST_SHARE");
    if (is_persistent)
      len += sprintf(text + len, "\n\t\tPERSISTENT");
    else
      len += sprintf(text + len, "\n\t\tEVANESCENT");
    if (is_full_virt)
      len += sprintf(text + len, "\n\t\tFULL VIRT");
    if (has_vhost_vsock)
      len += sprintf(text + len, "\n\t\tVHOST_VSOCK");
    if (is_uefi)
      len += sprintf(text + len, "\n\t\tUEFI");
    if (is_backed)
      {
      len += sprintf(text + len, "\n\t\tDERIVED FROM BACKING");
      len += sprintf(text + len, "\nDerived: %s", 
                     bitem->pbi.pbi_node->node_rootfs_sod);
      len += sprintf(text + len, "\nBacking: %s", 
                     bitem->pbi.pbi_node->node_rootfs_backing_file);
      }
    else
      {
      len += sprintf(text + len, "\n\t\tSOLO ROOTFS");
      len += sprintf(text + len, "\nRootfs: %s", 
                     bitem->pbi.pbi_node->node_rootfs_sod);
      }
    if (is_inside_cloonix)
      len += sprintf(text + len, "\n\t\tIS INSIDE CLOONIX");
    if (has_install_cdrom)
      len += sprintf(text + len, "\n INSTALL_CDROM: %s",
                     bitem->pbi.pbi_node->install_cdrom);
    if (has_no_reboot)
      len += sprintf(text + len, "\n NO_REBOOT");
    if (has_added_cdrom)
      len += sprintf(text + len, "\n ADDED_CDROM: %s",
                     bitem->pbi.pbi_node->added_cdrom);
    if (has_added_disk)
      len += sprintf(text + len, "\n ADDED_DISK: %s",
                     bitem->pbi.pbi_node->added_disk);


    display_info(title, text);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int c2c_item_info(char *text,  t_bank_item *bitem)
{
  int port_slave, len = 0;
  char ip_slave[MAX_NAME_LEN];
    int_to_ip_string (bitem->pbi.pbi_sat->topo_c2c.ip_slave, ip_slave);
    port_slave = bitem->pbi.pbi_sat->topo_c2c.port_slave;
    len += sprintf(text + len,"\nMaster: %s",bitem->pbi.pbi_sat->topo_c2c.master_cloonix);
    len += sprintf(text + len,"\nSlave: %s",bitem->pbi.pbi_sat->topo_c2c.slave_cloonix);
    len += sprintf(text + len,"\nip_slave: %s port_slave: %d", 
                              ip_slave, port_slave);
    if (bitem->pbi.pbi_sat->topo_c2c.is_peered)
      len += sprintf(text + len, "\n CONNECTION OK");
    else
      len += sprintf(text + len, "\n CONNECTION KO");
  return len;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void sat_item_info(GtkWidget *mn, t_item_ident *pm)
{
  t_bank_item *bitem;
  char title[2*MAX_NAME_LEN];
  char text[4*MAX_PATH_LEN];
  int len = 0;
  bitem = look_for_sat_with_id(pm->name);
  if (bitem)
    {
    sprintf(title, "%s", bitem->name);
    if (bitem->pbi.mutype == endp_type_tap)
      len += sprintf(text + len, "\nTAP");
    else if (bitem->pbi.mutype == endp_type_snf)
      len += sprintf(text + len, "\nSNF");
    else if (bitem->pbi.mutype == endp_type_nat)
      len += sprintf(text + len, "\nNAT");
    else if (bitem->pbi.mutype == endp_type_wif)
      len += sprintf(text + len, "\nWIF");
    else if (bitem->pbi.mutype == endp_type_c2c)
      len += sprintf(text + len, "\nC2C");
    else if (bitem->pbi.mutype == endp_type_a2b)
      len += sprintf(text + len, "\nA2B");

    if (is_a_c2c(bitem))
      len += c2c_item_info(text + len, bitem);
    display_info(title, text);
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void rad_node_color_cb(t_item_ident *pm, int choice)
{
  t_bank_item *bitem;
  bitem = look_for_node_with_id(pm->name);
  if (bitem)
    bitem->pbi.color_choice = choice;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rad_node_color_0_cb(GtkWidget *check, gpointer data)
{
  t_item_ident *pm = (t_item_ident *) data;
  if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(check)))
    rad_node_color_cb(pm, 0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rad_node_color_1_cb(GtkWidget *check, gpointer data)
{
  t_item_ident *pm = (t_item_ident *) data;
  if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(check)))
    rad_node_color_cb(pm, 1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rad_node_color_2_cb(GtkWidget *check, gpointer data)
{
  t_item_ident *pm = (t_item_ident *) data;
  if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(check)))
    rad_node_color_cb(pm, 2);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void node_item_color(GtkWidget *mn, t_item_ident *pm)
{
  GtkWidget *menu, *rad0, *rad1, *rad2;
  GSList *group = NULL;
  t_bank_item *bitem;
  rad0 = gtk_radio_menu_item_new_with_label(group, "blue");
  group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(rad0));
  rad1 = gtk_radio_menu_item_new_with_label(group, "cyan");
  group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(rad1));
  rad2 = gtk_radio_menu_item_new_with_label(group, "brown");
  menu = gtk_menu_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), rad0);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), rad1);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), rad2);
  g_signal_connect(G_OBJECT(rad0),"activate",
                     (GCallback) rad_node_color_0_cb, (gpointer) pm);
  g_signal_connect(G_OBJECT(rad1),"activate",
                     (GCallback)rad_node_color_1_cb, (gpointer) pm);
  g_signal_connect(G_OBJECT(rad2),"activate",
                     (GCallback)rad_node_color_2_cb, (gpointer) pm);
  bitem = look_for_node_with_id(pm->name);
  if (bitem)
    {
    if (bitem->pbi.color_choice == 0)
      {
      gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(rad0), TRUE);
      }
    else if (bitem->pbi.color_choice == 1)
      {
      gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(rad1), TRUE);
      }
    else if (bitem->pbi.color_choice == 2)
      {
      gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(rad2), TRUE);
      }
    else
      KOUT("%p", mn);
    }
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(mn), menu);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void end_cb(int tid, int status, char *err)
{
  if (status)  
    insert_next_warning(err, 1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void intf_item_promisc(GtkWidget *mn, t_item_ident *pm, int on)
{
  t_bank_item *bitem;
  bitem = look_for_eth_with_id(pm->name, pm->num);
  if (bitem)
    client_promisc_set(0, end_cb, bitem->name, bitem->num, on);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void intf_item_promisc_on(GtkWidget *mn, t_item_ident *pm)
{
  intf_item_promisc(mn, pm, 1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void intf_item_promisc_off(GtkWidget *mn, t_item_ident *pm)
{
  intf_item_promisc(mn, pm, 0);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void intf_item_monitor(GtkWidget *mn, t_item_ident *pm)
{
  t_bank_item *bitem;
  bitem = look_for_eth_with_id(pm->name, pm->num);
  if (bitem)
    {
    bdplot_create(pm->name, pm->num);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void node_item_delete(GtkWidget *mn, t_item_ident *pm)
{
  t_bank_item *bitem;
  bitem = look_for_node_with_id(pm->name);
  if (bitem)
    to_cloonix_switch_delete_node(pm->name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void lan_item_delete(GtkWidget *mn, t_item_ident *pm)
{
  t_bank_item *edge=NULL, *bitem;
  t_list_bank_item *cur, *next;
  bitem = look_for_lan_with_id(pm->name);
  if (bitem)
    {
    to_cloonix_switch_delete_lan(pm->name);
    if (bitem->bank_type == bank_type_lan)
      {
      cur = bitem->head_edge_list;
      while (cur)
        {
        next = cur->next;
        if (!cur->bitem)
          KOUT("%s", pm->name);
        if (!cur->bitem->att_eth)
          KOUT("%s", pm->name);
        if (cur->bitem->att_eth->bank_type == bank_type_eth)
          {
          if (!cur->bitem->att_eth->att_node)
            KOUT("%s", pm->name);
          edge = bank_get_item(bank_type_edge,
                               cur->bitem->att_eth->att_node->name,
                               cur->bitem->att_eth->num, pm->name);
          }
        else
          {
          edge = bank_get_item(bank_type_edge,
                               cur->bitem->att_eth->name,
                               cur->bitem->att_eth->num, pm->name);
          }
        if (edge)
          call_cloonix_interface_edge_delete(edge);
        cur = next;
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void hidden_visible_eth(GtkWidget *mn, t_item_ident *pm)
{
  t_bank_item *bitem;
  bitem = look_for_eth_with_id(pm->name, pm->num);
  if (bitem)
    hidden_visible_modification(bitem, 1);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void hidden_visible_lan(GtkWidget *mn, t_item_ident *pm)
{
  t_bank_item *bitem;
  bitem = look_for_lan_with_id(pm->name);
  if (bitem)
    hidden_visible_modification(bitem, 1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void hidden_visible_sat(GtkWidget *mn, t_item_ident *pm)
{
  t_bank_item *bitem;
  bitem = look_for_sat_with_id(pm->name);
  if (bitem)
    hidden_visible_modification(bitem, 1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void hidden_visible_node(GtkWidget *mn, t_item_ident *pm)
{
  t_bank_item *bitem;
  bitem = look_for_node_with_id(pm->name);
  if (bitem)
    hidden_visible_modification(bitem, 1);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void sat_item_wireshark(GtkWidget *mn, t_item_ident *pm)
{
  char err[MAX_PATH_LEN];
  t_bank_item *bitem;
  bitem = look_for_sat_with_id(pm->name);
  if (bitem)
    {
    if (bitem->pbi.mutype == endp_type_snf)
      {
      if (!wireshark_present_in_server())
        {
        memset(err, 0, MAX_PATH_LEN);
        snprintf(err, MAX_PATH_LEN-1, 
                 "%s or %s NOT ON SERVER %s AT SERVER START", 
                 WIRESHARK_BINARY_QT, WIRESHARK_BINARY, 
                 get_doors_client_addr()); 
        insert_next_warning(err, 1);
        }
      else
        {  
        start_wireshark(pm->name, bitem);
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void sat_item_delete(GtkWidget *mn, t_item_ident *pm)
{
  t_bank_item *bitem;
  bitem = look_for_sat_with_id(pm->name);
  if (bitem)
    to_cloonix_switch_delete_sat(pm->name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void edge_item_delete(GtkWidget *mn, t_item_ident *pm)
{
  t_bank_item *bitem;
  bitem = bank_get_item(pm->bank_type, pm->name, pm->num, pm->lan);
  if (bitem)
    call_cloonix_interface_edge_delete(bitem);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_item_ident *get_and_init_pm(t_bank_item *bitem)
{
  t_item_ident *pm;
  pm = (t_item_ident *)clownix_malloc(sizeof(t_item_ident),18);
  memset(pm, 0, sizeof(t_item_ident));
  strncpy(pm->name, bitem->name, MAX_NAME_LEN-1);
  strncpy(pm->lan, bitem->lan, MAX_NAME_LEN-1);
  pm->num = bitem->num;
  pm->bank_type = bitem->bank_type;
  return pm;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void edge_ctx_menu(t_bank_item *bitem)
{
  GtkWidget *item_delete;
  GtkWidget *menu = gtk_menu_new();
  t_item_ident *pm = get_and_init_pm(bitem);
  bitem->pbi.menu_on = 1;
  item_delete = gtk_menu_item_new_with_label("Delete");
  g_signal_connect(G_OBJECT(item_delete), "activate",
                   G_CALLBACK(edge_item_delete), (gpointer) pm);
  g_signal_connect(G_OBJECT(menu), "hide",
                   G_CALLBACK(menu_hidden), (gpointer) pm);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_delete);
  gtk_widget_show_all(menu);
#if GTK_MINOR_VERSION >= 22
  gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL);
#else
  gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, 0);
#endif
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void focus_back_to_main (GtkWidget *win, gpointer data)
{
  GtkWidget *mw = (GtkWidget *)data;
  gtk_widget_grab_focus(mw);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void lan_ctx_menu(t_bank_item *bitem)
{
  GtkWidget *item_delete, *item_hidden;
  GtkWidget *menu = gtk_menu_new();
  t_item_ident *pm = get_and_init_pm(bitem);
  bitem->pbi.menu_on = 1;
  item_hidden = gtk_menu_item_new_with_label("Hidden/Visible");
  item_delete = gtk_menu_item_new_with_label("Delete");
  g_signal_connect(G_OBJECT(item_delete), "activate",
                   G_CALLBACK(lan_item_delete), (gpointer) pm);
  g_signal_connect(G_OBJECT(item_hidden), "activate",
                   G_CALLBACK(hidden_visible_lan), (gpointer) pm);
  g_signal_connect(G_OBJECT(menu), "hide",
                   G_CALLBACK(menu_hidden), (gpointer) pm);



  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_hidden);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_delete);
  gtk_widget_show_all(menu);
#if GTK_MINOR_VERSION >= 22
  gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL);
#else
  gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, 0);
#endif
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void sat_ctx_menu(t_bank_item *bitem)
{
  GtkWidget *item_delete, *item_hidden, *item_info, *item_wireshark;
  GtkWidget *menu = gtk_menu_new();
  char cmd[MAX_PATH_LEN];
  t_item_ident *pm = get_and_init_pm(bitem);
  bitem->pbi.menu_on = 1;
  if (bitem->pbi.mutype == endp_type_snf)
    {
    memset(cmd, 0, MAX_PATH_LEN);
    if (wireshark_qt_present_in_server())
      snprintf(cmd, MAX_PATH_LEN-1, "wireshark-qt");
    else
      snprintf(cmd, MAX_PATH_LEN-1, "wireshark");
    item_wireshark = gtk_menu_item_new_with_label(cmd);
    }
  item_hidden = gtk_menu_item_new_with_label("Hidden/Visible");
  item_delete = gtk_menu_item_new_with_label("Delete");
  item_info = gtk_menu_item_new_with_label("Info");
  if (bitem->pbi.mutype == endp_type_snf)
    g_signal_connect(G_OBJECT(item_wireshark), "activate",
                     G_CALLBACK(sat_item_wireshark), (gpointer) pm);
  g_signal_connect(G_OBJECT(item_delete), "activate",
                   G_CALLBACK(sat_item_delete), (gpointer) pm);
  g_signal_connect(G_OBJECT(item_hidden), "activate",
                   G_CALLBACK(hidden_visible_sat), (gpointer) pm);
  g_signal_connect(G_OBJECT(item_info), "activate",
                   G_CALLBACK(sat_item_info), (gpointer) pm);
  g_signal_connect(G_OBJECT(menu), "hide",
                   G_CALLBACK(menu_hidden), (gpointer) pm);
  if (bitem->pbi.mutype == endp_type_snf)
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_wireshark);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_hidden);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_delete);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_info);
  gtk_widget_show_all(menu);
#if GTK_MINOR_VERSION >= 22
  gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL);
#else
  gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, 0);
#endif
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void intf_item_info(GtkWidget *mn, t_item_ident *pm)
{
  t_bank_item *bitem;
  char title[2*MAX_NAME_LEN];
  bitem = look_for_eth_with_id(pm->name, pm->num);
  if (bitem)
    {
    sprintf(title, "%s eth%d", bitem->name, pm->num);
    display_info(title, " ");
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void intf_ctx_menu(t_bank_item *bitem)
{
  GtkWidget *item_promisc_on, *item_promisc_off, *item_monitor; 
  GtkWidget *separator, *menu;
  GtkWidget *item_hidden, *item_info;
  t_item_ident *pm = get_and_init_pm(bitem);
  bitem->pbi.menu_on = 1;
  menu = gtk_menu_new();
  item_promisc_on = gtk_menu_item_new_with_label("Promisc on");
  item_promisc_off = gtk_menu_item_new_with_label("Promisc off");
  item_monitor = gtk_menu_item_new_with_label("Monitor");
  item_hidden = gtk_menu_item_new_with_label("Hidden/Visible");
  item_info = gtk_menu_item_new_with_label("Info");
  separator = gtk_separator_menu_item_new();

  g_signal_connect(G_OBJECT(item_monitor), "activate",
                   G_CALLBACK(intf_item_monitor), (gpointer) pm);
  g_signal_connect(G_OBJECT(item_promisc_on), "activate",
                   G_CALLBACK(intf_item_promisc_on), (gpointer) pm);
  g_signal_connect(G_OBJECT(item_hidden), "activate",
                   G_CALLBACK(hidden_visible_eth), (gpointer) pm);
  g_signal_connect(G_OBJECT(item_promisc_off), "activate",
                   G_CALLBACK(intf_item_promisc_off), (gpointer) pm);
  g_signal_connect(G_OBJECT(item_info), "activate",
                   G_CALLBACK(intf_item_info), (gpointer) pm);


  g_signal_connect(G_OBJECT(menu), "hide",
                   G_CALLBACK(menu_hidden), (gpointer) pm);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_monitor);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_hidden);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_info);
  gtk_widget_show_all(menu);
#if GTK_MINOR_VERSION >= 22
  gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL);
#else
  gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, 0);
#endif
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void node_ctx_menu(t_bank_item *bitem)
{
  GtkWidget *dtach_console, *desktop=NULL, *xterm_qmonitor;
  GtkWidget *item_delete, *item_info, *item_color;
  GtkWidget *save_whole, *save_derived, *qreboot_vm;
  GtkWidget *separator, *separator2, *menu = gtk_menu_new();
  GtkWidget *item_hidden;
  char *whole_rootfs = "save whole rootfs"; 
  char *derived_rootfs = "save derived rootfs"; 
  t_item_ident *pm = get_and_init_pm(bitem);
  bitem->pbi.menu_on = 1;
  if (bitem->bank_type == bank_type_node) 
    {
    if (get_path_to_qemu_spice())
      desktop = gtk_menu_item_new_with_label("spice desktop");
    xterm_qmonitor = gtk_menu_item_new_with_label("qemu monitor");
    }
  if (bitem->bank_type == bank_type_node) 
    {
    save_whole = gtk_menu_item_new_with_label(whole_rootfs);
    save_derived = gtk_menu_item_new_with_label(derived_rootfs);
    qreboot_vm = gtk_menu_item_new_with_label("send reboot req to qemu");
    }
  dtach_console = gtk_menu_item_new_with_label("dtach console");
  item_info = gtk_menu_item_new_with_label("Info");
  item_color = gtk_menu_item_new_with_label("Color");
  item_hidden = gtk_menu_item_new_with_label("Hidden/Visible");
  item_delete = gtk_menu_item_new_with_label("Delete");
  separator = gtk_separator_menu_item_new();
  separator2 = gtk_separator_menu_item_new();
  g_signal_connect(G_OBJECT(item_info), "activate",
                   G_CALLBACK(node_item_info), (gpointer) pm);
  g_signal_connect(G_OBJECT(item_delete), "activate",
                   G_CALLBACK(node_item_delete), (gpointer) pm);
  g_signal_connect(G_OBJECT(item_hidden), "activate",
                   G_CALLBACK(hidden_visible_node), (gpointer) pm);
  g_signal_connect(G_OBJECT(menu), "hide",
                   G_CALLBACK(menu_hidden), (gpointer) pm);
  g_signal_connect(G_OBJECT(dtach_console), "activate",
                   G_CALLBACK(node_dtach_console), (gpointer) pm);
  if (bitem->bank_type == bank_type_node) 
    {
    if (desktop)
      g_signal_connect(G_OBJECT(desktop), "activate",
                       G_CALLBACK(node_qemu_spice), (gpointer) pm);
    g_signal_connect(G_OBJECT(xterm_qmonitor), "activate",
                   G_CALLBACK(node_xterm_qmonitor), (gpointer) pm);
    }
  if ((bitem->bank_type == bank_type_node) && (desktop)) 
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), desktop);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), dtach_console);
  if (bitem->bank_type == bank_type_node) 
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), xterm_qmonitor);

  if (bitem->bank_type == bank_type_node) 
    {
    g_signal_connect(G_OBJECT(save_whole), "activate",
                     G_CALLBACK(node_sav_whole), (gpointer) pm);
    g_signal_connect(G_OBJECT(save_derived), "activate",
                     G_CALLBACK(node_sav_derived), (gpointer) pm);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), save_whole);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), save_derived);
    g_signal_connect(G_OBJECT(qreboot_vm), "activate",
                     G_CALLBACK(node_qreboot_vm), (gpointer) pm);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), qreboot_vm);
    }
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_info);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator2);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_color);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_hidden);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_delete);
  node_item_color(item_color, pm);
  gtk_widget_show_all(menu);
#if GTK_MINOR_VERSION >= 22
  gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL);
#else
  gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, 0);
#endif
}
/*--------------------------------------------------------------------------*/



/****************************************************************************/
void menu_init(void)
{
  menu_utils_init();
  menu_dialog_vm_init();
  menu_dialog_lan_init();
  menu_dialog_tap_init();
  menu_dialog_c2c_init();
  menu_dialog_a2b_init();
  menu_dialog_snf_init();
  memset(g_sav_whole, 0, MAX_PATH_LEN);
  memset(g_sav_derived, 0, MAX_PATH_LEN);
  snprintf(g_sav_whole, MAX_PATH_LEN-1, 
           "%s/sav_whole.qcow2", getenv("HOME"));
  snprintf(g_sav_derived, MAX_PATH_LEN-1, 
           "%s/sav_derived.qcow2", getenv("HOME"));
}
/*--------------------------------------------------------------------------*/




