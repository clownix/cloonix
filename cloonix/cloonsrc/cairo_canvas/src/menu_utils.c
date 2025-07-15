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
#include "doorways_sock.h"
#include "client_clownix.h"
#include "interface.h"
#include "commun_consts.h"
#include "bank.h"
#include "move.h"
#include "popup.h"
#include "cloonix.h"
#include "main_timer_loop.h"
#include "pid_clone.h"
#include "menu_utils.h"
#include "menus.h"
#include "xml_utils.h"
#include "layout_x_y.h"
#include "topo.h"
#include "pidwait.h"


int get_cloonix_rank(void);
char *get_distant_snf_dir(void);
char *get_doors_client_addr(void);

GtkWidget *get_main_window(void);


void put_top_left_icon(GtkWidget *mainwin);

static char binary_name[MAX_NAME_LEN];


/****************************************************************************/
typedef struct t_qemu_spice_item 
{
  char vm_name[MAX_NAME_LEN];
  char doors_server_path[MAX_NAME_LEN];
  char spice_server_path[MAX_PATH_LEN];
  struct t_qemu_spice_item *prev;
  struct t_qemu_spice_item *next;
} t_qemu_spice_item;

static t_qemu_spice_item *qemu_spice_head;

#define TIMR 50
#define TIMR_REPEAT 10



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

/****************************************************************************/
static t_qemu_spice_item *qemu_spice_find_name(char *vm_name)
{
  t_qemu_spice_item *cur = qemu_spice_head;
  while (cur)
    {
    if (!strcmp(cur->vm_name, vm_name))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void qemu_spice_release_name(char *vm_name)
{
  t_qemu_spice_item *cur = qemu_spice_find_name(vm_name);
  if (!cur)
    KOUT("ERROR");
  if (cur == qemu_spice_head)
    {
    if (cur->next)
      cur->next->prev = NULL;
    qemu_spice_head = cur->next;
    }
  else
    {
    if (!cur->prev)
      KOUT("ERROR");
    cur->prev->next = cur->next;
    if (cur->next)
      cur->next->prev = cur->prev; 
    }
  clownix_free(cur, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_qemu_spice_item *qemu_spice_reserve_name(char *vm_name)
{
  t_qemu_spice_item *result = NULL;
  t_qemu_spice_item *cur = qemu_spice_find_name(vm_name);
  if (!cur)
    {
    result = (t_qemu_spice_item *)
             clownix_malloc(sizeof(t_qemu_spice_item), 18);
    memset(result, 0, sizeof(t_qemu_spice_item));
    strcpy(result->vm_name, vm_name);
    if (qemu_spice_head)
      {
      qemu_spice_head->prev = result;
      result->next = qemu_spice_head;
      qemu_spice_head = result;
      }
    else
      qemu_spice_head = result;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void debug_print_cmd(char **argv)
{
  int i, len = 0;
  char info[MAX_PRINT_LEN];
  for (i=0; argv[i]; i++)
    {
    len += sprintf(info+len, "%s ", argv[i]);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int check_before_start_launch(char **argv)
{
  int result = 0;
  char info[MAX_PRINT_LEN];
  if (file_exists_exec(argv[0]))
    result = 1;
  else
    {
    sprintf(info, "File: %s Problem\n", argv[0]);
    insert_next_warning(info, 1);
    }
  debug_print_cmd(argv);
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void start_qemu_spice(char *name, int vm_id)
{
  char *spicy = pthexec_spicy_bin();
  char title[MAX_PATH_LEN];
  char net[MAX_NAME_LEN];
  char sock[2*MAX_PATH_LEN];
  char *argv[11];

  if (pidwait_find(type_pid_spicy_gtk, name, 0))
    KERR("ERROR %s", name);
  else
    {
    memset(title, 0, MAX_PATH_LEN);
    memset(net, 0, MAX_NAME_LEN);
    memset(sock, 0, 2*MAX_PATH_LEN);
    memset(argv, 0, 10*sizeof(char *));
    snprintf(net, MAX_NAME_LEN, "%s", get_net_name());
    snprintf(title, MAX_PATH_LEN-1, "--title=%s/%s", net, name);
    snprintf(sock, 2*MAX_PATH_LEN-1,
             "/var/lib/cloonix/%s/vm/vm%d/spice_sock", net, vm_id);
    argv[0] = spicy;
    argv[1] = title;
    argv[2] = "-d";
    argv[3] = get_doors_client_addr();
    argv[4] = "-c";
    argv[5] = sock;
    argv[6] = "-w";
    argv[7] = get_password();
    argv[8] = NULL;
    pidwait_alloc(type_pid_spicy_gtk, name, 0, sock, argv);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void start_wireshark(char *vm_name, int num)
{
  char *argv[15];
  char *xwycli = pthexec_xwycli_bin();
  char *wire = pthexec_wireshark_bin();
  char *cnf = pthexec_cloonix_cfg();
  char *net = get_net_name();
  char snf[2*MAX_PATH_LEN];
  char title[MAX_PATH_LEN];
  
  if (pidwait_find(type_pid_wireshark, vm_name, num))
    KERR("ERROR %s %d", vm_name, num);
  else
    {
    memset(argv, 0, 15*sizeof(char *));
    memset(snf, 0, 2*MAX_PATH_LEN);
    memset(title, 0, MAX_PATH_LEN);
    snprintf(snf, 2*MAX_PATH_LEN-1, "/var/lib/cloonix/%s/snf/%s_%d",
             get_net_name(), vm_name, num);
    snprintf(title,MAX_PATH_LEN-1,"%s,%s,eth%d",get_net_name(),vm_name,num);
    argv[0] = xwycli;
    argv[1] = cnf;
    argv[2] = net;
    argv[3] = "-dae";
    argv[4] = wire;
    argv[5] = "-i";
    argv[6] = snf;
    argv[7] = "-t";
    argv[8] = title;
    argv[9] = NULL;
    if (check_before_start_launch(argv))
      pidwait_alloc(type_pid_wireshark, vm_name, num, snf, argv);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void node_qemu_spice(GtkWidget *mn, t_item_ident *pm)
{
  t_qemu_spice_item *it;
  char info[MAX_PRINT_LEN];
  char *path = get_path_to_qemu_spice();
  int vm_id;
  if (path)
    {
    it = qemu_spice_reserve_name(pm->name);
    if (it)
      {
      vm_id = get_vm_id_from_topo(pm->name);
      if (!vm_id)
        {
        sprintf(info, "Could not get vm_id");
        insert_next_warning(info, 1);
        }
      else
        {
        start_qemu_spice(pm->name, vm_id);
        }
      qemu_spice_release_name(pm->name);
      }
    else
      {
      sprintf(info, "Problem qemu_spice_reserve_name");
      insert_next_warning(info, 1);
      }
    }
  else
    {
    KERR("ERROR Missing: %s", pthexec_spicy_bin());
    sprintf(info, "Missing: %s", pthexec_spicy_bin()); 
    insert_next_warning(info, 1);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void node_dtach_console(GtkWidget *mn, t_item_ident *pm)
{
  char *name = pm->name;
  char **argv;

  if (pidwait_find(type_pid_dtach, name, 0))
    KERR("ERROR %s", name);
  else
    {
    argv = get_argv_local_shk(name);
    if (check_before_start_launch(argv))
      pidwait_alloc(type_pid_dtach, name, 0, "none", argv);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static char **get_argv_crun_screen_console(char *name)
{
  static char cmd[2*MAX_PATH_LEN];
  static char nm[MAX_NAME_LEN];
  static char title[MAX_PATH_LEN];
  static char nemo[MAX_NAME_LEN];
  static char xwycli[MAX_PATH_LEN];
  static char urxvt[MAX_PATH_LEN];
  static char cfg[MAX_PATH_LEN];
  static char *argv[]={urxvt, "-T", title, "-e", xwycli,
                       cfg, nemo, "-crun", cmd, NULL};
  char **ptr_argv = NULL;
  memset(cmd, 0, 2*MAX_PATH_LEN);
  memset(nm, 0, MAX_NAME_LEN);
  memset(title, 0, MAX_PATH_LEN);
  memset(nemo, 0, MAX_NAME_LEN);
  memset(xwycli, 0, MAX_PATH_LEN);
  memset(urxvt, 0, MAX_PATH_LEN);
  memset(cfg, 0, MAX_PATH_LEN);

  strncpy(cfg, pthexec_cloonix_cfg(), MAX_PATH_LEN-1);
  strncpy(xwycli, pthexec_xwycli_bin(), MAX_PATH_LEN-1);
  strncpy(urxvt, pthexec_urxvt_bin(), MAX_PATH_LEN-1);
  strncpy(nm, name, MAX_NAME_LEN-1);
  snprintf(nemo, MAX_NAME_LEN-1, "%s", get_net_name());
  snprintf(title, MAX_PATH_LEN-1, "%s/%s", nemo, nm);
  snprintf(cmd, 2*MAX_PATH_LEN-1,
           "%s --cgroup-manager=disabled "
           "--log=/var/lib/cloonix/%s/log/debug_crun.log "
           "--root=/var/lib/cloonix/%s/%s exec -ti %s /bin/sh",
           pthexec_crun_bin(), nemo, nemo, CRUN_DIR, nm);
  ptr_argv = argv;
  return (ptr_argv);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char **xephyr_get_lxsession_args(char *nm)
{
  static char cmd[MAX_PATH_LEN];
  static char name[MAX_NAME_LEN];
  static char addr[MAX_NAME_LEN];
  static char passwd[MAX_NAME_LEN];
  static char ssh[MAX_PATH_LEN];
  static char *argv[] = {ssh, addr, passwd, name, cmd, NULL};
  char **ptr_argv = NULL;

  memset(cmd,    0, MAX_PATH_LEN);
  memset(ssh,    0, MAX_PATH_LEN);
  memset(addr,   0, MAX_NAME_LEN);
  memset(passwd, 0, MAX_NAME_LEN);
  memset(name,   0, MAX_NAME_LEN);

  strncpy(ssh, pthexec_dropbear_ssh(), MAX_PATH_LEN-1);
  strncpy(name, nm, MAX_NAME_LEN-1);
  strncpy(addr,  get_doors_client_addr(), MAX_NAME_LEN-1);
  strncpy(passwd, get_password(), MAX_NAME_LEN-1);
  snprintf(cmd, MAX_PATH_LEN-1, "%s", LXSESSION);
  ptr_argv = argv;
  return (ptr_argv);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void crun_item_xephyr(GtkWidget *mn, t_item_ident *pm)
{
  char *argv[20];
  char *net = get_net_name();
  char title[MAX_PATH_LEN];
  memset(title, 0, MAX_PATH_LEN);
  snprintf(title, MAX_PATH_LEN-1, "%s/%s", net, pm->name);
  if (pidwait_find(type_pid_xephyr_frame, pm->name, 0))
    KERR("ERROR %s", pm->name);
  else
    {
    memset(argv, 0, 20*sizeof(char *));
    argv[0] = pthexec_xephyr_bin();
    argv[1] = "to_be_defined";
    argv[2] = "-resizeable";
    argv[3] = "-title";
    argv[4] = title;
    argv[5] = "-ac";
    argv[6] = "-br";
    argv[7] = "-noreset";
    argv[8] = "-extension";
    argv[9] = "DPMS";
    argv[10] = "-extension";
    argv[11] = "GLX";
    argv[12] = "-extension";
    argv[13] = "MIT-SHM";
    argv[14] = "-extension";
    argv[15] = "MIT-SCREEN-SAVER";
    argv[16] = NULL;
    if (check_before_start_launch(argv))
      pidwait_alloc(type_pid_xephyr_frame, pm->name, 0, title, argv);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void crun_item_xephyr_session(char *name)
{ 
  char **argv;
  if (pidwait_find(type_pid_xephyr_session, name, 0))
    KERR("ERROR %s", name);
  else
    {
    argv = xephyr_get_lxsession_args(name);
    if (check_before_start_launch(argv))
      pidwait_alloc(type_pid_xephyr_session, name, 0, "none", argv);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void crun_item_rsh(GtkWidget *mn, t_item_ident *pm)
{
  char **argv;
  if (pidwait_find(type_pid_crun_screen, pm->name, 0))
    KERR("ERROR %s", pm->name);
  else
    {
    argv = get_argv_crun_screen_console(pm->name);
    pidwait_alloc(type_pid_crun_screen, pm->name, 0, "none", argv);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_end_del_all(void *data)
{
  look_for_lan_and_del_all();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void callback_del_all(int tid, int status, char *err)
{
  char info[MAX_PRINT_LEN];
  if (status)
    {
    sprintf(info, "Del all ended KO: %s\n", err);
    insert_next_warning(info, 1);
    }
  else
    clownix_timeout_add(1, timer_end_del_all, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void topo_delete(GtkWidget *mn)
{
  client_del_all(0, callback_del_all);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void menu_utils_init(void)
{
  qemu_spice_head = NULL;
  strcpy(binary_name, "cloonix_tux2tux");
  pidwait_init();
}
/*--------------------------------------------------------------------------*/
