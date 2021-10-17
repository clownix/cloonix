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

#define ETH_TYPE_MAX 4
#define ETH_LINE_MAX 7

GtkWidget *get_main_window(void);
static t_custom_vm g_custom_vm;
static t_slowperiodic g_bulkvm[MAX_BULK_FILES];
static t_slowperiodic g_bulkvm_photo[MAX_BULK_FILES];
static int g_nb_bulkvm;
static GtkWidget *g_custom_dialog, *g_entry_rootfs;
void menu_choice_kvm(void);

typedef struct t_cb_eth_type
{
  int rank;
  int eth_type;
  GtkWidget **rad;
} t_cb_eth_type;


/****************************************************************************/
static void qcow2_get(GtkWidget *check, gpointer data)
{
  char *name = (char *) data;
  gtk_entry_set_text(GTK_ENTRY(g_entry_rootfs), name);
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
static GtkWidget *get_bulkvm(void)
{
  int i;
  GtkWidget *el, *menu = NULL;
  GSList *group = NULL;
  gpointer data;
  memcpy(g_bulkvm_photo, g_bulkvm, MAX_BULK_FILES * sizeof(t_slowperiodic));
  if (g_nb_bulkvm > 0)
    {
    menu = gtk_menu_new();
    for (i=0; i<g_nb_bulkvm; i++)
      {
      el = gtk_radio_menu_item_new_with_label(group, g_bulkvm_photo[i].name);
      if (i == 0)
        group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(el));
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), el);
      data = (gpointer) g_bulkvm_photo[i].name;
      g_signal_connect(G_OBJECT(el),"activate",(GCallback)qcow2_get,data);
      }
    }
  return menu;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
t_custom_vm *get_ptr_custom_vm (void)
{
  return (&g_custom_vm);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void get_custom_vm (t_custom_vm **cust_vm)
{
  static t_custom_vm cust;
  *cust_vm = &cust;
  memcpy(&cust, &g_custom_vm, sizeof(t_custom_vm));
  if (!strcmp(g_custom_vm.name, "Cloon"))
    {
    g_custom_vm.current_number += 1;
    sprintf(cust.name, "%s%d", g_custom_vm.name, g_custom_vm.current_number);
    }
  else
    sprintf(cust.name, "%s", g_custom_vm.name);
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
static void has_nobackdoor_toggle(GtkToggleButton *togglebutton, gpointer user_data)
{
  if (gtk_toggle_button_get_active(togglebutton))
    g_custom_vm.nobackdoor_flag = 1;
  else
    g_custom_vm.nobackdoor_flag = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void has_natplug_toggle(GtkToggleButton *togglebutton, gpointer user_data)
{
  if (gtk_toggle_button_get_active(togglebutton))
    g_custom_vm.natplug_flag = 1;
  else
    g_custom_vm.natplug_flag = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void is_persistent_toggle (GtkToggleButton *togglebutton,
                              gpointer user_data)
{
  if (gtk_toggle_button_get_active(togglebutton))
    g_custom_vm.is_persistent = 1;
  else
    g_custom_vm.is_persistent = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void update_cust(t_custom_vm *cust, GtkWidget *entry_name, 
                        GtkWidget *entry_p9_host_share, GtkWidget *entry_cpu,
                        GtkWidget *entry_ram, GtkWidget *entry_eth)
{
  char *tmp;
  tmp = (char *) gtk_entry_get_text(GTK_ENTRY(entry_name));
  if (strcmp(tmp, cust->name))
    cust->current_number = 0;
  memset(cust->name, 0, MAX_NAME_LEN);
  strncpy(cust->name, tmp, MAX_NAME_LEN-1);

  tmp = (char *) gtk_entry_get_text(GTK_ENTRY(g_entry_rootfs));
  memset(cust->kvm_used_rootfs, 0, MAX_NAME_LEN);
  strncpy(cust->kvm_used_rootfs, tmp, MAX_NAME_LEN-1);

  cust->cpu = (int ) gtk_spin_button_get_value(GTK_SPIN_BUTTON(entry_cpu));
  cust->mem = (int ) gtk_spin_button_get_value(GTK_SPIN_BUTTON(entry_ram));
  cust->eth = (int ) gtk_spin_button_get_value(GTK_SPIN_BUTTON(entry_eth));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void custom_vm_dialog(t_custom_vm *cust)
{
  int response, line_nb = 0;
  GtkWidget *entry_name, *entry_ram, *entry_eth; 
  GtkWidget *entry_p9_host_share=NULL, *entry_cpu=NULL; 
  GtkWidget *grid, *parent, *is_persistent;
  GtkWidget *has_nobackdoor, *has_natplug, *qcow2_rootfs, *bulkvm_menu;

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
  if (g_custom_vm.is_persistent)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(is_persistent), TRUE);
  else
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(is_persistent), FALSE);
  g_signal_connect(is_persistent,"toggled",G_CALLBACK(is_persistent_toggle),NULL);


  has_nobackdoor = gtk_check_button_new_with_label("nobackdoor_flag");
  if (g_custom_vm.nobackdoor_flag)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(has_nobackdoor), TRUE);
  else
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(has_nobackdoor), FALSE);
  g_signal_connect(has_nobackdoor,"toggled", G_CALLBACK(has_nobackdoor_toggle),NULL);
  append_grid(grid, has_nobackdoor, "No hvc0 backdoor", line_nb++);

  has_natplug = gtk_check_button_new_with_label("natplug_flag");
  if (g_custom_vm.natplug_flag)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(has_natplug), TRUE);
  else
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(has_natplug), FALSE);
  g_signal_connect(has_natplug,"toggled", G_CALLBACK(has_natplug_toggle),NULL);

  append_grid(grid, has_natplug, "Nat on eth0:", line_nb++);

  append_grid(grid, is_persistent, "remanence:", line_nb++);
  entry_cpu = gtk_spin_button_new_with_range(1, 32, 1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(entry_cpu), cust->cpu);
  append_grid(grid, entry_cpu, "Cpu:", line_nb++);

  entry_ram = gtk_spin_button_new_with_range(128, 50000, 16);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(entry_ram), cust->mem);
  append_grid(grid, entry_ram, "Ram:", line_nb++);

  entry_eth = gtk_spin_button_new_with_range(0, MAX_DPDK_VM, 1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(entry_eth), cust->eth);
  append_grid(grid, entry_eth, "Eth:", line_nb++);

  g_entry_rootfs = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(g_entry_rootfs), cust->kvm_used_rootfs);
  append_grid(grid, g_entry_rootfs, "Rootfs:", line_nb++);

  qcow2_rootfs = gtk_menu_button_new ();
  append_grid(grid, qcow2_rootfs, "Choice:", line_nb++);
  bulkvm_menu = get_bulkvm();
  gtk_menu_button_set_popup ((GtkMenuButton *) qcow2_rootfs, bulkvm_menu);
  gtk_widget_show_all(bulkvm_menu);

  gtk_container_set_border_width(GTK_CONTAINER(grid), ETH_TYPE_MAX);
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
    update_cust(cust, entry_name, entry_p9_host_share, entry_cpu,
                entry_ram, entry_eth);
    gtk_widget_destroy(g_custom_dialog);
    g_custom_dialog = NULL;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void menu_choice_kvm(void)
{
  custom_vm_dialog(&g_custom_vm);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void menu_dialog_vm_init(void)
{
  int i;
  char *name;
  memset(&g_custom_vm, 0, sizeof(t_custom_vm)); 
  if (inside_cloonix(&name))
    snprintf(g_custom_vm.name, MAX_NAME_LEN-3, "IN_%s_n", name);
  else
    strcpy(g_custom_vm.name, "Cloon");
  strcpy(g_custom_vm.kvm_used_rootfs, "bullseye.qcow2");
  g_custom_vm.current_number = 0;
  g_custom_vm.is_persistent = 0;
  g_custom_vm.is_sda_disk = 0;
  g_custom_vm.is_full_virt = 0;
  g_custom_vm.has_p9_host_share = 0;
  g_custom_vm.nobackdoor_flag = 0;
  g_custom_vm.natplug_flag = 0;
  g_custom_vm.natplug = 0;
  g_custom_vm.cpu = 2;
  g_custom_vm.mem = 2000;
  g_custom_vm.eth = 3;
  for (i=0; i<MAX_DPDK_VM; i++)
    g_custom_vm.eth_table[i].eth_type = eth_type_dpdk;
  g_custom_dialog = NULL;
  memset(g_bulkvm, 0, MAX_BULK_FILES * sizeof(t_slowperiodic));
  g_nb_bulkvm = 0;
}
/*--------------------------------------------------------------------------*/



