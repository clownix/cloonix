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
#include "menu_dialog_tap.h"

GtkWidget *get_main_window(void);

GtkWidget *g_numadd, *g_entry_name;
static t_custom_tap g_custom_tap;


/*****************************************************************************/
t_custom_tap *get_custom_tap (void)
{
  static t_custom_tap cust;
  memcpy(&cust, &g_custom_tap, sizeof(t_custom_tap));
  if (g_custom_tap.append_number)
    {
    if (g_custom_tap.mutype == endp_type_tap)
      {
      g_custom_tap.number += 1;
      sprintf(cust.name, "%s%d", g_custom_tap.name, g_custom_tap.number);
      }
    else
      sprintf(cust.name, "%s", g_custom_tap.name);
    cust.mutype = g_custom_tap.mutype;
    }
  else
    {
    sprintf(cust.name, "%s", g_custom_tap.name);
    cust.mutype = g_custom_tap.mutype;
    }
  return (&cust); 
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
static void numadd_toggle(GtkToggleButton *togglebutton, gpointer user_data)
{
  if (g_custom_tap.mutype != endp_type_tap)
    {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(togglebutton), FALSE);
    }
  else if (gtk_toggle_button_get_active(togglebutton))
    g_custom_tap.append_number = 1;
  else
    g_custom_tap.append_number = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rad_tap_type_cb(GtkWidget *check, gpointer user_data)
{
  unsigned long tap_type = (unsigned long) user_data;
  g_custom_tap.mutype = tap_type;
  if (g_custom_tap.mutype == endp_type_tap)
    {
    if (g_custom_tap.append_number)
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(g_numadd), TRUE);
    else
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(g_numadd), FALSE);
    }
  else
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(g_numadd), FALSE);

  if (g_custom_tap.mutype == endp_type_tap)
    gtk_entry_set_text(GTK_ENTRY(g_entry_name), g_custom_tap.name);
  else if (g_custom_tap.mutype == endp_type_raw)
    gtk_entry_set_text(GTK_ENTRY(g_entry_name), "eth0");
  else if (g_custom_tap.mutype == endp_type_wif)
    gtk_entry_set_text(GTK_ENTRY(g_entry_name), "wlan0");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void flags_tap_check_button(GtkWidget *grid, int *line_nb)
{
  unsigned long int i;
  GtkWidget *rad[4];
  char *lib[5] = {"tap", "raw", "wif"}; 
  unsigned long int endp_type[3] = {endp_type_tap, endp_type_raw, 
                                     endp_type_wif};
  GtkWidget *hbox;
  GSList *group = NULL;
  char label[MAX_NAME_LEN];
  memset(label, 0, MAX_NAME_LEN);
  sprintf(label, "tap_type");

  for (i=0; i<3; i++)
    {
    rad[i] = gtk_radio_button_new_with_label(group, lib[i]);
    group  = gtk_radio_button_get_group(GTK_RADIO_BUTTON(rad[i]));
    }
  if (g_custom_tap.mutype == endp_type_tap)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rad[0]),TRUE);
  else if (g_custom_tap.mutype == endp_type_raw)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rad[1]),TRUE);
  else if (g_custom_tap.mutype == endp_type_wif)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rad[2]),TRUE);
  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  for (i=0; i<3; i++)
    {
    gtk_box_pack_start(GTK_BOX(hbox), rad[i], TRUE, TRUE, 10);
    g_signal_connect(G_OBJECT(rad[i]),"clicked",
                       (GCallback) rad_tap_type_cb, 
                       (gpointer) endp_type[i]);
    }
  append_grid(grid, hbox, "type:", (*line_nb)++);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void update_tap_cust(GtkWidget *name) 
{
  char *tmp;
  tmp = (char *) gtk_entry_get_text(GTK_ENTRY(name));
  memset(g_custom_tap.name, 0, MAX_NAME_LEN);
  strncpy(g_custom_tap.name, tmp, MAX_NAME_LEN-1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void custom_tap_dialog(void)
{
  int response, line_nb = 0;
  GtkWidget *grid, *dialog, *numadd,  *entry_name;
  GtkWidget *parent = get_main_window();

  grid = gtk_grid_new();
  gtk_grid_insert_column(GTK_GRID(grid), 0);
  gtk_grid_insert_column(GTK_GRID(grid), 1);

  dialog = gtk_dialog_new_with_buttons ("Custom your tap",
                                        GTK_WINDOW (parent),
                                        GTK_DIALOG_MODAL,
                                        "OK",GTK_RESPONSE_ACCEPT,
                                        NULL);
  gtk_window_set_default_size(GTK_WINDOW(dialog), 300, 20);
  entry_name = gtk_entry_new();
  g_entry_name = entry_name;

  if (g_custom_tap.mutype == endp_type_tap)
    gtk_entry_set_text(GTK_ENTRY(g_entry_name), g_custom_tap.name);
  else if (g_custom_tap.mutype == endp_type_raw)
    gtk_entry_set_text(GTK_ENTRY(g_entry_name), "eth0");
  else if (g_custom_tap.mutype == endp_type_wif)
    gtk_entry_set_text(GTK_ENTRY(g_entry_name), "wlan0");

  append_grid(grid, entry_name, "name:", line_nb++);
  numadd = gtk_check_button_new_with_label("add number at end");
  g_numadd = numadd;
  if (g_custom_tap.mutype == endp_type_tap)
    {
    if (g_custom_tap.append_number)
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(g_numadd), TRUE);
    else
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(g_numadd), FALSE);
    }
  else
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(g_numadd), FALSE);

  g_signal_connect(numadd, "toggled", G_CALLBACK(numadd_toggle), NULL);
  append_grid(grid, numadd, "Append:", line_nb++);
  flags_tap_check_button(grid, &line_nb);
  gtk_container_set_border_width(GTK_CONTAINER(grid), 5);
  gtk_box_pack_start(
    GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), 
    grid, TRUE, TRUE, 0);
  gtk_widget_show_all(dialog);
  response = gtk_dialog_run(GTK_DIALOG(dialog));
  if (response == GTK_RESPONSE_ACCEPT)
    update_tap_cust(entry_name);
  gtk_widget_destroy(dialog);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void menu_choice_tap_params(void)
{
  custom_tap_dialog();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void menu_dialog_tap_init(void)
{
  memset(&g_custom_tap, 0, sizeof(t_custom_tap)); 
  strncpy(g_custom_tap.name, "tap", MAX_NAME_LEN-1);
  g_custom_tap.append_number = 1;
  g_custom_tap.mutype = endp_type_tap;
}
/*--------------------------------------------------------------------------*/
