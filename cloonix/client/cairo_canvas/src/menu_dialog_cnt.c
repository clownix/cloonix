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


static void custom_vm_dialog_create(void);
void kvm_distant_brandtype_cb(void);
void menu_choice_kvm(void);
char *get_brandtype_image(char *type);
char set_brandtype_image(char *type, char *image);
char *get_brandtype_type(void);
void global_brandtype_cb(GtkWidget *check, gpointer user_data);

GtkWidget *get_main_window(void);
static t_custom_cnt g_custom_cnt;
static GtkWidget *g_custom_dialog;
static GtkWidget *g_brandtype_label;
static GtkWidget *g_entry_image;
static t_slowperiodic g_bulzip[MAX_BULK_FILES];
static int g_nb_bulzip;
static t_slowperiodic g_bulcvm[MAX_BULK_FILES];
static int g_nb_bulcvm;

static int g_custom_dialog_change;

typedef struct t_cb_endp_type
{
  int rank;
  int endp_type;
  GtkWidget **rad;
} t_cb_endp_type;

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
      
/****************************************************************************/
static void img_get(GtkWidget *check, gpointer data)
{       
  char *name = (char *) data;
  set_brandtype_image(NULL, name);
  if (g_entry_image)
    gtk_entry_set_text(GTK_ENTRY(g_entry_image), get_brandtype_image(NULL));
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
static GtkWidget *setup_list_choices(void)
{
  GtkWidget *menu = NULL;
  if (!strcmp(get_brandtype_type(), "brandzip"))
    {
    menu = get_bulk_choice(g_nb_bulzip, g_bulzip, get_brandtype_image(NULL));
    }
  else if (!strcmp(get_brandtype_type(), "brandcvm"))
    {
    menu = get_bulk_choice(g_nb_bulcvm, g_bulcvm, get_brandtype_image(NULL));
    }
  else if (!strcmp(get_brandtype_type(), "brandkvm"))
    {
    }
  else
    KOUT("ERROR %s", get_brandtype_type());
  return menu;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void cnt_update_brandtype_image(void)
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
      set_brandtype_image("brandzip", "zipfrr.zip");
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
      set_brandtype_image("brandcvm", "alpine");
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cnt_distant_brandtype_cb(void)
{
  if (!g_custom_dialog)
    KERR("ERROR DISTANT CB");
  else
    {
    g_custom_dialog_change = 1;
    gtk_dialog_response(GTK_DIALOG(g_custom_dialog), GTK_RESPONSE_NONE);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cnt_brandtype_cb(void)
{
  cnt_update_brandtype_image();
  if (!g_custom_dialog)
    kvm_distant_brandtype_cb();
  else
    {
    g_custom_dialog_change = 1;
    gtk_dialog_response(GTK_DIALOG(g_custom_dialog), GTK_RESPONSE_NONE);
    }
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
static void update_cust_brandzip(t_custom_cnt *cust,
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
static void update_cust_brandcvm(t_custom_cnt *cust, GtkWidget *entry_name)
{
  char *tmp;
  tmp = (char *) gtk_entry_get_text(GTK_ENTRY(entry_name));
  memset(cust->name, 0, MAX_NAME_LEN);
  strncpy(cust->name, tmp, MAX_NAME_LEN-1);
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
                       G_CALLBACK(endp_type_cb),
                       (gpointer) cb_endp_type[i]);
    }
  append_grid(grid, hbox, title, (*line_nb)++, NULL);
  g_signal_connect (G_OBJECT (hbox), "destroy",
                    G_CALLBACK(free_endp_type), cb_endp_type);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void restart_custom_vm_dialog_create(void *data)
{
  if (!strcmp(get_brandtype_type(), "brandkvm"))
    menu_choice_kvm();
  else
    custom_vm_dialog_create();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void custom_vm_dialog_create(void)
{
  int i, j, k, response, line_nb = 0;
  unsigned long uli;
  GSList *group = NULL;
  GSList *brandgroup = NULL;
  GtkWidget *entry_name, *entry_startup_env, *grid, *parent;
  GtkWidget *is_persistent, *image_menu, *image_sub;
  GtkWidget *entry_vmount, *rad[ETH_LINE_MAX * ETH_TYPE_MAX];
  GtkWidget *brandtype[BRANDTYPE_NB_MAX];
  GtkWidget *hbox_brandtype;
  char *brandlib[BRANDTYPE_NB_MAX] = {"brandkvm", "brandzip", "brandcvm"};
  char *lib[ETH_TYPE_MAX] = {"n", "s", "v"};

  if (g_custom_dialog)
    {
    KERR("ERROR REQUEST MENU ON MENU ALIVE");
    return;
    }
  
  image_menu = gtk_menu_button_new();
  g_entry_image = gtk_entry_new();
  cnt_update_brandtype_image();

  grid = gtk_grid_new();
  gtk_grid_insert_column(GTK_GRID(grid), 0);
  gtk_grid_insert_column(GTK_GRID(grid), 1);
  parent = get_main_window();
  g_custom_dialog = gtk_dialog_new_with_buttons ("Custom your vm",
                             GTK_WINDOW (parent), GTK_DIALOG_MODAL,
                             "OK", GTK_RESPONSE_ACCEPT, NULL);
  gtk_window_set_default_size(GTK_WINDOW(g_custom_dialog), 300, 20);


  hbox_brandtype = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  for (i=0; i<BRANDTYPE_NB_MAX; i++)
    {
    brandtype[i] = gtk_radio_button_new_with_label(brandgroup, brandlib[i]);
    brandgroup = gtk_radio_button_get_group(GTK_RADIO_BUTTON(brandtype[i]));
    if (!strcmp(get_brandtype_type(), brandlib[i]))
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(brandtype[i]),TRUE);
    }
  for (uli=0; uli<BRANDTYPE_NB_MAX; uli++)
    {
    gtk_box_pack_start(GTK_BOX(hbox_brandtype), brandtype[uli],TRUE,TRUE,10);
    g_signal_connect(G_OBJECT(brandtype[uli]), "clicked",
                     G_CALLBACK(global_brandtype_cb), (gpointer) uli);
    }
  append_grid(grid, hbox_brandtype, "brandtype", line_nb++, NULL);




  entry_name = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(entry_name), g_custom_cnt.name);
  append_grid(grid, entry_name, "Name:", line_nb++, NULL);


  is_persistent = gtk_check_button_new_with_label("persistent_rootfs");
  if (g_custom_cnt.is_persistent)
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
      if (g_custom_cnt.eth_table[i].endp_type == j+endp_type_none)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rad[k]),TRUE);
      }
    }
  for (i=0; i<ETH_LINE_MAX; i++)
    {
    flags_eth_check_button(grid, &line_nb, i, rad);
    }

  gtk_container_set_border_width(GTK_CONTAINER(grid), ETH_TYPE_MAX);

  if (!strcmp(get_brandtype_type(), "brandzip"))
    {
    entry_startup_env = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry_startup_env), g_custom_cnt.startup_env);
    append_grid(grid, entry_startup_env, "Setup Env:", line_nb++, NULL);
    entry_vmount = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry_vmount), g_custom_cnt.vmount);
    append_grid(grid, entry_vmount, "Vmount:", line_nb++, NULL);
    }

  gtk_entry_set_text(GTK_ENTRY(g_entry_image), get_brandtype_image(NULL));

  g_brandtype_label = gtk_label_new(get_brandtype_type());
  append_grid(grid, g_entry_image, "none", line_nb++, g_brandtype_label);
  append_grid(grid, image_menu, "Choice:", line_nb++, NULL);
  image_sub = setup_list_choices();
  if (image_sub)
    {
    gtk_menu_button_set_popup(GTK_MENU_BUTTON(image_menu), image_sub);
    gtk_widget_show_all(image_sub);
    }
  gtk_box_pack_start(
        GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(g_custom_dialog))),
        grid, TRUE, TRUE, 0);
  gtk_widget_show_all(g_custom_dialog);
  response = gtk_dialog_run(GTK_DIALOG(g_custom_dialog));
  
  if (response == GTK_RESPONSE_NONE)
    {
    if (g_custom_dialog_change)
      clownix_timeout_add(1,restart_custom_vm_dialog_create,NULL,NULL,NULL);
    g_custom_dialog_change = 0;
    }
  else
    {
    if (!strcmp(get_brandtype_type(), "brandzip"))
      update_cust_brandzip(&g_custom_cnt, entry_name,
                           entry_startup_env, entry_vmount);
    else
      update_cust_brandcvm(&g_custom_cnt, entry_name);
    }
  gtk_widget_destroy(g_custom_dialog);
  g_custom_dialog = NULL;
  g_entry_image = NULL;
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
  custom_vm_dialog_create();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void menu_dialog_cnt_init(void)
{
  int i;
  g_custom_dialog_change = 0;
  memset(&g_custom_cnt, 0, sizeof(t_custom_cnt)); 
  strcpy(g_custom_cnt.name, "Cnt");
  strcpy(g_custom_cnt.startup_env, "NODE_ID=1 CLOONIX=great");
  strcpy(g_custom_cnt.vmount, " ");
  g_custom_cnt.nb_tot_eth = 3;
  for (i=0; i<g_custom_cnt.nb_tot_eth; i++)
    g_custom_cnt.eth_table[i].endp_type = endp_type_eths;
  g_custom_dialog = NULL;
}
/*--------------------------------------------------------------------------*/



