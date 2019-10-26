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
#include <unistd.h>
#include <gtk/gtk.h>
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "connect_cloonix.h"
#include "cloonix_conf_info.h"
#include "store.h"
#include "gui.h"
#include "header_sock.h"
#include "stats.h"


static char g_cloonix_root_tree[MAX_PATH_LEN];
static char g_cloonix_gui_bin[MAX_PATH_LEN];
static char g_cloonix_cli_bin[MAX_PATH_LEN];
static char g_cloonix_ssh_bin[MAX_PATH_LEN];
static char g_cloonix_spice_bin[MAX_PATH_LEN];
static char g_cloonix_config_file[MAX_PATH_LEN];
static char g_urxvt_terminal_bin[MAX_PATH_LEN];
int file_exists_exec(char *path);

/*****************************************************************************/
int file_exists_exec(char *path)
{
  int err, result = 0;
  err = access(path, X_OK);
  if (!err)
    result = 1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_cloonix_root_tree(void)
{
  return g_cloonix_root_tree;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_cloonix_gui_bin(void)
{
  return g_cloonix_gui_bin;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_cloonix_cli_bin(void)
{
  return g_cloonix_cli_bin;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
char *get_cloonix_ssh_bin(void)
{
  return g_cloonix_ssh_bin;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_cloonix_spice_bin(void)
{
  return g_cloonix_spice_bin;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
char *get_cloonix_config_file(void)
{
  return g_cloonix_config_file;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_urxvt_terminal_bin(void)
{
  return g_urxvt_terminal_bin;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
gboolean refresh_request_timeout (gpointer data)
{
  clownix_timer_beat();
  return TRUE;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void init_local_cloonix_paths(char *curdir, char *callbin, char *conf)
{
  int err;
  char path[2*MAX_PATH_LEN];
  char *ptr;
  memset(g_cloonix_root_tree, 0, MAX_PATH_LEN);
  memset(path, 0, 2*MAX_PATH_LEN);
  if (callbin[0] == '/')
    snprintf(path, 2*MAX_PATH_LEN-1, "%s", callbin);
  else
    snprintf(path, 2*MAX_PATH_LEN-1, "%s/%s", curdir, callbin);
  ptr = strrchr(path, '/');
  if (!ptr)
    KOUT("%s", path);
  *ptr = 0;
  ptr = strrchr(path, '/');
  if (!ptr)
    KOUT("%s", path);
  *ptr = 0;
  ptr = strrchr(path, '/');
  if (!ptr)
    KOUT("%s", path);
  *ptr = 0;
  path[MAX_PATH_LEN-1] = 0;
  strncpy(g_cloonix_root_tree, path, MAX_PATH_LEN-1);
  snprintf(path, 2*MAX_PATH_LEN-1,
           "%s/client/hyperzor/hyperzor", g_cloonix_root_tree);
  path[MAX_PATH_LEN-1] = 0;
  err = access(path, X_OK);
  if (err)
    {
    printf("\nBAD INSTALL, NOT FOUND:\n%s\n", path);
    KOUT("%s %d", path, err);
    }
  snprintf(path, 2*MAX_PATH_LEN-1,
           "%s/client/cairo_canvas/cloonix_gui", g_cloonix_root_tree);
  path[MAX_PATH_LEN-1] = 0;
  strcpy(g_cloonix_gui_bin, path);
  err = access(g_cloonix_gui_bin, X_OK);
  if (err)
    {
    printf("\nBAD INSTALL, NOT FOUND:\n%s\n", g_cloonix_gui_bin);
    KOUT("%s %d", g_cloonix_gui_bin, err);
    }
  snprintf(path, 2*MAX_PATH_LEN-1,
           "%s/client/ctrl/cloonix_ctrl", g_cloonix_root_tree);
  path[MAX_PATH_LEN-1] = 0;
  strcpy(g_cloonix_cli_bin, path);
  err = access(g_cloonix_cli_bin, X_OK);
  if (err)
    {
    printf("\nBAD INSTALL, NOT FOUND:\n%s\n", g_cloonix_cli_bin);
    KOUT("%s %d", g_cloonix_cli_bin, err);
    }
  snprintf(path, 2*MAX_PATH_LEN-1,
           "%s/common/agent_dropbear/agent_bin/dropbear_cloonix_ssh",
           g_cloonix_root_tree);
  path[MAX_PATH_LEN-1] = 0;
  strcpy(g_cloonix_ssh_bin, path);
  err = access(g_cloonix_ssh_bin, X_OK);
  if (err)
    {
    printf("\nBAD INSTALL, NOT FOUND:\n%s\n", g_cloonix_ssh_bin);
    KOUT("%s %d", g_cloonix_ssh_bin, err);
    }

  snprintf(path, 2*MAX_PATH_LEN-1,
           "%s/common/spice/spice_lib/bin/spicy", g_cloonix_root_tree);
  path[MAX_PATH_LEN-1] = 0;
  strcpy(g_cloonix_spice_bin, path);
  err = access(g_cloonix_spice_bin, X_OK);
  if (err)
    {
    printf("\nBAD INSTALL, NOT FOUND:\n%s\n", g_cloonix_spice_bin);
    KOUT("%s %d", g_cloonix_spice_bin, err);
    }

  strncpy(g_cloonix_config_file, conf, MAX_PATH_LEN-1);
  err = access(g_cloonix_config_file, F_OK);
  if (err)
    {
    printf("\nBAD INSTALL, NOT FOUND:\n%s\n", g_cloonix_config_file);
    KOUT("%s %d", g_cloonix_config_file, err);
    }
  err = access("/usr/bin/urxvt", X_OK);
  if (!err)
    strncpy(g_urxvt_terminal_bin, "/usr/bin/urxvt", MAX_PATH_LEN-1);
  else
    KERR("%d", err);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void store_evt_net_exists(char *net_name, int exists)
{
  if (exists)
    {
    store_net_alloc(net_name);
    stats_net_alloc(net_name);
    }
  else
    {
    store_net_free(net_name);
    stats_net_free(net_name);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void store_evt_vm_exists(char *net_name, char *name,
                          int nb_eth, int vm_id, int exists)
{
  if (exists)
    {
    store_vm_alloc(net_name, name, nb_eth, vm_id);
    stats_item_alloc(net_name, name);
    }
  else
    {
    store_vm_free(net_name, name);
    stats_item_free(net_name, name);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void store_evt_sat_exists(char *net_name, char *name, int type, int exists)
{
  if (exists)
    {
    store_sat_alloc(net_name, name, type);
    stats_item_alloc(net_name, name);
    }
  else
    {
    store_sat_free(net_name, name);
    stats_item_free(net_name, name);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void store_evt_lan_exists(char *net_name, char *name, int exists)
{
  if (exists)
    store_lan_alloc(net_name, name);
  else
    store_lan_free(net_name, name);
}

/****************************************************************************/
int main(int argc, char *argv[])
{
  char current_directory[MAX_PATH_LEN];
  memset(g_cloonix_root_tree, 0, MAX_PATH_LEN);
  memset(g_cloonix_gui_bin, 0, MAX_PATH_LEN);
  memset(g_cloonix_config_file, 0, MAX_PATH_LEN);
  memset(g_urxvt_terminal_bin, 0, MAX_PATH_LEN);
  if (argc < 2)
    {
    printf("\nPARAM NEEDED, try:\n\n");
    printf("%s /etc/cloonix/cloonix_conf\n\n", argv[0]);
    KOUT("%d", argc);
    }
  memset(current_directory, 0, MAX_PATH_LEN);
  if (!getcwd(current_directory, MAX_PATH_LEN-1))
    KOUT(" ");

  init_local_cloonix_paths(current_directory, argv[0], argv[1]);
  gtk_init(NULL, NULL);
  connect_cloonix_init(g_cloonix_config_file, 
                       store_evt_net_exists,
                       store_evt_vm_exists,
                       store_evt_sat_exists,
                       store_evt_lan_exists,
                       stats_endp,
                       stats_sysinfo);
  g_timeout_add(100, refresh_request_timeout, (gpointer) NULL);
  store_init();
  gui_init();
  daemon(0,0);
  gtk_main();
  return 0;
}
/*---------------------------------------------------------------------------*/
                                                  
