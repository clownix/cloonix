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
#include "menu_dialog_cnt.h"

#define ETH_TYPE_MAX 3
#define ETH_LINE_MAX 7


GtkWidget *get_main_window(void);
static t_custom_cnt g_custom_cnt;
static GtkWidget *g_custom_dialog, *g_entry_image;
static GtkWidget *g_image_menu;
static GtkWidget *g_brandtype_label;
void menu_choice_cnt(void);
static t_slowperiodic g_bulzip[MAX_BULK_FILES];
static int g_nb_bulzip;
static t_slowperiodic g_bulcvm[MAX_BULK_FILES];
static int g_nb_bulcvm;


typedef struct t_cb_endp_type
{
  int rank;
  int endp_type;
  GtkWidget **rad;
} t_cb_endp_type;



/****************************************************************************/
static void img_get(GtkWidget *check, gpointer data)
{
  char *name = (char *) data;
  t_custom_cnt *cust = &g_custom_cnt;
  memset(cust->zip_image, 0, MAX_NAME_LEN);
  strncpy(cust->zip_image, name, MAX_NAME_LEN-1);
  gtk_entry_set_text(GTK_ENTRY(g_entry_image), name);
}
/*--------------------------------------------------------------------------*/
 
/****************************************************************************/
static void append_grid(GtkWidget *grid, GtkWidget *entry, char *lab, int ln,
                        GtkWidget *input_label)
{   
  GtkWidget *label;
  if (input_label != NULL)
    label = input_label;
  else
    label = gtk_label_new(lab);
  gtk_grid_insert_row(GTK_GRID(grid), ln);
  gtk_grid_attach(GTK_GRID(grid), label, 0, ln, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), entry, 1, ln, 1, 1);
} 
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static GtkWidget *get_bul(int nb_bul, t_slowperiodic *bul, char *image)
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
      g_signal_connect(G_OBJECT(el),"activate",(GCallback)img_get,data);
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
static void setup_list_choices_zip(void)
{
  GtkWidget *menu = NULL;
  t_custom_cnt *cust = &g_custom_cnt;
  gtk_entry_set_text(GTK_ENTRY(g_entry_image), cust->zip_image);
  menu = get_bul(g_nb_bulzip, g_bulzip, cust->zip_image);
  if (menu)
    {
    gtk_menu_button_set_popup ((GtkMenuButton *)g_image_menu, menu);
    gtk_widget_show_all(menu);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void setup_list_choices_cvm(void)
{
  GtkWidget *menu = NULL;
  t_custom_cnt *cust = &g_custom_cnt;
  gtk_entry_set_text(GTK_ENTRY(g_entry_image), cust->cvm_image);
  menu = get_bul(g_nb_bulcvm, g_bulcvm, cust->cvm_image);
  if (menu)
    {
    gtk_menu_button_set_popup ((GtkMenuButton *)g_image_menu, menu);
    gtk_widget_show_all(menu);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void brandtype_cb(GtkWidget *check, gpointer user_data)
{
  unsigned long uli = (unsigned long) user_data;
  if (uli == 0)
    {
    strncpy(g_custom_cnt.brandtype, "brandzip", MAX_NAME_LEN);
    gtk_entry_set_text(GTK_ENTRY(g_entry_image), g_custom_cnt.zip_image);
    setup_list_choices_zip();
    }
  else if(uli == 1)
    {
    strncpy(g_custom_cnt.brandtype, "brandcvm", MAX_NAME_LEN);
    gtk_entry_set_text(GTK_ENTRY(g_entry_image), g_custom_cnt.cvm_image);
    setup_list_choices_cvm();
    }
  else
    KOUT("ERROR %lu", uli);
  gtk_label_set_text(GTK_LABEL(g_brandtype_label), g_custom_cnt.brandtype);
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

/****************************************************************************/
static void is_persistent_toggle (GtkToggleButton *togglebutton,
                                  gpointer user_data)
{
  if (gtk_toggle_button_get_active(togglebutton))
    g_custom_cnt.is_persistent = 1;
  else
    g_custom_cnt.is_persistent = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void update_cust(t_custom_cnt *cust,
                        GtkWidget *entry_name,
                        GtkWidget *entry_startup_env,
                        GtkWidget *entry_vmount)
{
  int len;
  char *tmp;
  tmp = (char *) gtk_entry_get_text(GTK_ENTRY(entry_name));
  memset(cust->name, 0, MAX_NAME_LEN);
  strncpy(cust->name, tmp, MAX_NAME_LEN-1);
  tmp = (char *) gtk_entry_get_text(GTK_ENTRY(entry_startup_env));
  memset(cust->startup_env, 0, MAX_PATH_LEN);
  strncpy(cust->startup_env, tmp, MAX_PATH_LEN-1);
  tmp = (char *) gtk_entry_get_text(GTK_ENTRY(entry_vmount));
  len = strspn(tmp, " \r\n\t");
  memset(cust->vmount, 0, MAX_SIZE_VMOUNT);
  strncpy(cust->vmount, tmp+len, MAX_SIZE_VMOUNT-1);
}
/*--------------------------------------------------------------------------*/
 
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
  t_custom_cnt *cust_vm = &g_custom_cnt;
  int i,j,k, max_rank = MAX_ETH_VM;
  int nb_tot_eth = 0, rank = cb_endp_type->rank;
  int endp_type = cb_endp_type->endp_type;
  GtkWidget **rad = cb_endp_type->rad;
  cust_vm->eth_table[rank].endp_type = endp_type;

  if (endp_type == endp_type_none)
    {
    for (i=rank + 1; i<max_rank; i++)
      {
      cust_vm->eth_table[i].endp_type = endp_type_none;
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
      if (cust_vm->eth_table[i].endp_type == endp_type_none)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rad[k]),TRUE);
      }
    }
  for (i=0; i < max_rank; i++)
    {
    if (cust_vm->eth_table[i].endp_type == endp_type_eths)
      nb_tot_eth += 1;
    else if (cust_vm->eth_table[i].endp_type == endp_type_ethv)
      nb_tot_eth += 1;
    else if ((cust_vm->eth_table[i].endp_type != endp_type_none) &&
             (cust_vm->eth_table[i].endp_type != 0))
      KOUT("%d %d %d", i, nb_tot_eth, cust_vm->eth_table[i].endp_type);
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
  t_cb_endp_type **cb_endp_type;
  char title[MAX_NAME_LEN];
  unsigned long int endp_type[ETH_TYPE_MAX]={endp_type_none,
                                             endp_type_eths,
                                             endp_type_ethv};

  memset(title, 0, MAX_NAME_LEN);
  snprintf(title, MAX_NAME_LEN-1, "Eth%d", rank);
  cb_endp_type=(t_cb_endp_type **)malloc(ETH_TYPE_MAX*sizeof(t_cb_endp_type *));
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
  append_grid(grid, hbox, title, (*line_nb)++, NULL);
  g_signal_connect (G_OBJECT (hbox), "destroy",
                    (GCallback) free_endp_type, cb_endp_type);
}
/*--------------------------------------------------------------------------*/





/****************************************************************************/
static void custom_vm_dialog(t_custom_cnt *cust)
{
  int i, j, k, response, line_nb = 0, found=0;
  unsigned long uli;
  GSList *group = NULL;
  GSList *brandgroup = NULL;
  GtkWidget *entry_name, *entry_startup_env, *grid, *parent;
  GtkWidget *is_persistent;
  GtkWidget *entry_vmount, *rad[ETH_LINE_MAX * ETH_TYPE_MAX];
  GtkWidget *brandtype[BRANDTYPE_NB_MAX];
  GtkWidget *hbox_brandtype;
  char *brandlib[BRANDTYPE_NB_MAX] = {"brandzip", "brandcvm"};
  char *lib[ETH_TYPE_MAX] = {"n", "s", "v"};

  if (g_custom_dialog)
    return;
  
  g_image_menu = gtk_menu_button_new();
  g_entry_image = gtk_entry_new();

  if (!strcmp(cust->brandtype, "brandzip"))
    {
    for (i=0; i<g_nb_bulzip; i++)
      {
      if (!strcmp(cust->zip_image, g_bulzip[i].name))
        found = 1;
      }
    if ((found == 0) && (g_nb_bulzip >= 1))
      strncpy(cust->zip_image, g_bulzip[0].name, MAX_NAME_LEN-1);
    }
  else if (!strcmp(cust->brandtype, "brandcvm"))
    {
    for (i=0; i<g_nb_bulcvm; i++)
      {
      if (!strcmp(cust->cvm_image, g_bulcvm[i].name))
        found = 1;
      }
    if ((found == 0) && (g_nb_bulcvm >= 1))
      strncpy(cust->cvm_image, g_bulcvm[0].name, MAX_NAME_LEN-1);
    }
  else
    KOUT("ERROR %s", cust->brandtype);

  grid = gtk_grid_new();
  gtk_grid_insert_column(GTK_GRID(grid), 0);
  gtk_grid_insert_column(GTK_GRID(grid), 1);
  parent = get_main_window();
  g_custom_dialog = gtk_dialog_new_with_buttons ("Custom your vm",
                             GTK_WINDOW (parent), GTK_DIALOG_MODAL,
                             "OK", GTK_RESPONSE_ACCEPT, NULL);
  gtk_window_set_default_size(GTK_WINDOW(g_custom_dialog), 300, 20);

  entry_name = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(entry_name), cust->name);
  append_grid(grid, entry_name, "Name:", line_nb++, NULL);

  hbox_brandtype = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  for (i=0; i<BRANDTYPE_NB_MAX; i++)
    {
    brandtype[i] = gtk_radio_button_new_with_label(brandgroup, brandlib[i]);
    brandgroup = gtk_radio_button_get_group(GTK_RADIO_BUTTON(brandtype[i]));
    if (!strcmp(cust->brandtype, brandlib[i]))
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(brandtype[i]),TRUE);
    }
  for (uli=0; uli<BRANDTYPE_NB_MAX; uli++)
    {
    gtk_box_pack_start(GTK_BOX(hbox_brandtype), brandtype[uli],TRUE,TRUE,10);
    g_signal_connect(G_OBJECT(brandtype[uli]), "clicked",
                     G_CALLBACK(brandtype_cb), (gpointer) uli); 
    }
  append_grid(grid, hbox_brandtype, "brandtype", line_nb++, NULL);

  is_persistent = gtk_check_button_new_with_label("persistent_rootfs");
  if (cust->is_persistent)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(is_persistent), TRUE);
  else
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(is_persistent), FALSE);
  g_signal_connect(is_persistent,"toggled",G_CALLBACK(is_persistent_toggle),NULL);
  append_grid(grid, is_persistent, "remanence:", line_nb++, NULL);



  for (i=0; i<ETH_LINE_MAX; i++)
    {
    group = NULL;
    for (j=0; j<ETH_TYPE_MAX; j++)
      {
      k = i*ETH_TYPE_MAX + j;
      rad[k] = gtk_radio_button_new_with_label(group,lib[j]);
      group  = gtk_radio_button_get_group(GTK_RADIO_BUTTON(rad[k]));
      if (cust->eth_table[i].endp_type == j+endp_type_none)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rad[k]),TRUE);
      }
    }
  for (i=0; i<ETH_LINE_MAX; i++)
    {
    flags_eth_check_button(grid, &line_nb, i, rad);
    }

  gtk_container_set_border_width(GTK_CONTAINER(grid), ETH_TYPE_MAX);

  entry_startup_env = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(entry_startup_env), cust->startup_env);
  append_grid(grid, entry_startup_env, "Setup Env:", line_nb++, NULL);

  entry_vmount = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(entry_vmount), cust->vmount);
  append_grid(grid, entry_vmount, "Vmount:", line_nb++, NULL);

  if (!strcmp(cust->brandtype, "brandzip"))
    {
    gtk_entry_set_text(GTK_ENTRY(g_entry_image), cust->zip_image);
    append_grid(grid, g_entry_image, "none", line_nb++, g_brandtype_label);
    append_grid(grid, g_image_menu, "Choice:", line_nb++, NULL);
    setup_list_choices_zip();
    }
  else
    {
    gtk_entry_set_text(GTK_ENTRY(g_entry_image), cust->cvm_image);
    append_grid(grid, g_entry_image, "none", line_nb++, g_brandtype_label);
    append_grid(grid, g_image_menu, "Choice:", line_nb++, NULL);
    setup_list_choices_cvm();
    }

  gtk_box_pack_start(
        GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(g_custom_dialog))),
        grid, TRUE, TRUE, 0);
  gtk_widget_show_all(g_custom_dialog);
  response = gtk_dialog_run(GTK_DIALOG(g_custom_dialog));
  
  if (response == GTK_RESPONSE_NONE)
    {
    g_custom_dialog = NULL;
    menu_choice_cnt();
    }
  else
    {
    update_cust(cust, entry_name, entry_startup_env, entry_vmount);
    gtk_widget_destroy(g_custom_dialog);
    g_custom_dialog = NULL;
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void get_custom_cnt(t_custom_cnt **cust_vm)
{
  static t_custom_cnt cust;
  *cust_vm = &cust;
  memcpy(&cust, &g_custom_cnt, sizeof(t_custom_cnt));
  if (!strcmp(g_custom_cnt.name, "Cnt"))
    {
    g_custom_cnt.current_number += 1;
    sprintf(cust.name, "Cnt%d", g_custom_cnt.current_number);
    }
  else
    sprintf(cust.name, "%s", g_custom_cnt.name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void menu_choice_cnt(void)
{
  custom_vm_dialog(&g_custom_cnt);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void menu_dialog_cnt_init(void)
{
  int i;
  g_brandtype_label = gtk_label_new("brandzip:");
  memset(&g_custom_cnt, 0, sizeof(t_custom_cnt)); 
  strcpy(g_custom_cnt.brandtype, "brandzip");
  strcpy(g_custom_cnt.name, "Cnt");
  strcpy(g_custom_cnt.startup_env, "NODE_ID=1 CLOONIX=great");
  strcpy(g_custom_cnt.vmount, " ");
  strcpy(g_custom_cnt.zip_image, "zipfrr.zip");
  strcpy(g_custom_cnt.cvm_image, "cvm_rootfs");
  g_custom_cnt.nb_tot_eth = 3;
  for (i=0; i<g_custom_cnt.nb_tot_eth; i++)
    g_custom_cnt.eth_table[i].endp_type = endp_type_eths;
  g_custom_dialog = NULL;
}
/*--------------------------------------------------------------------------*/



