/*****************************************************************************/
/*    Copyright (C) 2006-2024 clownix@clownix.net License AGPL-3             */
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
#include <sys/wait.h>

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
#include "bdplot.h"


int get_cloonix_rank(void);
GtkWidget *get_main_window(void);

static char g_sav_whole[MAX_PATH_LEN];



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
    if (bitem->pbi.menu_on)
      {
      bitem->pbi.menu_on = 0;
      bitem->pbi.grabbed = 0;
      }
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
  int response;
  char *tmp;
  char name[MAX_NAME_LEN];
  GtkWidget *entry_rootfs, *parent, *dialog;
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
  gtk_entry_set_text(GTK_ENTRY(entry_rootfs), g_sav_whole);
  gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                     entry_rootfs, TRUE, TRUE, 0);
  gtk_widget_show_all(dialog);
  response = gtk_dialog_run(GTK_DIALOG (dialog));
  if (response == GTK_RESPONSE_ACCEPT)
    {
    tmp = (char *) gtk_entry_get_text(GTK_ENTRY(entry_rootfs));
    memset(g_sav_whole, 0, MAX_PATH_LEN);
    strncpy(g_sav_whole, tmp, MAX_PATH_LEN-1);
    if (tmp && strlen(tmp))
      {
      client_sav_vm(0, end_cb_node_sav_rootfs, name, tmp);
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
  node_sav_rootfs(mn, pm);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void cnt_item_info(GtkWidget *mn, t_item_ident *pm)
{
  t_bank_item *bitem;
  static char title[MAX_PATH_LEN];
  static char text[MAX_TEXT];
  int len = 0;
  bitem = look_for_cnt_with_id(pm->name);
  if (bitem)
    {
    if (bitem->bank_type != bank_type_cnt)
      KOUT("%s", bitem->name);
    snprintf(title, MAX_PATH_LEN, "%s", bitem->name);
    title[MAX_TITLE-1] = 0;
    len += snprintf(text + len, MAX_TEXT-len, "\t\tCONTAINER");
    len += snprintf(text + len, MAX_TEXT-len-1, "\nImage: %s", 
                     bitem->pbi.pbi_cnt->image);
    text[MAX_TEXT-1] = 0;
    display_info(title, text);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void node_item_info(GtkWidget *mn, t_item_ident *pm)
{
  t_bank_item *bitem;
  static char title[MAX_PATH_LEN];
  static char text[MAX_TEXT];
  int is_i386, is_persistent, is_backed, is_inside_cloon, is_natplug;
  int vm_config_flags, has_install_cdrom, has_added_cdrom;
  int is_full_virt, has_no_reboot, has_added_disk, with_pxe, len = 0;
  bitem = look_for_node_with_id(pm->name);
  if (bitem)
    {
    if (bitem->bank_type != bank_type_node)
      KOUT("%s", bitem->name);
    snprintf(title, MAX_PATH_LEN, "%s", bitem->name);
    title[MAX_TITLE-1] = 0;
    len += snprintf(text + len, MAX_TEXT-len, "\t\tQEMU_KVM");
    text[MAX_TEXT-1] = 0;
    vm_config_flags = bitem->pbi.pbi_node->node_vm_config_flags;
    is_persistent = vm_config_flags & VM_CONFIG_FLAG_PERSISTENT;
    is_i386 = vm_config_flags & VM_CONFIG_FLAG_I386;
    is_full_virt  = vm_config_flags & VM_CONFIG_FLAG_FULL_VIRT;
    is_backed   = vm_config_flags & VM_FLAG_DERIVED_BACKING;
    is_inside_cloon = vm_config_flags & VM_FLAG_IS_INSIDE_CLOON;
    has_install_cdrom = vm_config_flags & VM_CONFIG_FLAG_INSTALL_CDROM;
    has_no_reboot = vm_config_flags & VM_CONFIG_FLAG_NO_REBOOT;
    with_pxe = vm_config_flags & VM_CONFIG_FLAG_WITH_PXE;
    has_added_cdrom = vm_config_flags & VM_CONFIG_FLAG_ADDED_CDROM;
    has_added_disk = vm_config_flags & VM_CONFIG_FLAG_ADDED_DISK;
    is_natplug = vm_config_flags & VM_CONFIG_FLAG_NATPLUG;

    if (is_i386)
      len += snprintf(text + len, MAX_TEXT-len-1, "\n\t\tI386");
    if (is_natplug)
      len += snprintf(text + len, MAX_TEXT-len-1, "\n\t\tNATPLUG");
    if (is_persistent)
      len += snprintf(text + len, MAX_TEXT-len-1, "\n\t\tPERSISTENT");
    else
      len += snprintf(text + len, MAX_TEXT-len-1, "\n\t\tEVANESCENT");
    if (is_full_virt)
      len += snprintf(text + len, MAX_TEXT-len-1, "\n\t\tFULL VIRT");
    if (is_backed)
      {
      len += snprintf(text + len, MAX_TEXT-len-1, "\n\t\tDERIVED FROM BACKING");
      len += snprintf(text + len, MAX_TEXT-len-1, "\nDerived: %s", 
                     bitem->pbi.pbi_node->node_rootfs_sod);
      len += snprintf(text + len, MAX_TEXT-len-1, "\nBacking: %s", 
                     bitem->pbi.pbi_node->node_rootfs_backing_file);
      }
    else
      {
      len += snprintf(text + len, MAX_TEXT-len-1, "\n\t\tSOLO ROOTFS");
      len += snprintf(text + len, MAX_TEXT-len-1, "\nRootfs: %s", 
                     bitem->pbi.pbi_node->node_rootfs_sod);
      }
    if (is_inside_cloon)
      len += snprintf(text + len, MAX_TEXT-len-1, "\n\t\tIS INSIDE CLOON");
    if (has_install_cdrom)
      len += snprintf(text + len, MAX_TEXT-len-1, "\n INSTALL_CDROM: %s",
                     bitem->pbi.pbi_node->install_cdrom);
    if (has_no_reboot)
      len += snprintf(text + len, MAX_TEXT-len-1, "\n NO_REBOOT");
    if (with_pxe)
      len += snprintf(text + len, MAX_TEXT-len-1, "\n WITH_PXE");
    if (has_added_cdrom)
      len += snprintf(text + len, MAX_TEXT-len-1, "\n ADDED_CDROM: %s",
                     bitem->pbi.pbi_node->added_cdrom);
    if (has_added_disk)
      len += snprintf(text + len, MAX_TEXT-len-1, "\n ADDED_DISK: %s",
                     bitem->pbi.pbi_node->added_disk);

    display_info(title, text);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int c2c_item_info(char *text,  t_bank_item *bitem)
{
  int len = 0;
  uint16_t tcp_port, loc_udp_port, dist_udp_port;
  int local_is_master, tcp_connection_peered;
  int udp_connection_peered;
  char tcp_ip[MAX_NAME_LEN];
  char loc_udp_ip[MAX_NAME_LEN];
  char dist_udp_ip[MAX_NAME_LEN];
  char *dist_cloon = bitem->pbi.pbi_sat->topo_c2c.dist_cloon; 
  int_to_ip_string (bitem->pbi.pbi_sat->topo_c2c.dist_tcp_ip, tcp_ip);
  int_to_ip_string (bitem->pbi.pbi_sat->topo_c2c.dist_udp_ip, dist_udp_ip);
  int_to_ip_string (bitem->pbi.pbi_sat->topo_c2c.loc_udp_ip, loc_udp_ip);
  tcp_port = bitem->pbi.pbi_sat->topo_c2c.dist_tcp_port;
  loc_udp_port = bitem->pbi.pbi_sat->topo_c2c.loc_udp_port;
  dist_udp_port = bitem->pbi.pbi_sat->topo_c2c.dist_udp_port;
  local_is_master = bitem->pbi.pbi_sat->topo_c2c.local_is_master;
  tcp_connection_peered = bitem->pbi.pbi_sat->topo_c2c.tcp_connection_peered;
  udp_connection_peered = bitem->pbi.pbi_sat->topo_c2c.udp_connection_peered;
  if (local_is_master)
    {
    len += sprintf(text + len,"\nLOCAL IS MASTER");
    if (tcp_connection_peered)
      {
      len += sprintf(text + len,"\nTCP PEERED %s %s:%hu",
                               dist_cloon, tcp_ip, tcp_port);
      }
    else
      {
      len += sprintf(text + len,"\nTCP NOT PEERED %s %s:%hu",
                               dist_cloon, tcp_ip, tcp_port);
      }
    }
  else
    {
    len += sprintf(text + len,"\nDISTANT IS MASTER");
    len += sprintf(text + len,"\nTCP PEERED %s", dist_cloon);
    }
  if (udp_connection_peered)
    len += sprintf(text + len,"\nUDP PEERED");
  else
    len += sprintf(text + len,"\nUDP NOT PEERED");
  len += sprintf(text + len,"\nUdp loc  %s:%hu", loc_udp_ip, loc_udp_port);
  len += sprintf(text + len,"\nUdp dist %s:%hu", dist_udp_ip, dist_udp_port);
  return len;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int a2b_item_info(char *text,  t_bank_item *bitem)
{
  int len =0;
  len += sprintf(text+len, "\nA2B: %s\n", bitem->name);
  return len;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void sat_item_info(GtkWidget *mn, t_item_ident *pm)
{
  t_bank_item *bitem;
  char title[MAX_PATH_LEN];
  char text[4*MAX_PATH_LEN];
  int len = 0;
  bitem = look_for_sat_with_id(pm->name);
  if (bitem)
    {
    snprintf(title, MAX_PATH_LEN, "%s", bitem->name);
    title[2*MAX_NAME_LEN-1] = 0;
    if ((bitem->pbi.endp_type == endp_type_phyas) ||
        (bitem->pbi.endp_type == endp_type_phyav) ||
        (bitem->pbi.endp_type == endp_type_phyms) ||
        (bitem->pbi.endp_type == endp_type_phymv))
      len += sprintf(text + len, "\nPHY");
    else if ((bitem->pbi.endp_type == endp_type_taps) ||
             (bitem->pbi.endp_type == endp_type_tapv))
      len += sprintf(text + len, "\nTAP");
    else if ((bitem->pbi.endp_type == endp_type_nats) ||
             (bitem->pbi.endp_type == endp_type_natv))
      len += sprintf(text + len, "\nNAT");
    else if ((bitem->pbi.endp_type == endp_type_c2cs) ||
             (bitem->pbi.endp_type == endp_type_c2cv))
      len += c2c_item_info(text + len, bitem);
    else if (bitem->pbi.endp_type == endp_type_a2b)
      len += a2b_item_info(text + len, bitem);
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
static void cnt_item_delete(GtkWidget *mn, t_item_ident *pm)
{
  t_bank_item *bitem;
  bitem = look_for_cnt_with_id(pm->name);
  if (bitem)
    to_cloonix_switch_delete_sat(pm->name);
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
  wireshark_launch(pm->vm_id, pm->name, pm->num);
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
  t_bank_item *att_node;
  pm = (t_item_ident *)clownix_malloc(sizeof(t_item_ident),18);
  memset(pm, 0, sizeof(t_item_ident));
  strncpy(pm->name, bitem->name, MAX_NAME_LEN-1);
  strncpy(pm->lan, bitem->lan, MAX_NAME_LEN-1);
  pm->num = bitem->num;
  pm->bank_type = bitem->bank_type;
  switch (bitem->bank_type)
    {
    case bank_type_node:
      break;
    case bank_type_cnt:
      break;
    case bank_type_lan:
      break;
    case bank_type_eth:
      att_node = bitem->att_node;
      if (!att_node)
        KOUT(" ");
      if (bitem->num < 0)
        KOUT("%s %d", bitem->name, bitem->num);
      if (att_node->bank_type == bank_type_node)
        {
        if (bitem->num >= att_node->pbi.pbi_node->nb_tot_eth)
          KOUT("%s %d", bitem->name, bitem->num);
        pm->vm_id = att_node->pbi.pbi_node->node_vm_id;
        pm->endp_type = att_node->pbi.pbi_node->eth_tab[bitem->num].endp_type;
        if ((pm->endp_type != endp_type_eths) &&
            (pm->endp_type != endp_type_ethv))
          KOUT("%s %d", bitem->name, bitem->num);
        }
      else if (att_node->bank_type == bank_type_cnt)
        {
        if (bitem->num >= att_node->pbi.pbi_cnt->nb_tot_eth)
          KOUT("%s %d", bitem->name, bitem->num);
        pm->vm_id = att_node->pbi.pbi_cnt->cnt_vm_id;
        pm->endp_type = att_node->pbi.pbi_cnt->eth_tab[bitem->num].endp_type;
        if ((pm->endp_type != endp_type_eths) &&
            (pm->endp_type != endp_type_ethv))
          KOUT("%s %d", bitem->name, bitem->num);
        }
      else if (att_node->bank_type == bank_type_sat)
        {
        if (bitem->num == 0)
          pm->endp_type = att_node->pbi.pbi_sat->topo_a2b.endp_type0;
        else if (bitem->num == 1)
          pm->endp_type = att_node->pbi.pbi_sat->topo_a2b.endp_type1;
        if ((pm->endp_type != endp_type_eths) &&
            (pm->endp_type != endp_type_ethv))
          KOUT("%s %d", bitem->name, bitem->num);
        }
      else
        KOUT("%s %d", bitem->name, bitem->num); 
      break;
    case bank_type_sat:
        pm->endp_type = bitem->pbi.endp_type;
      break;
    case bank_type_edge:
      break;
    default:
      KOUT("%d", bitem->bank_type);
    }
  return pm;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void intf_item_dyn_snf_on(GtkWidget *mn, t_item_ident *pm)
{
  to_cloonix_switch_dyn_snf_req(pm->bank_type, pm->name, pm->num, 1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void intf_item_dyn_snf_off(GtkWidget *mn, t_item_ident *pm)
{
  to_cloonix_switch_dyn_snf_req(pm->bank_type, pm->name, pm->num, 0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void wireshark_launch(int vm_id, char *name, int num)
{
  t_bank_item *bitem, *bnode, *bcnt;
  bitem = look_for_sat_with_id(name);
  bnode = look_for_node_with_id(name);
  bcnt = look_for_cnt_with_id(name);
  if ((bitem) || (bnode) || (bcnt))
    {
    if (start_wireshark(name, num))
      KERR("ERROR: wireshark for %s %d", name, num);
    }
  else
    KERR("ERROR: wireshark for %s %d", name, num);
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
  gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL);
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
  gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void sat_ctx_menu(t_bank_item *bitem)
{
  GtkWidget *item_delete, *item_hidden, *item_info, *item_wireshark;
  GtkWidget *item_dyn_snf, *menu = gtk_menu_new();
  t_item_ident *pm = get_and_init_pm(bitem);
  bitem->pbi.menu_on = 1;

  if ((pm->endp_type == endp_type_tapv)  ||
      (pm->endp_type == endp_type_phyav) ||
      (pm->endp_type == endp_type_phymv) ||
      (pm->endp_type == endp_type_natv)  ||
      (pm->endp_type == endp_type_c2cv))
    item_dyn_snf = gtk_menu_item_new_with_label("Add snf capa");

  if ((pm->endp_type == endp_type_taps)  ||
      (pm->endp_type == endp_type_phyas) ||
      (pm->endp_type == endp_type_phyms) ||
      (pm->endp_type == endp_type_nats)  ||
      (pm->endp_type == endp_type_c2cs))
    {
    item_wireshark = gtk_menu_item_new_with_label("wireshark");
    item_dyn_snf = gtk_menu_item_new_with_label("Del snf capa");
    }

  item_hidden = gtk_menu_item_new_with_label("Hidden/Visible");
  item_delete = gtk_menu_item_new_with_label("Delete");
  item_info = gtk_menu_item_new_with_label("Info");

  if ((pm->endp_type == endp_type_tapv)  ||
      (pm->endp_type == endp_type_phyav) ||
      (pm->endp_type == endp_type_phymv) ||
      (pm->endp_type == endp_type_natv)  ||
      (pm->endp_type == endp_type_c2cv))
    g_signal_connect(G_OBJECT(item_dyn_snf), "activate",
                     G_CALLBACK(intf_item_dyn_snf_on), (gpointer) pm);

  if ((pm->endp_type == endp_type_taps)  ||
      (pm->endp_type == endp_type_phyas) ||
      (pm->endp_type == endp_type_phyms) ||
      (pm->endp_type == endp_type_nats)  ||
      (pm->endp_type == endp_type_c2cs))
    {
    g_signal_connect(G_OBJECT(item_wireshark), "activate",
                     G_CALLBACK(sat_item_wireshark), (gpointer) pm);
    g_signal_connect(G_OBJECT(item_dyn_snf), "activate",
                     G_CALLBACK(intf_item_dyn_snf_off), (gpointer) pm);
    }

  g_signal_connect(G_OBJECT(item_delete), "activate",
                   G_CALLBACK(sat_item_delete), (gpointer) pm);
  g_signal_connect(G_OBJECT(item_hidden), "activate",
                   G_CALLBACK(hidden_visible_sat), (gpointer) pm);
  g_signal_connect(G_OBJECT(item_info), "activate",
                   G_CALLBACK(sat_item_info), (gpointer) pm);
  g_signal_connect(G_OBJECT(menu), "hide",
                   G_CALLBACK(menu_hidden), (gpointer) pm);

  if ((pm->endp_type == endp_type_taps)  ||
      (pm->endp_type == endp_type_phyas) ||
      (pm->endp_type == endp_type_phyms) ||
      (pm->endp_type == endp_type_nats)  ||
      (pm->endp_type == endp_type_c2cs))
    {
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_wireshark);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_dyn_snf);
    }

  if ((pm->endp_type == endp_type_tapv)  ||
      (pm->endp_type == endp_type_phyav) ||
      (pm->endp_type == endp_type_phymv) ||
      (pm->endp_type == endp_type_natv)  ||
      (pm->endp_type == endp_type_c2cv))
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_dyn_snf);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_hidden);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_delete);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_info);

  gtk_widget_show_all(menu);
  gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void intf_item_info(GtkWidget *mn, t_item_ident *pm)
{
  t_bank_item *att_node, *bitem;
  int num;
  char title[2*MAX_PATH_LEN];
  bitem = look_for_eth_with_id(pm->name, pm->num);
  if (bitem)
    {
    snprintf(title, 2*MAX_PATH_LEN, "%s eth%d", bitem->name, pm->num);
    title[2*MAX_NAME_LEN-1] = 0;
    att_node = bitem->att_node;
    num = bitem->num;
    if (!att_node)
      KOUT(" ");
    if (bitem->att_node->pbi.endp_type == endp_type_a2b)
      {
      if (num == 0)
        {
        if (att_node->pbi.pbi_sat->topo_a2b.endp_type0 == endp_type_ethv)
          display_info(title, "vhost");
        else if (att_node->pbi.pbi_sat->topo_a2b.endp_type0 == endp_type_eths)
          display_info(title, "spyed");
        else
          KOUT(" ");
        }
      else if (num == 1)
        {
        if (att_node->pbi.pbi_sat->topo_a2b.endp_type1 == endp_type_ethv)
          display_info(title, "vhost");
        else if (att_node->pbi.pbi_sat->topo_a2b.endp_type1 == endp_type_eths)
          display_info(title, "spyed");
        else
          KOUT(" ");
        }
      else
        KOUT(" ");
      }
    else
      {
      if ((num < 0) || (num >= att_node->pbi.pbi_node->nb_tot_eth))
        KOUT(" ");
      if (att_node->pbi.pbi_node->eth_tab[num].endp_type == endp_type_ethv)
        display_info(title, "vhost");
      else if (att_node->pbi.pbi_node->eth_tab[num].endp_type == endp_type_eths)
        display_info(title, "spyed");
      else
        KOUT(" ");
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void intf_ctx_menu(t_bank_item *bitem)
{
  GtkWidget *item_promisc_on, *item_promisc_off, *item_monitor; 
  GtkWidget *separator, *menu = gtk_menu_new();
  GtkWidget *item_hidden, *item_info, *item_wireshark, *item_dyn_snf;
  t_item_ident *pm = get_and_init_pm(bitem);
  bitem->pbi.menu_on = 1;
  if (pm->endp_type == endp_type_ethv)
    item_dyn_snf = gtk_menu_item_new_with_label("Add snf capa");
  if (pm->endp_type == endp_type_eths)
    {
    item_wireshark = gtk_menu_item_new_with_label("wireshark");
    item_dyn_snf = gtk_menu_item_new_with_label("Del snf capa");
    }
  item_promisc_on = gtk_menu_item_new_with_label("Promisc on");
  item_promisc_off = gtk_menu_item_new_with_label("Promisc off");
  item_monitor = gtk_menu_item_new_with_label("Monitor");
  item_hidden = gtk_menu_item_new_with_label("Hidden/Visible");
  item_info = gtk_menu_item_new_with_label("Info");
  separator = gtk_separator_menu_item_new();
  if (pm->endp_type == endp_type_ethv)
    g_signal_connect(G_OBJECT(item_dyn_snf), "activate",
                     G_CALLBACK(intf_item_dyn_snf_on), (gpointer) pm);
  if (pm->endp_type == endp_type_eths)
    {
    g_signal_connect(G_OBJECT(item_wireshark), "activate",
                     G_CALLBACK(sat_item_wireshark), (gpointer) pm);
    g_signal_connect(G_OBJECT(item_dyn_snf), "activate",
                     G_CALLBACK(intf_item_dyn_snf_off), (gpointer) pm);
    }
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
  if (pm->endp_type == endp_type_ethv)
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_dyn_snf);
  if (pm->endp_type == endp_type_eths)
    {
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_wireshark);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_dyn_snf);
    }
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_monitor);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_hidden);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_info);
  gtk_widget_show_all(menu);
  gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void node_ctx_menu(t_bank_item *bitem)
{
  GtkWidget *dtach_console, *desktop=NULL;
  GtkWidget *item_delete, *item_info, *item_color;
  GtkWidget *save_whole, *qreboot_vm;
  GtkWidget *separator, *separator2, *menu = gtk_menu_new();
  GtkWidget *item_hidden;
  t_item_ident *pm = get_and_init_pm(bitem);
  bitem->pbi.menu_on = 1;
  if (bitem->bank_type != bank_type_node)
    KOUT("%s", bitem->name);
  if (get_path_to_qemu_spice())
    desktop = gtk_menu_item_new_with_label("Spice");
  save_whole = gtk_menu_item_new_with_label("Save");
  qreboot_vm = gtk_menu_item_new_with_label("Reboot");
  dtach_console = gtk_menu_item_new_with_label("Console");
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
  if (desktop)
    g_signal_connect(G_OBJECT(desktop), "activate",
                     G_CALLBACK(node_qemu_spice), (gpointer) pm);
  if (desktop)
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), desktop);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), dtach_console);

  g_signal_connect(G_OBJECT(save_whole), "activate",
                   G_CALLBACK(node_sav_whole), (gpointer) pm);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), save_whole);

  g_signal_connect(G_OBJECT(qreboot_vm), "activate",
                   G_CALLBACK(node_qreboot_vm), (gpointer) pm);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), qreboot_vm);

  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_delete);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_info);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator2);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_color);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_hidden);
  node_item_color(item_color, pm);
  gtk_widget_show_all(menu);
  gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cnt_ctx_menu(t_bank_item *bitem)
{
  GtkWidget *item_rsh, *item_info, *item_delete, *menu = gtk_menu_new();
  t_item_ident *pm = get_and_init_pm(bitem);
  bitem->pbi.menu_on = 1;
  if (bitem->bank_type != bank_type_cnt)
    KOUT("%s", bitem->name);

  item_rsh = gtk_menu_item_new_with_label("Remote console");
  item_info = gtk_menu_item_new_with_label("Info");
  item_delete = gtk_menu_item_new_with_label("Delete");

  g_signal_connect(G_OBJECT(item_rsh), "activate",
                   G_CALLBACK(crun_item_rsh), (gpointer) pm);

  g_signal_connect(G_OBJECT(item_info), "activate",
                   G_CALLBACK(cnt_item_info), (gpointer) pm);
  g_signal_connect(G_OBJECT(item_delete), "activate",
                   G_CALLBACK(cnt_item_delete), (gpointer) pm);
  g_signal_connect(G_OBJECT(menu), "hide",
                   G_CALLBACK(menu_hidden), (gpointer) pm);

  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_rsh);

  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_info);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_delete);
  gtk_widget_show_all(menu);
  gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void menu_init(void)
{
  menu_utils_init();
  menu_dialog_kvm_init();
  menu_dialog_cnt_init();
  menu_dialog_c2c_init();
  memset(g_sav_whole, 0, MAX_PATH_LEN);
  snprintf(g_sav_whole, MAX_PATH_LEN-1, "/tmp/sav_whole.qcow2"); 
}
/*--------------------------------------------------------------------------*/




