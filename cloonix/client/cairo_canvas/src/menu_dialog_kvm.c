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
static void is_cisco_toggle(GtkToggleButton *togglebutton, gpointer user_data)
{
  if (gtk_toggle_button_get_active(togglebutton))
    g_custom_vm.is_cisco = 1;
  else
    g_custom_vm.is_cisco = 0;
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
                        GtkWidget *entry_p9_host_share,
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
static void free_eth_type(GtkWidget *check, gpointer user_data)
{
  t_cb_eth_type **cb_eth_type = (t_cb_eth_type **) user_data;
  int i;
  for (i=0; i<ETH_TYPE_MAX; i++)
    free(cb_eth_type[i]);
  free(cb_eth_type);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void eth_type_cb(GtkWidget *check, gpointer user_data)
{
  t_cb_eth_type *cb_eth_type = (t_cb_eth_type *) user_data;
  t_custom_vm *cust_vm = get_ptr_custom_vm();
  int i,j,k, max_rank = MAX_SOCK_VM + MAX_DPDK_VM + MAX_WLAN_VM;
  int nb_tot_eth = 0, rank = cb_eth_type->rank;
  int eth_type = cb_eth_type->eth_type; 
  GtkWidget **rad = cb_eth_type->rad;
  cust_vm->eth_tab[rank].eth_type = eth_type;

  if (eth_type == eth_type_none)
    {
    for (i=rank + 1; i<max_rank; i++)
      {
      cust_vm->eth_tab[i].eth_type = eth_type_none;
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
      if (cust_vm->eth_tab[i].eth_type == eth_type_none)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rad[k]),TRUE);
      } 
    }
  for (i=0; i < max_rank; i++)
    {
    if (cust_vm->eth_tab[i].eth_type == eth_type_sock)
      nb_tot_eth += 1;
    else if (cust_vm->eth_tab[i].eth_type == eth_type_dpdk)
      nb_tot_eth += 1;
    else if (cust_vm->eth_tab[i].eth_type == eth_type_wlan)
      nb_tot_eth += 1;
    else if ((cust_vm->eth_tab[i].eth_type != eth_type_none) &&
             (cust_vm->eth_tab[i].eth_type != 0))
      KOUT("%d %d %d", i, nb_tot_eth, cust_vm->eth_tab[i].eth_type);
    }
  cust_vm->nb_tot_eth = nb_tot_eth;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void flags_eth_check_button(GtkWidget *grid, int *line_nb,
                                   int rank, GtkWidget **rad)
{
  unsigned long int i;
  GtkWidget *hbox;
  t_cb_eth_type **cb_eth_type;
  char title[MAX_NAME_LEN];
  unsigned long int endp_type[ETH_TYPE_MAX]={eth_type_none, eth_type_sock,
                                  eth_type_dpdk, eth_type_wlan};
  
  memset(title, 0, MAX_NAME_LEN); 
  snprintf(title, MAX_NAME_LEN-1, "Eth%d", rank);
  cb_eth_type = (t_cb_eth_type **) malloc(ETH_TYPE_MAX * sizeof(t_cb_eth_type *));
  for (i=0; i<ETH_TYPE_MAX; i++)
    {
    cb_eth_type[i] = (t_cb_eth_type *) malloc(sizeof(t_cb_eth_type)); 
    memset(cb_eth_type[i], 0, sizeof(t_cb_eth_type));
    cb_eth_type[i]->rank = rank;
    cb_eth_type[i]->eth_type = endp_type[i];
    cb_eth_type[i]->rad = rad;
    }
  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  for (i=0; i<ETH_TYPE_MAX; i++)
    {
    gtk_box_pack_start(GTK_BOX(hbox),rad[ETH_TYPE_MAX*rank+i],TRUE,TRUE,10);
    g_signal_connect(G_OBJECT(rad[ETH_TYPE_MAX*rank+i]),"clicked",
                       (GCallback) eth_type_cb,
                       (gpointer) cb_eth_type[i]);
    }
  append_grid(grid, hbox, title, (*line_nb)++);
  g_signal_connect (G_OBJECT (hbox), "destroy", 
                    (GCallback) free_eth_type, cb_eth_type);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void custom_vm_dialog(t_custom_vm *cust)
{
  int i, j, k, response, line_nb = 0;
  GtkWidget *entry_name, *entry_ram; 
  GtkWidget *entry_p9_host_share=NULL, *entry_cpu=NULL; 
  GtkWidget *grid, *parent, *is_persistent;
  GtkWidget *is_cisco, *qcow2_rootfs, *bulkvm_menu;
  GtkWidget *rad[ETH_LINE_MAX * ETH_TYPE_MAX];
  GSList *group;
  char *lib[ETH_TYPE_MAX] = {"n", "s", "d", "w"};
  t_custom_vm *cust_vm;

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


   is_cisco = gtk_check_button_new_with_label("csr1000v qcow2");
   if (g_custom_vm.is_cisco)
     gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(is_cisco), TRUE);
   else
     gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(is_cisco), FALSE);
   g_signal_connect(is_cisco,"toggled", G_CALLBACK(is_cisco_toggle),NULL);

  append_grid(grid, is_cisco, "Is cisco:", line_nb++);

  append_grid(grid, is_persistent, "remanence:", line_nb++);
  entry_cpu = gtk_spin_button_new_with_range(1, 32, 1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(entry_cpu), cust->cpu);
  append_grid(grid, entry_cpu, "Cpu:", line_nb++);

  entry_ram = gtk_spin_button_new_with_range(128, 50000, 16);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(entry_ram), cust->mem);
  append_grid(grid, entry_ram, "Ram:", line_nb++);

  g_entry_rootfs = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(g_entry_rootfs), cust->kvm_used_rootfs);
  append_grid(grid, g_entry_rootfs, "Rootfs:", line_nb++);

  qcow2_rootfs = gtk_menu_button_new ();
  append_grid(grid, qcow2_rootfs, "Choice:", line_nb++);
  bulkvm_menu = get_bulkvm();
  gtk_menu_button_set_popup ((GtkMenuButton *) qcow2_rootfs, bulkvm_menu);
  gtk_widget_show_all(bulkvm_menu);


  cust_vm = get_ptr_custom_vm();
  for (i=0; i<ETH_LINE_MAX; i++)
    {
    group = NULL;
    for (j=0; j<ETH_TYPE_MAX; j++)
      {
      k= i*ETH_TYPE_MAX + j;
      rad[k] = gtk_radio_button_new_with_label(group,lib[j]);
      group  = gtk_radio_button_get_group(GTK_RADIO_BUTTON(rad[k]));
      if (cust_vm->eth_tab[i].eth_type == j+eth_type_none)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rad[k]),TRUE);
      }
    }
  for (i=0; i<ETH_LINE_MAX; i++)
    {  
    flags_eth_check_button(grid, &line_nb, i, rad);
    }

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
    update_cust(cust, entry_name, entry_p9_host_share, entry_cpu, entry_ram);
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
  char *name;
  if (inside_cloonix(&name))
    snprintf(g_custom_vm.name, MAX_NAME_LEN-3, "IN_%s_n", name);
  else
    strcpy(g_custom_vm.name, "Cloon");
  strcpy(g_custom_vm.kvm_used_rootfs, "buster.qcow2");
  g_custom_vm.current_number = 0;
  g_custom_vm.is_persistent = 0;
  g_custom_vm.is_sda_disk = 0;
  g_custom_vm.is_full_virt = 0;
  g_custom_vm.has_p9_host_share = 0;
  g_custom_vm.is_cisco = 0;
  g_custom_vm.cpu = 2;
  g_custom_vm.mem = 2000;
  g_custom_vm.nb_tot_eth = 3;
  g_custom_vm.eth_tab[0].eth_type = eth_type_dpdk;
  g_custom_vm.eth_tab[1].eth_type = eth_type_dpdk;
  g_custom_vm.eth_tab[2].eth_type = eth_type_dpdk;
  g_custom_dialog = NULL;
  memset(g_bulkvm, 0, MAX_BULK_FILES * sizeof(t_slowperiodic));
  g_nb_bulkvm = 0;
}
/*--------------------------------------------------------------------------*/



