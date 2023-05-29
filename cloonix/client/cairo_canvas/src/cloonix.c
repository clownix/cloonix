/*****************************************************************************/
/*    Copyright (C) 2006-2023 clownix@clownix.net License AGPL-3             */
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
#include <libcrcanvas.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <net/if.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_consts.h"
#include "bank.h"
#include "interface.h"
#include "doorways_sock.h"
#include "client_clownix.h"
#include "menu_utils.h"
#include "menus.h"
#include "move.h"
#include "popup.h"
#include "main_timer_loop.h"
#include "pid_clone.h"
#include "cloonix.h"
#include "eventfull_eth.h"
#include "layout_rpc.h"
#include "layout_topo.h"
#include "menu_dialog_kvm.h"
#include "file_read_write.h"
#include "cloonix_conf_info.h"
#include "bdplot.h"


void interface_topo_subscribe(void);

void timeout_periodic_work(void *data);


/*--------------------------------------------------------------------------*/
gboolean refresh_request_timeout (gpointer  data);
void topo_set_signals(GtkWidget *window);
GtkWidget *topo_canvas(void);

static int g_cloonix_rank;

static t_topo_clc g_clc;

static int eth_choice = 0;
static GtkWidget *g_main_window;

static int g_i_am_in_cloon;
static char g_i_am_in_cloonix_name[MAX_NAME_LEN];

static gint main_win_x, main_win_y, main_win_width, main_win_height;
static guint main_timeout;
static char g_current_directory[MAX_PATH_LEN];
static char g_doors_client_addr[MAX_PATH_LEN];
static char g_cloonix_root_tree[MAX_PATH_LEN];
static char g_dtach_work_path[MAX_PATH_LEN];
static char g_distant_snf_dir[MAX_PATH_LEN];
static char g_password[MSG_DIGEST_LEN];
static t_cloonix_conf_info *g_cloonix_conf_info;

/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int get_cloonix_rank(void)
{
  return g_cloonix_rank;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
t_cloonix_conf_info *get_own_cloonix_conf_info(void)
{
  return (g_cloonix_conf_info);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_password(void)
{
  return g_password;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int file_exists_exec(char *path)
{
  int err, result = 0;
  err = access(path, X_OK);
  if (!err)
    result = 1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *local_get_cloonix_name(void)
{
  return (g_clc.network);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int inside_cloon(char **name)
{
  *name = g_i_am_in_cloonix_name;
  return g_i_am_in_cloon;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_doors_client_addr(void)
{
  return g_doors_client_addr;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static const char *get_dtach_work_path(void)
{
  return (g_dtach_work_path);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_local_cloonix_tree(void)
{
  return g_cloonix_root_tree;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_distant_cloonix_tree(void)
{
  return g_clc.bin_dir;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char **get_argv_local_xwy(char *name)
{
  static char bin_path[MAX_PATH_LEN];
  static char config[MAX_PATH_LEN];
  static char cmd[MAX_PATH_LEN];
  static char path[MAX_PATH_LEN];
  static char nm[MAX_NAME_LEN];
  static char title[MAX_PATH_LEN];
  static char nemo[MAX_NAME_LEN];
  char *locbintree = get_local_cloonix_tree();
  char *dstbintree = get_distant_cloonix_tree();
  static char *argv[]={"/usr/libexec/cloonix/client/cloonix-urxvt",
                       "-T", title, "-e", bin_path, config, nemo,
                       "-cmd", cmd, "-a", path, NULL}; 
  memset(bin_path, 0, MAX_PATH_LEN);
  memset(config, 0, MAX_PATH_LEN);
  memset(cmd, 0, MAX_PATH_LEN);
  memset(path, 0, MAX_PATH_LEN);
  memset(nm, 0, MAX_NAME_LEN);
  memset(title, 0, MAX_PATH_LEN);
  memset(nemo, 0, MAX_NAME_LEN);
  strncpy(nm, name, MAX_NAME_LEN-1);
  snprintf(nemo, MAX_NAME_LEN-1, "%s", local_get_cloonix_name()); 
  snprintf(title, MAX_PATH_LEN-1, "%s/%s", nemo, nm); 
  snprintf(bin_path, MAX_PATH_LEN-1, "%s/client/cloonix-xwycli", locbintree);
  snprintf(config, MAX_PATH_LEN-1, "/usr/libexec/cloonix/common/etc/cloonix.cfg");
  snprintf(cmd, MAX_PATH_LEN-1, "%s/server/cloonix-dtach", dstbintree);
  snprintf(path, MAX_PATH_LEN-1, "%s/%s", get_dtach_work_path(), nm);
  return (argv);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
GtkWidget *get_main_window(void)
{
  return g_main_window;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_spice_vm_path(int vm_id)
{
  static char path[2*MAX_PATH_LEN];
  snprintf(path, 2*MAX_PATH_LEN, "/var/lib/cloonix/%s/vm/vm%d/%s", 
           g_clc.network, vm_id, SPICE_SOCK);
  path[2*MAX_PATH_LEN-1] = 0;
  return(path);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_path_to_qemu_spice(void)
{
  char *result = NULL;
  static char path[MAX_PATH_LEN];
  sprintf(path,"%s/client/cloonix-spicy", get_local_cloonix_tree());
  if (file_exists_exec(path))
    result = path;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_path_to_nemo_icon(void)
{
  char *result = NULL;
  static char path[MAX_PATH_LEN];
  sprintf(path, 
          "%s/client/lib_client/include/clownix64.png", 
          get_local_cloonix_tree());
  if (is_file_readable(path))
    result = path;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_current_directory(void)
{
  return g_current_directory;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void set_main_window_coords(int x, int y, int width, int heigh)
{
  main_win_x = x;
  main_win_y = y;
  main_win_width = width;
  main_win_height = heigh;
  set_popup_window_coords();
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void init_set_main_window_coords(void)
{
  gtk_window_get_position(GTK_WINDOW(g_main_window), &main_win_x, &main_win_y);
  gtk_window_get_size(GTK_WINDOW(g_main_window), 
                      &main_win_width, &main_win_height);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void get_main_window_coords(gint *x, gint *y, gint *width, gint *heigh)
{
  *x = main_win_x;
  *y = main_win_y;
  *width = main_win_width;
  *heigh = main_win_height;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void put_top_left_icon(GtkWidget *mainwin)
{
  GtkWidget *image;
  GdkPixbuf *pixbuf;
  char *path = get_path_to_nemo_icon();
  image = gtk_image_new_from_file (path);
  pixbuf = gtk_image_get_pixbuf(GTK_IMAGE(image));
  gtk_window_set_icon(GTK_WINDOW(mainwin), pixbuf);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void destroy_handler (GtkWidget *win, gpointer data)
{
  pid_clone_kill_all();
  sleep(2);
  if (data)
    KOUT(" ");
  if (win != g_main_window)
    KOUT(" ");
  if (g_source_remove(main_timeout))
    g_print ("OK\n"); 
  else
    g_print ("KO\n"); 
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void my_mkdir(char *dst_dir)
{
  struct stat stat_file;
  char copy1_dir[MAX_PATH_LEN];
  char copy2_dir[MAX_PATH_LEN];
  char *up_one, *up_two;
  memset(copy1_dir, 0, MAX_PATH_LEN);
  strncpy(copy1_dir, dst_dir, MAX_PATH_LEN-1);
  memset(copy2_dir, 0, MAX_PATH_LEN);
  strncpy(copy2_dir, dst_dir, MAX_PATH_LEN-1);
  up_one = dirname(copy2_dir); 
  up_two = dirname(up_one); 
  up_one = dirname(copy1_dir); 
  if (strlen(up_two) > 2) 
    {
    if (stat(up_two, &stat_file))
      if (mkdir(up_two, 0777))
        KOUT("%s, %d", up_two, errno);
    }
  if (strlen(up_one) > 2) 
    {
    if (stat(up_one, &stat_file))
      if (mkdir(up_one, 0777))
        KOUT("%s, %d", up_two, errno);
    }
  if (mkdir(dst_dir, 0777))
    {
    if (errno != EEXIST)
      KOUT("%s, %d", dst_dir, errno);
    else
      {
      if (stat(dst_dir, &stat_file))
        KOUT("%s, %d", dst_dir, errno);
      if (!S_ISDIR(stat_file.st_mode))
        {
        unlink(dst_dir);
        if (mkdir(dst_dir, 0777))
          KOUT("%s, %d", dst_dir, errno);
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void main_timer_activation(void)
{
  if (main_timeout)
    g_source_remove(main_timeout);
  main_timeout = g_timeout_add(10, refresh_request_timeout, (gpointer) NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *get_distant_snf_dir(void)
{
  return (g_distant_snf_dir);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void work_dir_resp(int tid, t_topo_clc *conf)
{
  char title[2*MAX_NAME_LEN];
  char tmp_dtach_work_path[2*MAX_PATH_LEN];
  char tmp_distant_snf_dir[2*MAX_PATH_LEN];
  GtkWidget *window, *vbox;
  GtkWidget *scrolled;
  GError *pixerror;
  eth_choice = 0;
  if (strcmp(conf->version, cloonix_conf_info_get_version()))
    {
    printf("\n\nCloon Version client:%s DIFFER FROM server:%s\n\n", 
           cloonix_conf_info_get_version(), conf->version);
    exit(-1);
    }
  daemon(0,1);
  move_init();
  popup_init();
  memcpy(&g_clc, conf, sizeof(t_topo_clc));

  snprintf(tmp_dtach_work_path, 2*MAX_PATH_LEN, "/var/lib/cloonix/%s/%s",
                                g_clc.network, DTACH_SOCK);
  tmp_dtach_work_path[MAX_PATH_LEN-1] = 0;
  strcpy(g_dtach_work_path, tmp_dtach_work_path); 

  snprintf(tmp_distant_snf_dir, 2*MAX_PATH_LEN, "/var/lib/cloonix/%s/%s",
           g_clc.network, SNF_DIR);
  tmp_distant_snf_dir[MAX_PATH_LEN-1] = 0;
  strcpy(g_distant_snf_dir, tmp_distant_snf_dir);

  if (gtk_init_check(NULL, NULL) == FALSE)
    KOUT("Error in gtk_init_check function");

  if (gdk_pixbuf_init_modules("/usr/libexec/cloonix/common/share", &pixerror))
    KERR("ERROR gdk_pixbuf_init_modules");

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_accept_focus(GTK_WINDOW(window), FALSE);


  menu_init();
  g_main_window = window;
  init_set_main_window_coords();
  g_signal_connect (G_OBJECT (window), "destroy",
		      (GCallback) destroy_handler, NULL);
  if (g_i_am_in_cloon)
    snprintf(title, 2*MAX_NAME_LEN, "%s/%s", 
             g_i_am_in_cloonix_name, local_get_cloonix_name());
  else
    snprintf(title, MAX_NAME_LEN, "%s", local_get_cloonix_name());
  title[MAX_NAME_LEN-1] = 0;
  gtk_window_set_title (GTK_WINDOW (window), title);
  gtk_window_set_default_size (GTK_WINDOW (window), WIDTH, HEIGH);
  put_top_left_icon(window);
  topo_set_signals(window);
  scrolled = gtk_scrolled_window_new(NULL, NULL);
  vbox   = topo_canvas();
  gtk_container_add (GTK_CONTAINER(scrolled), vbox);
  gtk_container_add (GTK_CONTAINER (window), scrolled);
  gtk_widget_show_all(window);
  main_timer_activation();
  send_layout_event_sub(get_clownix_main_llid(), 0, 1);
  glib_prepare_rx_tx(get_clownix_main_llid());
  interface_topo_subscribe();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void init_local_cloonix_bin_path(char *curdir, char *callbin)
{
  char path[2*MAX_PATH_LEN];
  char *ptr;
  memset(g_cloonix_root_tree, 0, MAX_PATH_LEN);
  memset(path, 0, MAX_PATH_LEN);

  if (callbin[0] == '/')
    snprintf(path, MAX_PATH_LEN, "%s", callbin);
  else if ((callbin[0] == '.') && (callbin[1] == '/'))
    snprintf(path, MAX_PATH_LEN, "%s/%s", curdir, &(callbin[2]));
  else
    KOUT("%s", callbin);

  ptr = strrchr(path, '/');
  if (!ptr)
    KOUT("%s", path);
  *ptr = 0;

  ptr = strrchr(path, '/');
  if (!ptr)
    KOUT("%s", path);
  *ptr = 0;

  strncpy(g_cloonix_root_tree, path, MAX_PATH_LEN-1);
  snprintf(path,2*MAX_PATH_LEN,"%s/client/cloonix-gui",g_cloonix_root_tree);
  path[MAX_PATH_LEN-1] = 0;
  if (access(path, X_OK))
    KOUT("%s", path);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int main(int argc, char *argv[])
{
  g_i_am_in_cloon = i_am_inside_cloon(g_i_am_in_cloonix_name);
  main_timeout = 0;
  eth_choice = 0;
  if (argc < 2)
    KOUT("%d", argc);
  if (cloonix_conf_info_init(argv[1]))
    KOUT("%s", argv[1]);
  if (argc < 3)
    {
    printf("\nMISSING NAME:");
    printf("\n\n%s\n\n", cloonix_conf_info_get_names());
    exit(1);
    }
  g_cloonix_conf_info = cloonix_cnf_info_get(argv[2], &g_cloonix_rank);
  if (!g_cloonix_conf_info)
    {
    printf("\nBAD NAME %s:", argv[2]);
    printf("\n\n%s\n\n", cloonix_conf_info_get_names());
    exit(1);
    }
  printf("\nVersion:%s\n", cloonix_conf_info_get_version());
  memset(g_current_directory, 0, MAX_PATH_LEN);
  if (!getcwd(g_current_directory, MAX_PATH_LEN-1))
    KOUT(" ");
  init_local_cloonix_bin_path(g_current_directory, argv[0]); 
  if(!getenv("HOME"))
    KOUT("No HOME env");
  if(!getenv("USER"))
    KOUT("No USER env");
  if(!getenv("DISPLAY"))
    KOUT("No DISPLAY env");

  memset(g_doors_client_addr, 0, MAX_PATH_LEN);
  strncpy(g_doors_client_addr, g_cloonix_conf_info->doors, MAX_PATH_LEN-1);
  printf("CONNECT TO UNIX SERVER: %s\n", g_doors_client_addr);
  memset(g_password, 0, MSG_DIGEST_LEN);
  strncpy(g_password, g_cloonix_conf_info->passwd, MSG_DIGEST_LEN-1);
  interface_switch_init(g_doors_client_addr, g_password);
  eventfull_init();
  client_get_path(0, work_dir_resp);
  printf("CONNECTED\n");
  layout_topo_init();
  request_move_stop_go(1);
  bdplot_init();
  clownix_timeout_add(10, timeout_periodic_work, NULL, NULL, NULL);
  gtk_main();
  return 0;
}
/*--------------------------------------------------------------------------*/


