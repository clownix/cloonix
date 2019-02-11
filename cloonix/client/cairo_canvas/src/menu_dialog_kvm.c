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
#include "cloonix.h"

GtkWidget *get_main_window(void);

static t_custom_vm custom_vm;

static int g_timer_relaunch_dialog;
static GtkWidget *g_custom_dialog;

void menu_choice_kvm(void);

/*****************************************************************************/

/*****************************************************************************/
t_custom_vm *get_ptr_custom_vm (void)
{
  return (&custom_vm);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void get_custom_vm (t_custom_vm **cust_vm)
{
  static t_custom_vm cust;
  *cust_vm = &cust;
  memcpy(&cust, &custom_vm, sizeof(t_custom_vm));
  if (custom_vm.add_number)
    {
    custom_vm.current_number += 1;
    sprintf(cust.name, "%s%d", custom_vm.name, custom_vm.current_number);
    }
  else
    sprintf(cust.name, "%s", custom_vm.name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void append_grid(GtkWidget *grid, GtkWidget *entry, char *lab, int ln)
{
  GtkWidget *lb;
  gtk_grid_insert_row(GTK_GRID(grid), ln);
  lb = gtk_label_new(lab);
  gtk_grid_attach(GTK_GRID(grid), lb, 0, ln, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), entry, 1, ln, 1, 1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_relaunch_dialog(void *data)
{
  if (!g_custom_dialog)
    KOUT(" ");
  gtk_widget_destroy(g_custom_dialog);
  g_timer_relaunch_dialog = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void arm_timer_relaunch_dialog(void)
{
  if (!g_custom_dialog)
    KOUT(" ");
  if (!g_timer_relaunch_dialog)
    {
    clownix_timeout_add(1, timer_relaunch_dialog, NULL, NULL, NULL);
    g_timer_relaunch_dialog = 1;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void is_full_virt_toggle(GtkToggleButton *togglebutton,
                                gpointer user_data)
{
  if (gtk_toggle_button_get_active(togglebutton))
    custom_vm.is_full_virt = 1;
  else
    custom_vm.is_full_virt = 0;
  arm_timer_relaunch_dialog();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void is_uefi_toggle(GtkToggleButton *togglebutton,         
                                 gpointer user_data)
{
  if (gtk_toggle_button_get_active(togglebutton))
    custom_vm.is_uefi = 1;
  else
    custom_vm.is_uefi = 0;
  arm_timer_relaunch_dialog();
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void has_p9_host_share_toggle(GtkToggleButton *togglebutton,
                                     gpointer user_data)
{
  if (gtk_toggle_button_get_active(togglebutton))
    custom_vm.has_p9_host_share = 1;
  else
    custom_vm.has_p9_host_share = 0;
  arm_timer_relaunch_dialog();
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void is_persistent_toggle (GtkToggleButton *togglebutton,
                              gpointer user_data)
{
  if (gtk_toggle_button_get_active(togglebutton))
    custom_vm.is_persistent = 1;
  else
    custom_vm.is_persistent = 0;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void numadd_toggle (GtkToggleButton *togglebutton, gpointer user_data)
{
  if (gtk_toggle_button_get_active(togglebutton))
    custom_vm.add_number = 1;
  else
    custom_vm.add_number = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void update_cust(t_custom_vm *cust, GtkWidget *entry_name, 
                        GtkWidget *entry_rootfs, GtkWidget *entry_p9_host_share,
                        GtkWidget *entry_cpu, GtkWidget *entry_ram,
                        GtkWidget *entry_eth_nb, GtkWidget *entry_wlan_nb) 
{
  char *tmp;
  tmp = (char *) gtk_entry_get_text(GTK_ENTRY(entry_name));
  if (strcmp(tmp, cust->name))
    cust->current_number = 0;
  memset(cust->name, 0, MAX_NAME_LEN);
  strncpy(cust->name, tmp, MAX_NAME_LEN-1);

  tmp = (char *) gtk_entry_get_text(GTK_ENTRY(entry_rootfs));
  memset(cust->kvm_used_rootfs, 0, MAX_NAME_LEN);
  strncpy(cust->kvm_used_rootfs, tmp, MAX_NAME_LEN-1);

  if (custom_vm.has_p9_host_share)
    {
    tmp = (char *) gtk_entry_get_text(GTK_ENTRY(entry_p9_host_share));
    memset(cust->kvm_p9_host_share, 0, MAX_PATH_LEN);
    strncpy(cust->kvm_p9_host_share, tmp, MAX_PATH_LEN-1);
    }

  cust->cpu = (int ) gtk_spin_button_get_value(GTK_SPIN_BUTTON(entry_cpu));
  cust->mem = (int ) gtk_spin_button_get_value(GTK_SPIN_BUTTON(entry_ram));
  cust->nb_eth=(int )gtk_spin_button_get_value(GTK_SPIN_BUTTON(entry_eth_nb));
  cust->nb_wlan=(int )gtk_spin_button_get_value(GTK_SPIN_BUTTON(entry_wlan_nb));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void nb_eth_activate_cb(GtkWidget *entry_eth_nb)
{

  int nb_eth=(int )gtk_spin_button_get_value(GTK_SPIN_BUTTON(entry_eth_nb));
  custom_vm.nb_eth = nb_eth;
  arm_timer_relaunch_dialog();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void nb_wlan_activate_cb(GtkWidget *entry_wlan_nb)
{

  int nb_wlan=(int )gtk_spin_button_get_value(GTK_SPIN_BUTTON(entry_wlan_nb));
  custom_vm.nb_wlan = nb_wlan;
  arm_timer_relaunch_dialog();
}
/*--------------------------------------------------------------------------*/



/****************************************************************************/
static void custom_vm_dialog(t_custom_vm *cust)
{
  int response, line_nb = 0;
  GtkWidget *entry_name, *entry_rootfs, *entry_ram; 
  GtkWidget *entry_p9_host_share; 
  GtkWidget *entry_cpu=NULL, *entry_eth_nb, *entry_wlan_nb;
  GtkWidget *grid, *parent, *numadd;
  GtkWidget *is_persistent, *is_uefi;
  GtkWidget *is_full_virt;
  GtkWidget *has_p9_host_share;

  if (g_custom_dialog)
    return;
  grid = gtk_grid_new();
  gtk_grid_insert_column(GTK_GRID(grid), 0);
  gtk_grid_insert_column(GTK_GRID(grid), 1);
  parent = get_main_window();
  g_custom_dialog = gtk_dialog_new_with_buttons ("Custom your vm",
                                                  GTK_WINDOW (parent),
                                                  GTK_DIALOG_MODAL,
                                         "OK", GTK_RESPONSE_ACCEPT,
                                                  NULL);
  gtk_window_set_default_size(GTK_WINDOW(g_custom_dialog), 300, 20);

  entry_name = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(entry_name), cust->name);
  append_grid(grid, entry_name, "Name:", line_nb++);

  is_persistent = gtk_check_button_new_with_label("persistent_rootfs");
  if (custom_vm.is_persistent)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(is_persistent), TRUE);
  else
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(is_persistent), FALSE);
  g_signal_connect(is_persistent,"toggled",G_CALLBACK(is_persistent_toggle),NULL);



  is_uefi = gtk_check_button_new_with_label("uefi");
  if (custom_vm.is_uefi)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(is_uefi), TRUE);
  else
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(is_uefi), FALSE);
  g_signal_connect(is_uefi,"toggled",
                   G_CALLBACK(is_uefi_toggle),NULL);

  is_full_virt = gtk_check_button_new_with_label("full virt");
  if (custom_vm.is_full_virt)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(is_full_virt), TRUE);
  else
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(is_full_virt), FALSE);
  g_signal_connect(is_full_virt,"toggled",
                   G_CALLBACK(is_full_virt_toggle),NULL);


  has_p9_host_share = gtk_check_button_new_with_label("9p share host files");

  if (custom_vm.has_p9_host_share)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(has_p9_host_share), TRUE);
  else
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(has_p9_host_share), FALSE);
  g_signal_connect(has_p9_host_share,"toggled", G_CALLBACK(has_p9_host_share_toggle),NULL);



  numadd = gtk_check_button_new_with_label("end number");
  if (custom_vm.add_number)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(numadd), TRUE);
  else
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(numadd), FALSE);
  g_signal_connect(numadd, "toggled", G_CALLBACK(numadd_toggle), NULL);

  append_grid(grid, numadd, "Append:", line_nb++);

  append_grid(grid, is_persistent, "remanence of file-system:", line_nb++);
  append_grid(grid, is_uefi, "uefi:", line_nb++);
  append_grid(grid, is_full_virt, "full virt:", line_nb++);
  append_grid(grid, has_p9_host_share, "9p host share files:", line_nb++);

  entry_cpu = gtk_spin_button_new_with_range(1, 32, 1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(entry_cpu), cust->cpu);
  append_grid(grid, entry_cpu, "Nb cpu:", line_nb++);

  entry_ram = gtk_spin_button_new_with_range(128, 50000, 16);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(entry_ram), cust->mem);
  append_grid(grid, entry_ram, "Ram:", line_nb++);

  entry_rootfs = gtk_entry_new();

  gtk_entry_set_text(GTK_ENTRY(entry_rootfs), cust->kvm_used_rootfs);
  append_grid(grid, entry_rootfs, "Rootfs:", line_nb++);

  if (custom_vm.has_p9_host_share)
    {
    entry_p9_host_share = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry_p9_host_share), cust->kvm_p9_host_share);
    append_grid(grid,entry_p9_host_share,"Path to host share:",line_nb++);
    }

  entry_eth_nb = gtk_spin_button_new_with_range(1, MAX_ETH_VM, 1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(entry_eth_nb), cust->nb_eth);
  g_signal_connect(G_OBJECT(entry_eth_nb),"value-changed", 
                  (GCallback) nb_eth_activate_cb, (gpointer) cust);
  append_grid(grid, entry_eth_nb, "Max eth:", line_nb++);

  entry_wlan_nb = gtk_spin_button_new_with_range(0, MAX_WLAN_VM, 1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(entry_wlan_nb), cust->nb_wlan);
  g_signal_connect(G_OBJECT(entry_wlan_nb),"value-changed", 
                  (GCallback) nb_wlan_activate_cb, (gpointer) cust);
  append_grid(grid, entry_wlan_nb, "Max wlan:", line_nb++);

  gtk_container_set_border_width(GTK_CONTAINER(grid), 5);
  gtk_box_pack_start(
        GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(g_custom_dialog))),
        grid, TRUE, TRUE, 0);
  gtk_widget_show_all(g_custom_dialog);
  response = gtk_dialog_run(GTK_DIALOG(g_custom_dialog));
  
  if (response == GTK_RESPONSE_NONE)
    {
    g_custom_dialog = NULL;
    menu_choice_kvm();
    }
  else
    {
    update_cust(cust, entry_name,   entry_rootfs, entry_p9_host_share, 
                entry_cpu, entry_ram, entry_eth_nb, entry_wlan_nb); 
    gtk_widget_destroy(g_custom_dialog);
    g_custom_dialog = NULL;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void menu_choice_kvm(void)
{
  custom_vm_dialog(&custom_vm);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void menu_dialog_vm_init(void)
{
  char *name;
  if (inside_cloonix(&name))
    snprintf(custom_vm.name, MAX_NAME_LEN-3, "IN_%s_n", name);
  else
    strcpy(custom_vm.name, "Cloon");
  strcpy(custom_vm.kvm_used_rootfs, "stretch.qcow2");
  custom_vm.current_number = 0;
  custom_vm.add_number = 1;
  custom_vm.is_persistent = 0;
  custom_vm.is_sda_disk = 0;
  custom_vm.is_uefi = 0;
  custom_vm.is_full_virt = 0;
  custom_vm.has_p9_host_share = 0;
  custom_vm.cpu = 4;
  custom_vm.mem = 2000;
  custom_vm.nb_eth = 3;
  custom_vm.nb_wlan = 0;
  g_timer_relaunch_dialog = 0;
  g_custom_dialog = NULL;
}
/*--------------------------------------------------------------------------*/



