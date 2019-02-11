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
#include <string.h>
#include <gtk/gtk.h>
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "connect_cloonix.h"
#include "cloonix_conf_info.h"
#include "store.h"
#include "gui.h"
#include "main.h"
#include "header_sock.h"
#include "stats.h"

int file_exists_exec(char *path);

typedef struct t_grid_ident
{
  char net_name[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
} t_grid_ident;

static GtkWidget *g_main_window;
static GtkWidget *g_tree_view;

/****************************************************************************/
static void close_and_fork(char *cmd)
{
  int i;
  if (fork() == 0)
    {
    for (i=0; i<1024; i++)
      close(i);
    clownix_timeout_del_all();
    gtk_main_quit();
    daemon(0,0);
    system(cmd);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int get_addr_and_passwd(char *net_name, char **doors, char **passwd)
{
  int result = -1;
  t_cloonix_conf_info *cnf;
  int i, nb_cloonix_servers;
  *doors = NULL;
  *passwd = NULL;
  cloonix_conf_info_get_all(&nb_cloonix_servers, &cnf);
  for (i=0; i<nb_cloonix_servers; i++)
    {
    if (!strcmp(cnf[i].name, net_name))
      {
      result = 0;
      *doors = cnf[i].doors;
      *passwd = cnf[i].passwd;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void launch_cloonix_ssh(char *net_name, char *name)
{
  char *doors, *passwd, *binterm;
  char cmd[2*MAX_PATH_LEN];
  memset(cmd, 0, 2*MAX_PATH_LEN);
  binterm = get_urxvt_terminal_bin();
  if (strlen(binterm) == 0)
    KERR("%s %s", net_name, name);
  else if (get_addr_and_passwd(net_name, &doors, &passwd))
    KERR("%s %s", net_name, name);
  else
    {
    snprintf(cmd, 2*MAX_PATH_LEN-1, 
             "%s -T %s/%s -e /bin/bash -c \"%s %s %s %s\"", 
                                                binterm, net_name, name,
                                                get_cloonix_ssh_bin(),
                                                doors, passwd, name);
    close_and_fork(cmd);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void launch_spice_desktop(char *net_name, char *name)
{
  char *doors, *passwd, *binspice, *work_dir;
  char cmd[2*MAX_PATH_LEN];
  int vm_id = store_get_vm_id(net_name, name);

  memset(cmd, 0, 2*MAX_PATH_LEN);
  binspice = get_cloonix_spice_bin();
  work_dir = get_cloonix_work_dir(net_name);
  if (!work_dir)
    KERR("%s %s", net_name, name);
  else if (vm_id == 0)
    KERR("%s %s", net_name, name);
  else if (strlen(binspice) == 0) 
    KERR("%s %s", net_name, name);
  else if (get_addr_and_passwd(net_name, &doors, &passwd))
    KERR("%s %s", net_name, name);
  else
    {
    snprintf(cmd, 2*MAX_PATH_LEN-1, 
    "%s --title=%s/%s -d %s -c %s/vm/vm%d/%s -w %s", 
    binspice, net_name, name, doors,
    work_dir, vm_id, SPICE_SOCK, passwd);
KERR("%s", cmd);
    close_and_fork(cmd);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void kill_vm_confirm(char *net_name, char *name)
{
  char title[2*MAX_NAME_LEN];
  char cmd[2*MAX_PATH_LEN];
  GtkWidget *dialog;
  int resp;
  memset(title, 0, 2*MAX_NAME_LEN);
  memset(cmd, 0, 2*MAX_PATH_LEN);
  snprintf(title, 2*MAX_NAME_LEN-1, "Kill %s/%s", net_name, name);
  dialog = gtk_dialog_new_with_buttons (title,
                                        GTK_WINDOW(g_main_window),
                                        GTK_DIALOG_MODAL |
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        "Kill", GTK_RESPONSE_ACCEPT,
                                        "Abort", GTK_RESPONSE_REJECT,
                                        NULL);
  gtk_widget_show_all(dialog);
  resp = gtk_dialog_run(GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
  if (resp == GTK_RESPONSE_ACCEPT)
    {
    snprintf(cmd, 2*MAX_PATH_LEN - 1, "%s %s %s del kvm %s", 
                                      get_cloonix_cli_bin(),
                                      get_cloonix_config_file(),
                                      net_name, name);
    close_and_fork(cmd);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void insert_lb(GtkWidget *grid, char *txt, int idx)
{
  GtkWidget *lb = gtk_label_new(txt);
  gtk_grid_attach(GTK_GRID(grid), lb, 0, idx, 1, 1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static GtkWidget *build_sysinfo_grid(GtkWidget **gval)
{
  int i;
  GtkWidget *grid = NULL;
  grid = gtk_grid_new();
  for (i=0;i<4; i++)
    gtk_grid_insert_column(GTK_GRID(grid), i);
  for (i=0;i<sysdf_max; i++)
    {
    gtk_grid_insert_row(GTK_GRID(grid), i);
    if (i == sysdf_df)
      gtk_grid_attach(GTK_GRID(grid), gval[i], 0, i, 4, 1);
    else
      gtk_grid_attach(GTK_GRID(grid), gval[i], 1, i, 1, 1);
    }
  insert_lb(grid, "time_ms    ", sysdf_time_ms);
  insert_lb(grid, "uptime     ", sysdf_uptime);
  insert_lb(grid, "load1      ", sysdf_load1);
  insert_lb(grid, "load5      ", sysdf_load5);
  insert_lb(grid, "load15     ", sysdf_load15);
  insert_lb(grid, "totalram   ", sysdf_totalram);
  insert_lb(grid, "freeram    ", sysdf_freeram);
  insert_lb(grid, "cachedram  ", sysdf_cachedram);
  insert_lb(grid, "sharedram  ", sysdf_sharedram);
  insert_lb(grid, "bufferram  ", sysdf_bufferram);
  insert_lb(grid, "totalswap  ", sysdf_totalswap);
  insert_lb(grid, "freeswap   ", sysdf_freeswap);
  insert_lb(grid, "procs      ", sysdf_procs);
  insert_lb(grid, "totalhigh  ", sysdf_totalhigh);
  insert_lb(grid, "freehigh   ", sysdf_freehigh);
  insert_lb(grid, "mem_unit   ", sysdf_mem_unit);
  insert_lb(grid, "process_utime ", sysdf_process_utime);
  insert_lb(grid, "process_stime ", sysdf_process_stime);
  insert_lb(grid, "process_cutime", sysdf_process_cutime);
  insert_lb(grid, "process_cstime", sysdf_process_cstime);
  insert_lb(grid, "process_rss   ", sysdf_process_rss);
  return grid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void grid_destroy(GtkWidget *ignored, gpointer data)
{
  t_grid_ident *grid_ident = (t_grid_ident *) data;
  stats_free_grid_var(grid_ident->net_name, grid_ident->name);
  clownix_free(data, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void launch_sysinfo_dump(char *net_name, char *name)
{
  char title[2*MAX_NAME_LEN];
  t_grid_ident *grid_ident;
  GtkWidget *window, *scrolled, *grid;
  GtkWidget **gl = stats_alloc_grid_var(net_name, name);
  snprintf(title, 2*MAX_NAME_LEN - 1, "%s/%s", net_name, name);
  if (gl)
    {
    grid_ident = (t_grid_ident *) clownix_malloc(sizeof(t_grid_ident), 7);
    memset(grid_ident, 0, sizeof(t_grid_ident));
    strncpy(grid_ident->net_name, net_name, MAX_NAME_LEN-1);
    strncpy(grid_ident->name, name, MAX_NAME_LEN-1);
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    g_signal_connect (window, "destroy", G_CALLBACK(grid_destroy), grid_ident);
    scrolled = gtk_scrolled_window_new(NULL, NULL);
    grid = build_sysinfo_grid(gl); 
    gtk_window_set_default_size (GTK_WINDOW (window), 300, 500);
    gtk_window_set_title (GTK_WINDOW (window), title);
    gtk_container_add (GTK_CONTAINER(scrolled), grid);
    gtk_container_add(GTK_CONTAINER(window), scrolled);
    gtk_widget_show_all(window);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void kill_net_confirm(char *net_name)
{
  char title[2*MAX_NAME_LEN];
  char cmd[MAX_PATH_LEN + MAX_NAME_LEN];
  GtkWidget *dialog;
  int resp;
  memset(title, 0, 2*MAX_NAME_LEN);
  memset(cmd, 0, MAX_PATH_LEN + MAX_NAME_LEN);
  snprintf(title, 2*MAX_NAME_LEN-1, "Kill %s", net_name);
  dialog = gtk_dialog_new_with_buttons (title,
                                        GTK_WINDOW(g_main_window),
                                        GTK_DIALOG_MODAL | 
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        "Kill", GTK_RESPONSE_ACCEPT,
                                        "Abort", GTK_RESPONSE_REJECT,
                                        NULL);
  gtk_widget_show_all(dialog);
  resp = gtk_dialog_run(GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
  if (resp == GTK_RESPONSE_ACCEPT)
    {
    snprintf(cmd, MAX_PATH_LEN + MAX_NAME_LEN-1, "%s %s %s kil", 
                                                 get_cloonix_cli_bin(),
                                                 get_cloonix_config_file(),
                                                 net_name);
    close_and_fork(cmd);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void collapse_tree_view(void)
{
  gtk_tree_view_collapse_all(GTK_TREE_VIEW(g_tree_view));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void double_click(GtkTreeView        *view,
                          GtkTreePath        *path,
                          GtkTreeViewColumn  *col)
{
  int result;
  GtkTreeIter iter;
  GtkTreeModel *model;
  char *str;
  char net_name[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  char cmd[2*MAX_PATH_LEN];
  model = gtk_tree_view_get_model(view);
  if (gtk_tree_model_get_iter(model, &iter, path))
    {
    str = gtk_tree_model_get_string_from_iter(model, &iter);
    result = find_names_with_str_iter(str, net_name, name);
    switch(result)
      {
      case type_network:
      case type_vm_title:
      case type_sat_title:
      case type_lan_title:
      case type_vm:
        if (gtk_tree_view_row_expanded(view, path))
          gtk_tree_view_collapse_row (view, path);
        else
          gtk_tree_view_expand_row (view, path, FALSE);
        break;
      case type_vm_ssh:
        launch_cloonix_ssh(net_name, name);
        break;
      case type_vm_kil:
        kill_vm_confirm(net_name, name);
        break;
      case type_vm_sys:
        launch_sysinfo_dump(net_name, name);
        break;
      case type_sat:
        break;
      case type_lan:
        break;
      case type_graph:
        memset(cmd, 0, 2*MAX_PATH_LEN);
        snprintf(cmd, 2*MAX_PATH_LEN-1, "%s %s %s",  
                                        get_cloonix_gui_bin(),
                                        get_cloonix_config_file(),
                                        net_name);
        close_and_fork(cmd);
        break;
      case type_kill:
        kill_net_confirm(net_name);
        break;

      case type_vm_spice:
        launch_spice_desktop(net_name, name);
        break;

      default:
        KOUT("%s", str);
      }

    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static GtkWidget *tree_setup(void)
{
  GtkCellRenderer  *renderer = gtk_cell_renderer_text_new();
  GtkWidget        *view = gtk_tree_view_new();
  GtkTreeStore *store = store_get();
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (view),
                                              -1, NULL, renderer,
                                              "text", 0, NULL);
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (view),
                                              -1, NULL, renderer,
                                              "text", 1, NULL);
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW (view), FALSE);
  gtk_tree_view_set_model(GTK_TREE_VIEW (view), GTK_TREE_MODEL(store));
  g_signal_connect(view, "row-activated", G_CALLBACK(double_click), NULL);
  return view;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void gui_init(void)
{
  GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  g_tree_view = tree_setup();
  signal(SIGCHLD, SIG_IGN);
  g_main_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (g_main_window), 300, 200);
  gtk_window_set_title (GTK_WINDOW (g_main_window), "Hyperzor");
  g_signal_connect (g_main_window,"destroy",G_CALLBACK(gtk_main_quit),NULL);
  gtk_container_add (GTK_CONTAINER(scrolled), g_tree_view);
  gtk_box_pack_start(GTK_BOX (vbox), scrolled, TRUE, TRUE, 10);
  gtk_container_add(GTK_CONTAINER(g_main_window), vbox);
  gtk_widget_show_all(g_main_window);
}
/*--------------------------------------------------------------------------*/
