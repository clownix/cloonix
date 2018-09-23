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
#include "menu_dialog_a2b.h"

GtkWidget *get_main_window(void);

static t_custom_a2b g_custom_a2b;


/*****************************************************************************/
t_custom_a2b *get_custom_a2b (void)
{
  static t_custom_a2b cust;
  memcpy(&cust, &g_custom_a2b, sizeof(t_custom_a2b));
  if (g_custom_a2b.append_number)
    {
    g_custom_a2b.number += 1;
    sprintf(cust.name, "%s%d", g_custom_a2b.name, g_custom_a2b.number);
    }
  else
    {
    sprintf(cust.name, "%s", g_custom_a2b.name);
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
  if (gtk_toggle_button_get_active(togglebutton))
    g_custom_a2b.append_number = 1;
  else
    g_custom_a2b.append_number = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void update_a2b_cust(t_custom_a2b *cust, 
                            GtkWidget *name, 
                            GtkWidget *numadd)
{
  char *tmp;
  tmp = (char *) gtk_entry_get_text(GTK_ENTRY(name));
  memset(cust->name, 0, MAX_NAME_LEN);
  strncpy(cust->name, tmp, MAX_NAME_LEN-1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void custom_a2b_dialog(t_custom_a2b *cust)
{
  int response, line_nb = 0;
  GtkWidget *grid, *dialog, *numadd,  *entry_name;
  GtkWidget *parent = get_main_window();

  grid = gtk_grid_new();
  gtk_grid_insert_column(GTK_GRID(grid), 0);
  gtk_grid_insert_column(GTK_GRID(grid), 1);

  dialog = gtk_dialog_new_with_buttons ("Custom your a2b",
                                        GTK_WINDOW (parent),
                                        GTK_DIALOG_MODAL,
                                        "OK",GTK_RESPONSE_ACCEPT,
                                        NULL);
  gtk_window_set_default_size(GTK_WINDOW(dialog), 300, 20);
  entry_name = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(entry_name), cust->name);
  append_grid(grid, entry_name, "name:", line_nb++);
  numadd = gtk_check_button_new_with_label("add number at end");
  if (g_custom_a2b.append_number)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(numadd), TRUE);
  else
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(numadd), FALSE);
  g_signal_connect(numadd, "toggled", G_CALLBACK(numadd_toggle), NULL);
  append_grid(grid, numadd, "Append:", line_nb++);
  gtk_container_set_border_width(GTK_CONTAINER(grid), 5);
  gtk_box_pack_start(
    GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), 
    grid, TRUE, TRUE, 0);
  gtk_widget_show_all (dialog);
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  if (response == GTK_RESPONSE_ACCEPT)
    update_a2b_cust(cust, entry_name, numadd);
  gtk_widget_destroy(dialog);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void menu_choice_a2b_params(void)
{
  custom_a2b_dialog(&g_custom_a2b);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void menu_dialog_a2b_init(void)
{
  memset(&g_custom_a2b, 0, sizeof(t_custom_a2b)); 
  strncpy(g_custom_a2b.name, "a2b", MAX_NAME_LEN-1);
  g_custom_a2b.append_number = 1;
}
/*--------------------------------------------------------------------------*/
