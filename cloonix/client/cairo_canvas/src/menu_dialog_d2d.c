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
#include "menu_dialog_d2d.h"
#include "cloonix_conf_info.h"
#include "popup.h"


GtkWidget *get_main_window(void);
static t_custom_d2d g_custom_d2d;


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

/*****************************************************************************/
t_custom_d2d *get_custom_d2d (void)
{
  return (&g_custom_d2d); 
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void update_d2d_cust(t_custom_d2d *cust, GtkWidget *name, 
                            GtkWidget *cloonix_slave)
{
  int rank;
  char *tmp;
  char savname[MAX_NAME_LEN];
  char savslave[MAX_NAME_LEN];
  t_cloonix_conf_info *cnf;

  tmp = (char *) gtk_entry_get_text(GTK_ENTRY(cloonix_slave));
  strncpy(savslave, tmp, MAX_NAME_LEN-1);
  tmp = (char *) gtk_entry_get_text(GTK_ENTRY(name));
  strncpy(savname, tmp, MAX_NAME_LEN-1);
  if ((!strlen(savslave)) || (!strlen(savname)))
    KERR("%s %s", savname, savname);
  else
    {
    cnf = cloonix_cnf_info_get(savslave, &rank);
    if (!cnf)
      insert_next_warning(cloonix_conf_info_get_names(), 1);
    else
      {
      memset(cust, 0, sizeof(t_custom_d2d));
      strncpy(cust->name, savname, MAX_NAME_LEN-1);
      strncpy(cust->dist_cloonix, savslave, MAX_NAME_LEN-1);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void custom_d2d_dialog(t_custom_d2d *cust)
{
  int response, line_nb = 0;
  GtkWidget *grid, *dialog, *parent, *entry_name;
  GtkWidget *entry_cloonix_slave;
  char *slave = cust->dist_cloonix;

  grid = gtk_grid_new();
  gtk_grid_insert_column(GTK_GRID(grid), 0);
  gtk_grid_insert_column(GTK_GRID(grid), 1);

  parent = get_main_window();
  dialog = gtk_dialog_new_with_buttons ("Custom your d2d",
                                        GTK_WINDOW (parent),
                                        GTK_DIALOG_MODAL,
                                        "OK", GTK_RESPONSE_ACCEPT,
                                        NULL);
  gtk_window_set_default_size(GTK_WINDOW(dialog), 450, 30);

  entry_name = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(entry_name), cust->name);
  append_grid(grid, entry_name, "Name of d2d:", line_nb++);

  entry_cloonix_slave = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(entry_cloonix_slave), slave);
  append_grid(grid, entry_cloonix_slave, "Distant cloonix name:", line_nb++);

  gtk_container_set_border_width(GTK_CONTAINER(grid), 5);

  gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                     grid, TRUE, TRUE, 0);
  gtk_widget_show_all(dialog);
  response = gtk_dialog_run (GTK_DIALOG(dialog));
  if (response == GTK_RESPONSE_ACCEPT)
    update_d2d_cust(cust, entry_name, entry_cloonix_slave);
  gtk_widget_destroy(dialog);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void menu_choice_d2d_params(void)
{
  custom_d2d_dialog(&g_custom_d2d);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void menu_dialog_d2d_init(void)
{
  strcpy(g_custom_d2d.name, "d2d_to_mito");
  strcpy(g_custom_d2d.dist_cloonix, "mito");
}
/*--------------------------------------------------------------------------*/
