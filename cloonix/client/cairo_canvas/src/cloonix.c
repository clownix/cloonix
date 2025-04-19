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
#include <fcntl.h>

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
#include "pidwait.h"


void interface_topo_subscribe(void);

void timeout_periodic_work(void *data);


static int g_novnc;
static int g_argc;
static char *g_argv[10];
/*--------------------------------------------------------------------------*/
gboolean refresh_request_timeout (gpointer  data);
void topo_set_signals(GtkWidget *window);
GtkWidget *topo_canvas(void);

static int g_cloonix_rank;

static t_topo_clc g_clc;

static int eth_choice = 0;
static GtkWidget *g_main_window;

static gint main_win_x, main_win_y, main_win_width, main_win_height;
static guint main_timeout;
static char g_current_directory[MAX_PATH_LEN];
static char g_doors_client_addr[MAX_PATH_LEN];
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
char *get_net_name(void)
{
  return (g_clc.network);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_doors_client_addr(void)
{
  return g_doors_client_addr;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char **get_argv_local_shk(char *name)
{
  static char config[MAX_PATH_LEN];
  static char cmd[MAX_PATH_LEN];
  static char path[MAX_PATH_LEN];
  static char nm[MAX_NAME_LEN];
  static char title[MAX_PATH_LEN];
  static char nemo[MAX_NAME_LEN];
  static char *argv[]={URXVT_BIN, "-T", title, "-e", XWYCLI_BIN, config,
                       nemo, "-cmd", cmd, "-a", path, NULL}; 
  char **ptr_argv;
  memset(config, 0, MAX_PATH_LEN);
  memset(cmd, 0, MAX_PATH_LEN);
  memset(path, 0, MAX_PATH_LEN);
  memset(nm, 0, MAX_NAME_LEN);
  memset(title, 0, MAX_PATH_LEN);
  memset(nemo, 0, MAX_NAME_LEN);
  strncpy(nm, name, MAX_NAME_LEN-1);
  snprintf(nemo, MAX_NAME_LEN-1, "%s", get_net_name()); 
  snprintf(title, MAX_PATH_LEN-1, "%s/%s", nemo, nm); 
  snprintf(config, MAX_PATH_LEN-1, CLOONIX_CFG);
  snprintf(cmd, MAX_PATH_LEN-1, "/usr/libexec/cloonix/cloonfs/cloonix-dtach");
  snprintf(path, MAX_PATH_LEN-1, "/var/lib/cloonix/%s/%s/%s", nemo, DTACH_SOCK, nm);
  ptr_argv = argv;
  return (ptr_argv);
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
  sprintf(path,"/usr/libexec/cloonix/cloonfs/cloonix-spicy");
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
  sprintf(path,"/usr/libexec/cloonix/cloonfs/etc/clownix64.png"); 
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
  pidwait_kill_all_active();
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
  int disp;
  char title[2*MAX_NAME_LEN];
  char tmp_distant_snf_dir[2*MAX_PATH_LEN];
  char *display;
  GtkWidget *vbox, *scrolled;
  GError *pixerror;
  eth_choice = 0;
  if (strcmp(conf->version, cloonix_conf_info_get_version()))
    {
    printf("\n\nCloon Version client:%s DIFFER FROM server:%s\n\n", 
           cloonix_conf_info_get_version(), conf->version);
    }

  display = getenv("DISPLAY");
  if (display)
    {
    if (sscanf(display, ":%d", &disp) != 1)
      KERR("WARNING DISPLAY %s", display); 
    else
      {
      if (disp >= NOVNC_DISPLAY)
        g_novnc = 1;
      }
    }
  daemon(0,0);
  move_init();
  popup_init();
  memcpy(&g_clc, conf, sizeof(t_topo_clc));
  snprintf(tmp_distant_snf_dir, 2*MAX_PATH_LEN, "/var/lib/cloonix/%s/%s",
           g_clc.network, SNF_DIR);
  tmp_distant_snf_dir[MAX_PATH_LEN-1] = 0;
  strcpy(g_distant_snf_dir, tmp_distant_snf_dir);
  if (gdk_pixbuf_init_modules("/usr/libexec/cloonix/cloonfs/share", &pixerror))
    KERR("ERROR gdk_pixbuf_init_modules");
  g_main_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_accept_focus(GTK_WINDOW(g_main_window), FALSE);
  menu_init();
  init_set_main_window_coords();
  g_signal_connect(G_OBJECT(g_main_window), "destroy",
		      (GCallback) destroy_handler, NULL);
  snprintf(title, MAX_NAME_LEN, "%s", get_net_name());
  title[MAX_NAME_LEN-1] = 0;
  gtk_window_set_title(GTK_WINDOW(g_main_window), title);
  gtk_window_set_default_size(GTK_WINDOW(g_main_window), WIDTH, HEIGH);
  put_top_left_icon(g_main_window);
  topo_set_signals(g_main_window);
  scrolled = gtk_scrolled_window_new(NULL, NULL);
  vbox   = topo_canvas();
  gtk_container_add (GTK_CONTAINER(scrolled), vbox);
  gtk_container_add (GTK_CONTAINER (g_main_window), scrolled);
  gtk_widget_show_all(g_main_window);
  main_timer_activation();
  send_layout_event_sub(get_clownix_main_llid(), 0, 1);
  glib_prepare_rx_tx(get_clownix_main_llid());
  interface_topo_subscribe();
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void callback_end_fix(int tid, int status, char *err)
{
  if (tid)
    KERR("ERROR callback_end_fix");
  if (status)
    KERR("ERROR callback_end_fix %s", err);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static char *local_read_whole_file(char *file_name, int *len)
{
  char *buf = NULL;
  int fd, readlen;
  struct stat statbuf;
  if (stat(file_name, &statbuf) != 0)
    KERR("Cannot get size of file %s\n", file_name);
  else if (statbuf.st_size)
    {
    *len = statbuf.st_size;
    fd = open(file_name, O_RDONLY);
    if (fd == -1)
      KERR("Cannot open file %s errno %d\n", file_name, errno);
    else
      {
      buf = (char *) malloc((*len + 1) * sizeof(char));
      memset(buf, 0, (*len + 1) * sizeof(char));
      readlen = read(fd, buf, *len);
      if (readlen != *len)
        {
        KERR("Length of file error for file %s %d\n", file_name, errno);
        free(buf);
        buf = NULL;
        }
      close (fd);
      }
    }
  return buf;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void cnf_fix_xauthority(void)
{
  int len;
  char cmd[2*MAX_PATH_LEN];
  char *buf, *tmp = "/tmp/cloonix_fix_xauthority";
  char *display = getenv("DISPLAY");
  char *xauthority = getenv("XAUTHORITY");
  if (!display)
    KERR("ERROR NO DISPLAY FOR A SOFTWARE GUI");
  else if (!xauthority)
    KERR("ERROR NO XAUTHORITY FOR A SOFTWARE GUI");
  else
    {
    memset(cmd, 0, 2 * MAX_PATH_LEN);
    snprintf(cmd, 2 * MAX_PATH_LEN - 1,
    "/usr/libexec/cloonix/cloonfs/xauth nextract - %s > %s", display, tmp);
    if (system(cmd))
      KERR("ERROR %s", cmd);
    else
      {
      buf = local_read_whole_file(tmp, &len);
      if (!buf)
        KERR("ERROR %s", cmd);
      else
        {
        client_fix_display(0, callback_end_fix, display, buf);
        free(buf);
        }
      }
    }
}
/*---------------------------------------------------------------------------*/


/****************************************************************************/
int main(int argc, char *argv[])
{
  int running_in_crun;
  char tmpnet[MAX_NAME_LEN];
  g_novnc = 0;
  g_argc = 2;
  g_argv[0] = "/usr/libexec/cloonix/cloonfs/cloonix-gui";
  g_argv[1] = "--no-xshm";
  g_argv[2] = NULL;
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

  signal(SIGPIPE,SIG_IGN);
  if (gtk_init_check(NULL, NULL) == FALSE)
    KOUT("Error in gtk_init_check first function");
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

  memset(g_doors_client_addr, 0, MAX_PATH_LEN);
  running_in_crun = lib_io_running_in_crun(tmpnet);
  if (running_in_crun && (!strcmp(tmpnet, argv[2])))
    {
    snprintf(g_doors_client_addr, MAX_PATH_LEN-1,
             "%s_%s/proxy_pmain.sock",
             PROXYSHARE_IN, argv[2]);
    }
  else
    {
    strncpy(g_doors_client_addr, g_cloonix_conf_info->doors, MAX_PATH_LEN-1);
    }
  printf("CONNECT TO UNIX SERVER: %s\n", g_doors_client_addr);
  memset(g_password, 0, MSG_DIGEST_LEN);
  strncpy(g_password, g_cloonix_conf_info->passwd, MSG_DIGEST_LEN-1);
  interface_switch_init(g_doors_client_addr, g_password);
  eventfull_init();
  client_get_path(0, work_dir_resp);
  printf("CONNECTED\n");
  layout_topo_init();
  cnf_fix_xauthority();
  request_move_stop_go(1);
  bdplot_init();
  clownix_timeout_add(10, timeout_periodic_work, NULL, NULL, NULL);
  gtk_main();
  return 0;
}
/*--------------------------------------------------------------------------*/


