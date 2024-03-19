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
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "cloonix.h"
#include "menu_dialog_kvm.h"

#define ETH_TYPE_MAX 3
#define ETH_LINE_MAX 7


GtkWidget *get_bulkvm(void);
GtkWidget *get_main_window(void);
static t_custom_vm g_custom_vm;
static t_slowperiodic g_bulkvm[MAX_BULK_FILES];
static t_slowperiodic g_bulkvm_photo[MAX_BULK_FILES];
static int g_nb_bulkvm;
static GtkWidget *g_custom_dialog, *g_entry_rootfs;
void menu_choice_kvm(void);


typedef struct t_cb_endp_type
{
  int rank;
  int endp_type;
  GtkWidget **rad;
} t_cb_endp_type;


/****************************************************************************/
static void free_endp_type(GtkWidget *check, gpointer user_data)
{
  t_cb_endp_type **cb_endp_type = (t_cb_endp_type **) user_data;
  int i;
  for (i=0; i<ETH_TYPE_MAX; i++)
    free(cb_endp_type[i]);
  free(cb_endp_type);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void endp_type_cb(GtkWidget *check, gpointer user_data)
{
  t_cb_endp_type *cb_endp_type = (t_cb_endp_type *) user_data;
  t_custom_vm *cust_vm = &g_custom_vm;
  int i,j,k, max_rank = MAX_ETH_VM;
  int nb_tot_eth = 0, rank = cb_endp_type->rank;
  int endp_type = cb_endp_type->endp_type;
  GtkWidget **rad = cb_endp_type->rad;
  cust_vm->eth_tab[rank].endp_type = endp_type;

  if (endp_type == endp_type_none)
    {
    for (i=rank + 1; i<max_rank; i++)
      {
      cust_vm->eth_tab[i].endp_type = endp_type_none;
      if (i < ETH_LINE_MAX)
        {
        k = ETH_TYPE_MAX * i;
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rad[k]),TRUE);
        for(j=1; j<ETH_TYPE_MAX; j++)
          gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rad[k+j]),FALSE);
        }
      }
    }
  else
    {
    for (i=0; i<rank; i++)
      {
      k = ETH_TYPE_MAX * rank;
      if (cust_vm->eth_tab[i].endp_type == endp_type_none)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rad[k]),TRUE);
      }
    }
  for (i=0; i < max_rank; i++)
    {
    if (cust_vm->eth_tab[i].endp_type == endp_type_eths)
      nb_tot_eth += 1;
    else if (cust_vm->eth_tab[i].endp_type == endp_type_ethv)
      nb_tot_eth += 1;
    else if ((cust_vm->eth_tab[i].endp_type != endp_type_none) &&
             (cust_vm->eth_tab[i].endp_type != 0))
      KOUT("%d %d %d", i, nb_tot_eth, cust_vm->eth_tab[i].endp_type);
    }
  cust_vm->nb_tot_eth = nb_tot_eth;
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
static void flags_eth_check_button(GtkWidget *grid, int *line_nb,
                                   int rank, GtkWidget **rad)
{
  unsigned long int i;
  GtkWidget *hbox;
  t_cb_endp_type **cb_endp_type;
  char title[MAX_NAME_LEN];
  unsigned long int endp_type[ETH_TYPE_MAX]={endp_type_none,
                                             endp_type_eths,
                                             endp_type_ethv};

  memset(title, 0, MAX_NAME_LEN);
  snprintf(title, MAX_NAME_LEN-1, "Eth%d", rank);
  cb_endp_type = (t_cb_endp_type **) malloc(ETH_TYPE_MAX * sizeof(t_cb_endp_type *));
  for (i=0; i<ETH_TYPE_MAX; i++)
    {
    cb_endp_type[i] = (t_cb_endp_type *) malloc(sizeof(t_cb_endp_type));
    memset(cb_endp_type[i], 0, sizeof(t_cb_endp_type));
    cb_endp_type[i]->rank = rank;
    cb_endp_type[i]->endp_type = endp_type[i];
    cb_endp_type[i]->rad = rad;
    }
  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  for (i=0; i<ETH_TYPE_MAX; i++)
    {
    gtk_box_pack_start(GTK_BOX(hbox),rad[ETH_TYPE_MAX*rank+i],TRUE,TRUE,10);
    g_signal_connect(G_OBJECT(rad[ETH_TYPE_MAX*rank+i]),"clicked",
                       (GCallback) endp_type_cb,
                       (gpointer) cb_endp_type[i]);
    }
  append_grid(grid, hbox, title, (*line_nb)++);
  g_signal_connect (G_OBJECT (hbox), "destroy",
                    (GCallback) free_endp_type, cb_endp_type);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void qcow2_get(GtkWidget *check, gpointer data)
{
  char *name = (char *) data;
  gtk_entry_set_text(GTK_ENTRY(g_entry_rootfs), name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void has_no_qemu_ga_toggle(GtkToggleButton *togglebutton, gpointer user_data)
{
  if (gtk_toggle_button_get_active(togglebutton))
    g_custom_vm.no_qemu_ga = 1;
  else
    g_custom_vm.no_qemu_ga = 0;
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
                        GtkWidget *entry_cpu, GtkWidget *entry_ram)
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
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void custom_vm_dialog(t_custom_vm *cust)
{
  int i, j, k, response, line_nb = 0, found=0;
  GSList *group = NULL;
  GtkWidget *entry_name, *entry_ram, *entry_cpu=NULL; 
  GtkWidget *grid, *parent, *is_persistent;
  GtkWidget *has_no_qemu_ga, *has_natplug, *qcow2_rootfs, *bulkvm_menu;
  GtkWidget *rad[ETH_LINE_MAX * ETH_TYPE_MAX];
  char *lib[ETH_TYPE_MAX] = {"n", "s", "v"};

  if (g_custom_dialog)
    return;

  memcpy(g_bulkvm_photo, g_bulkvm, MAX_BULK_FILES * sizeof(t_slowperiodic));
  for (i=0; i<g_nb_bulkvm; i++)
    {
    if (!strcmp(cust->kvm_used_rootfs, g_bulkvm_photo[i].name))
      found = 1;
    }
  if ((found == 0) && (g_nb_bulkvm >= 1))
    strncpy(cust->kvm_used_rootfs, g_bulkvm_photo[0].name, MAX_NAME_LEN-1);

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


  has_no_qemu_ga = gtk_check_button_new_with_label("no_qemu_ga");
  if (g_custom_vm.no_qemu_ga)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(has_no_qemu_ga), TRUE);
  else
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(has_no_qemu_ga), FALSE);
  g_signal_connect(has_no_qemu_ga,"toggled", G_CALLBACK(has_no_qemu_ga_toggle),NULL);
  append_grid(grid, has_no_qemu_ga, "No qemu guest agent", line_nb++);

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


  for (i=0; i<ETH_LINE_MAX; i++)
    {
    group = NULL;
    for (j=0; j<ETH_TYPE_MAX; j++)
      {
      k = i*ETH_TYPE_MAX + j;
      rad[k] = gtk_radio_button_new_with_label(group,lib[j]);
      group  = gtk_radio_button_get_group(GTK_RADIO_BUTTON(rad[k]));
      if (cust->eth_tab[i].endp_type == j+endp_type_none)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rad[k]),TRUE);
      }
    }
  for (i=0; i<ETH_LINE_MAX; i++)
    {
    flags_eth_check_button(grid, &line_nb, i, rad);
    }

  gtk_container_set_border_width(GTK_CONTAINER(grid), ETH_TYPE_MAX);

  g_entry_rootfs = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(g_entry_rootfs), cust->kvm_used_rootfs);
  append_grid(grid, g_entry_rootfs, "Rootfs:", line_nb++);

  qcow2_rootfs = gtk_menu_button_new ();
  append_grid(grid, qcow2_rootfs, "Choice:", line_nb++);
  if (g_nb_bulkvm > 0)
    {
    bulkvm_menu = get_bulkvm();
    gtk_menu_button_set_popup ((GtkMenuButton *) qcow2_rootfs, bulkvm_menu);
    gtk_widget_show_all(bulkvm_menu);
    }


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
    update_cust(cust, entry_name, entry_cpu, entry_ram);
    gtk_widget_destroy(g_custom_dialog);
    g_custom_dialog = NULL;
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
GtkWidget *get_bulkvm(void)
{
  int i, found = 0;
  GtkWidget *el0, *el, *menu = NULL;
  GSList *group = NULL;
  gpointer data;
  memcpy(g_bulkvm_photo, g_bulkvm, MAX_BULK_FILES * sizeof(t_slowperiodic));
  t_custom_vm *cust = &g_custom_vm;
  if (g_nb_bulkvm > 0)
    {
    menu = gtk_menu_new();
    for (i=0; i<g_nb_bulkvm; i++)
      {
      el = gtk_radio_menu_item_new_with_label(group, g_bulkvm_photo[i].name);
      if (i == 0)
        {
        el0 = el;
        group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(el));
        }
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), el);
      data = (gpointer) g_bulkvm_photo[i].name;
      g_signal_connect(G_OBJECT(el),"activate",(GCallback)qcow2_get,data);
      if (!strcmp(cust->kvm_used_rootfs, g_bulkvm_photo[i].name))
        {
        found = 1;
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (el), TRUE);
        }
      }
    if (found == 0)
      {
      strncpy(cust->kvm_used_rootfs, g_bulkvm_photo[0].name, MAX_NAME_LEN-1);
      gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (el0), TRUE);
      }
    }
  return menu;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int get_vm_config_flags(t_custom_vm *cust_vm, int *natplug)
{
  int vm_config_flags = 0;
  *natplug = 0;
  if (cust_vm->no_qemu_ga)
    vm_config_flags |= VM_CONFIG_FLAG_NO_QEMU_GA;
  if (cust_vm->natplug_flag)
    {
    vm_config_flags |= VM_CONFIG_FLAG_NATPLUG;
    *natplug = cust_vm->natplug;
    }
  if (cust_vm->is_full_virt)
    vm_config_flags |= VM_CONFIG_FLAG_FULL_VIRT;

  if (cust_vm->is_i386)
    {
    vm_config_flags |= VM_CONFIG_FLAG_I386;
    }
  else
    {
    vm_config_flags &= ~VM_CONFIG_FLAG_I386;
    }

  if (cust_vm->is_persistent)
    {
    vm_config_flags |= VM_CONFIG_FLAG_PERSISTENT;
    }
  else
    {
    vm_config_flags &= ~VM_CONFIG_FLAG_PERSISTENT;
    }
  return vm_config_flags;
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
    sprintf(cust.name, "Cloon%d", g_custom_vm.current_number);
    }
  else
    sprintf(cust.name, "%s", g_custom_vm.name);
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

/****************************************************************************/
void menu_choice_kvm(void)
{
  custom_vm_dialog(&g_custom_vm);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void menu_dialog_kvm_init(void)
{
  int i;
  char *name;
  memset(&g_custom_vm, 0, sizeof(t_custom_vm)); 
  if (inside_cloon(&name))
    snprintf(g_custom_vm.name, MAX_NAME_LEN-3, "IN_%s_n", name);
  else
    strcpy(g_custom_vm.name, "Cloon");
  strcpy(g_custom_vm.kvm_used_rootfs, "bookworm.qcow2");
  g_custom_vm.current_number = 0;
  g_custom_vm.is_persistent = 0;
  g_custom_vm.is_sda_disk = 0;
  g_custom_vm.is_full_virt = 0;
  g_custom_vm.no_qemu_ga = 0;
  g_custom_vm.natplug_flag = 0;
  g_custom_vm.natplug = 0;
  g_custom_vm.cpu = 2;
  g_custom_vm.mem = 2000;
  g_custom_vm.nb_tot_eth = 3;
  for (i=0; i<g_custom_vm.nb_tot_eth; i++)
    g_custom_vm.eth_tab[i].endp_type = endp_type_eths;
  g_custom_dialog = NULL;
  memset(g_bulkvm, 0, MAX_BULK_FILES * sizeof(t_slowperiodic));
  g_nb_bulkvm = 0;
}
/*--------------------------------------------------------------------------*/



