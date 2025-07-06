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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "commun_daemon.h"
#include "event_subscriber.h"
#include "pid_clone.h"
#include "utils_cmd_line_maker.h"
#include "uml_clownix_switch.h"
#include "hop_event.h"
#include "llid_trace.h"
#include "machine_create.h"
#include "file_read_write.h"
#include "ovs.h"
#include "ovs_snf.h"
#include "fmt_diag.h"
#include "qmp.h"
#include "system_callers.h"
#include "stats_counters.h"
#include "kvm.h"
#include "msg.h"
#include "xwy.h"
#include "suid_power.h"

enum{
  msg_type_diag = 1,
  msg_type_pid,
};

t_pid_lst *create_list_pid(int *nb);
static void ovs_destroy_test(void);
static int g_system_promisc;

/****************************************************************************/
typedef struct t_ovs
{
  char name[MAX_NAME_LEN];
  int clone_start_pid;
  int clone_start_pid_just_done;
  int pid;
  int ovsdb_pid;
  int ovs_pid;
  int ovs_pid_ready;
  int getsuidroot;
  int open_ovsdb;
  int open_ovsdb_wait;
  int open_ovsdb_ok;
  int open_ovs;
  int open_ovs_wait;
  int open_ovs_ok;
  int llid;
  int periodic_count;
  int unanswered_pid_req;
  int connect_try_count;
  int destroy_requested;
  int nb_resp_ovs_ko;
  int watchdog_protect;
  int global_pid_req_watch;
  int daemon_done;
  int last_msg_req_destroy_sent;
} t_ovs;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
typedef struct t_arg_ovsx
{
  char bin_path[MAX_PATH_LEN];
  char net[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  char sock[MAX_PATH_LEN];
  char ovsx_bin[MAX_PATH_LEN];
  char db_dir[MAX_PATH_LEN];
} t_arg_ovsx;
/*--------------------------------------------------------------------------*/

static t_ovs *g_head_ovs;


/****************************************************************************/
static int connect_ovs_try(t_ovs *mu)
{
  char *sock = utils_get_ovs_path(mu->name);
  int llid = string_client_unix(sock, uml_clownix_switch_error_cb,
                                uml_clownix_switch_rx_cb, "ovsdb");
  if (llid)
    {
    if (hop_event_alloc(llid, type_hop_ovsdb, mu->name, 0))
      KERR("%s", mu->name);
    llid_trace_alloc(llid, mu->name, 0, 0, type_llid_trace_endp_ovsdb);
    }
  return llid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int try_send_msg_ovs(t_ovs *cur, int msg_type, int tid, char *msg)
{
  int result = -1;
  if (cur->llid)
    {
    if (msg_exist_channel(cur->llid))
      {
      if (msg_type == msg_type_diag)
        {
        hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
        rpct_send_sigdiag_msg(cur->llid, tid, msg);
        result = 0;
        }
      else if (msg_type == msg_type_pid)
        {
        rpct_send_pid_req(cur->llid, tid, msg, 0);
        result = 0;
        }
      else
        KERR("ERROR DROP MSG OVS %s", msg);
      }
    else
      KERR("ERROR DROP MSG OVS %s", msg);
    }
  else
    KERR("ERROR DROP MSG OVS %s", msg);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_ovs *_ovs_find_with_llid(int llid)
{
  t_ovs *cur = g_head_ovs;
  if (llid && cur && (cur->llid != llid))
    {
    cur = NULL;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_ovs *ovs_find(void)
{
  t_ovs *cur = g_head_ovs;
  return (cur);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static char **make_argv(t_arg_ovsx *arg_ovsx)
{
  static char *argv[8];
  memset(argv, 0, 8*sizeof(char *));
  argv[0] = arg_ovsx->bin_path;
  argv[1] = arg_ovsx->net;
  argv[2] = arg_ovsx->name;
  argv[3] = arg_ovsx->sock;
  argv[4] = arg_ovsx->ovsx_bin;
  argv[5] = arg_ovsx->db_dir;
  return argv;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void process_demonized(void *unused_data, int status, char *name)
{
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void ovs_watchdog(void *data)
{
  t_arg_ovsx *mu = (t_arg_ovsx *) data;
  t_ovs *cur = ovs_find();
  if (cur)
    cur->watchdog_protect = 0;
  if (cur && ((!cur->llid) || 
              (!cur->pid) || 
              (!cur->periodic_count)))
    {
    KERR("ERROR OVS %s TIMEOUT %s %d %d %d",
         cur->name, mu->name, cur->llid, cur->pid, cur->periodic_count);
    ovs_destroy();
    }
  clownix_free(data, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int ovs_birth(void *data)
{
  t_arg_ovsx *arg_ovsx = (t_arg_ovsx *) data;
  char **argv = make_argv(arg_ovsx);
  execv(arg_ovsx->bin_path, argv);
  KOUT("ERROR execv");
  return 0;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static int create_ovs_drv_process(char *name)
{
  int pid = 0;
  char **argv;
  char bin_path[MAX_PATH_LEN];
  char *net = cfg_get_cloonix_name();
  char *sock = utils_get_ovs_path(name);
  char *ovsx_bin = utils_get_ovs_bin_dir();
  char *ovs_db_dir = utils_get_ovs_dir();
  t_arg_ovsx *arg_ovsx = (t_arg_ovsx *) clownix_malloc(sizeof(t_arg_ovsx), 10);
  memset(arg_ovsx, 0, sizeof(t_arg_ovsx));
  memset(bin_path, 0, MAX_PATH_LEN);
  snprintf(bin_path, MAX_PATH_LEN-1, "%s", utils_get_ovs_drv_bin_dir());
  strncpy(arg_ovsx->bin_path, bin_path, MAX_PATH_LEN-1);
  strncpy(arg_ovsx->net, net, MAX_NAME_LEN-1);
  strncpy(arg_ovsx->name, name, MAX_NAME_LEN-1);
  strncpy(arg_ovsx->sock, sock, MAX_PATH_LEN-1);
  strncpy(arg_ovsx->ovsx_bin, ovsx_bin, MAX_PATH_LEN-1);
  strncpy(arg_ovsx->db_dir, ovs_db_dir, MAX_PATH_LEN-1);
  if (file_exists(sock, R_OK))
    unlink(sock);
  if (!file_exists(bin_path, X_OK))
    KERR("ERROR %s Does not exist or not exec", bin_path);
  if (!file_exists(ovsx_bin, X_OK))
    KERR("ERROR %s Does not exist or not exec", bin_path);
  argv = make_argv(arg_ovsx);
  utils_send_creation_info("ovsx", argv);
  pid = pid_clone_launch(ovs_birth, process_demonized, NULL,
                         arg_ovsx, arg_ovsx, NULL, name, -1, 1);
  arg_ovsx = (t_arg_ovsx *) clownix_malloc(sizeof(t_arg_ovsx), 11);
  memset(arg_ovsx, 0, sizeof(t_arg_ovsx));
  strncpy(arg_ovsx->bin_path, bin_path, MAX_PATH_LEN-1);
  strncpy(arg_ovsx->net, net, MAX_NAME_LEN-1);
  strncpy(arg_ovsx->name, name, MAX_NAME_LEN-1);
  strncpy(arg_ovsx->sock, sock, MAX_PATH_LEN-1);
  strncpy(arg_ovsx->ovsx_bin, ovsx_bin, MAX_PATH_LEN-1);
  strncpy(arg_ovsx->db_dir, ovs_db_dir, MAX_PATH_LEN-1);
  clownix_timeout_add(4000, ovs_watchdog, (void *) arg_ovsx, NULL, NULL);
  return pid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void ovs_start_openvswitch_if_not_done(void)
{
  if (g_head_ovs == NULL)
    {
    event_print("Start OpenVSwitch");
    g_head_ovs = (t_ovs *) clownix_malloc(sizeof(t_ovs), 9);
    memset(g_head_ovs, 0, sizeof(t_ovs));
    strncpy(g_head_ovs->name, "ovsdb", MAX_NAME_LEN-1);
    g_head_ovs->watchdog_protect = 1;
    g_head_ovs->clone_start_pid = create_ovs_drv_process("ovsdb");
    g_head_ovs->clone_start_pid_just_done = 1;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_ovs_beat(void *data)
{
  t_ovs *cur = g_head_ovs;
  int type_hop_tid = type_hop_ovsdb;
  if (!cur)
    {
    clownix_timeout_add(1, timer_ovs_beat, NULL, NULL, NULL);
    }
  else 
    {
    if (cur->last_msg_req_destroy_sent == 0)
      {
      if (!cur->clone_start_pid)
        {
        KERR("ERROR");
        clownix_timeout_add(1, timer_ovs_beat, NULL, NULL, NULL);
        }
      else if (cur->clone_start_pid_just_done == 1)
        {
        cur->clone_start_pid_just_done = 0;
        clownix_timeout_add(1, timer_ovs_beat, NULL, NULL, NULL);
        }
      else if (cur->connect_try_count == 0)
        {
        cur->connect_try_count = 1;
        clownix_timeout_add(1, timer_ovs_beat, NULL, NULL, NULL);
        }
      else if (cur->llid == 0)
        {
        cur->llid = connect_ovs_try(cur);
        cur->connect_try_count += 1;
        if (cur->connect_try_count == 20)
          {
          KOUT("OVS %s NOT LISTENING destroy_request", cur->name);
          }
        clownix_timeout_add(10, timer_ovs_beat, NULL, NULL, NULL);
        }
      else if (cur->pid == 0)
        {
        if (!strlen(cur->name))
          KOUT(" ");
        try_send_msg_ovs(cur, msg_type_pid, type_hop_tid, cur->name);
        clownix_timeout_add(1, timer_ovs_beat, NULL, NULL, NULL);
        }
      else if (cur->getsuidroot == 0)
        {
        try_send_msg_ovs(cur, msg_type_diag, 0, "ovs_req_suidroot");
        clownix_timeout_add(1, timer_ovs_beat, NULL, NULL, NULL);
        }
      else if (cur->open_ovsdb == 0)
        {
        cur->open_ovsdb = 1;
        try_send_msg_ovs(cur, msg_type_diag, 0, "ovs_req_ovsdb");
        clownix_timeout_add(1, timer_ovs_beat, NULL, NULL, NULL);
        }
      else if (cur->open_ovsdb_ok == 0)
        {
        cur->open_ovsdb_wait += 1;
        if (cur->open_ovsdb_wait > 50)
          KERR("WARNING OVSDB %s NOT RESPONDING", cur->name);
        if (cur->open_ovsdb_wait > 400)
          {
          KERR("ERROR OVSDB %s NOT RESPONDING", cur->name);
          ovs_destroy();
          }
        clownix_timeout_add(10, timer_ovs_beat, NULL, NULL, NULL);
        }
      else if (cur->open_ovs == 0)
        {
        cur->open_ovs = 1;
        try_send_msg_ovs(cur, msg_type_diag, 0, "ovs_req_ovs");
        clownix_timeout_add(1, timer_ovs_beat, NULL, NULL, NULL);
        }
      else if (cur->open_ovs_ok == 0)
        {
        cur->open_ovs_wait += 1;
        if (cur->open_ovs_wait > 70)
          {
          KERR("ERROR OVS %s NOT RESPONDING", cur->name);
          ovs_destroy();
          }
        clownix_timeout_add(3, timer_ovs_beat, NULL, NULL, NULL);
        }
      else
        {
        cur->periodic_count += 1;
        if (cur->periodic_count >= 10)
          {
          if (!strlen(cur->name))
            KOUT(" ");
          try_send_msg_ovs(cur, msg_type_pid, type_hop_tid, cur->name);
          cur->periodic_count = 5;
          cur->unanswered_pid_req += 1;
          if (cur->unanswered_pid_req > 100)
            {
            KERR("ERROR OVS %s NOT RESPONDING", cur->name);
            ovs_destroy();
            }
          }
        clownix_timeout_add(50, timer_ovs_beat, NULL, NULL, NULL);
        }
      }
    else
      clownix_timeout_add(50, timer_ovs_beat, NULL, NULL, NULL);
    ovs_destroy_test();
    cur->global_pid_req_watch += 1;
    if (cur->global_pid_req_watch > 1000)
      KOUT("ERROR OVS %s NOT GIVING PID", cur->name);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_get_all_pid(t_lst_pid **lst_pid)
{
  t_lst_pid *glob_lst = NULL;
  t_ovs *cur = g_head_ovs;
  int i, result = 0;
  if (cur)
    {
    if (cur->pid)
      result++;
    if (cur->ovs_pid > 0)
      result++;
    if (cur->ovsdb_pid > 0)
      result++;
    }
  if (result)
    {
    glob_lst = (t_lst_pid *)clownix_malloc(result*sizeof(t_lst_pid),25);
    memset(glob_lst, 0, result*sizeof(t_lst_pid));
    cur = g_head_ovs;
    i = 0;
    if (cur->pid)
      {
      strncpy(glob_lst[i].name, cur->name, MAX_NAME_LEN-1);
      glob_lst[i].pid = cur->pid;
      i++;
      }
    if (cur->ovsdb_pid > 0)
      {
      strcpy(glob_lst[i].name, "cloonix-ovsdb-server");
      glob_lst[i].pid = cur->ovsdb_pid;
      i++;
      }
    if (cur->ovs_pid > 0)
      {
      strcpy(glob_lst[i].name, "cloonix-ovs-vswitchd");
      glob_lst[i].pid = cur->ovs_pid;
      i++;
      }
    if (i != result)
      KOUT("%d %d", i, result);
    }
  *lst_pid = glob_lst;
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_pid_resp(int llid, char *name, int toppid, int pid)
{
  t_ovs *cur = _ovs_find_with_llid(llid);
  if (!cur)
    KERR("%s %d %d", name, toppid, pid);
  else
    {
    if (cur->daemon_done == 0)
      {
      if ((cur->open_ovsdb)    &&
          (cur->open_ovsdb_ok) &&
          (cur->open_ovs)      &&
          (cur->open_ovs_ok))
        {
        printf("\n    OPENVSWITCH STARTED\n\n");
        cur->daemon_done = 1;
        }
      }
    cur->global_pid_req_watch = 0;
    cur->unanswered_pid_req = 0;
    if (strcmp(name, cur->name))
      KERR("%s %s", name, cur->name);
    if (cur->pid == 0)
      {
      cur->pid = pid;
      }
    else
      {
      if (cur->pid != pid)
        {
        if (cur->ovsdb_pid == 0)
          cur->ovsdb_pid = pid;
        if (cur->ovs_pid == 0)
          cur->ovs_pid = toppid;
        if ((cur->ovs_pid > 0) && (cur->ovsdb_pid > 0))
          cur->ovs_pid_ready = 1;
        if (g_system_promisc == 0)
          {
          g_system_promisc = 1;
          msg_send_system_promisc();
          }
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_rpct_recv_sigdiag_msg(int llid, int tid, char *line)
{
  t_ovs *cur = _ovs_find_with_llid(llid);
  if (!cur)
    KERR("%s", line);
  else
    {
    if (!strcmp(line, "ovs_resp_suidroot_ok"))
      {
      cur->getsuidroot = 1;
      event_print("Start OpenVSwitch %s getsuidroot = 1", cur->name);
      }
    else if (!strcmp(line, "ovs_resp_suidroot_ko"))
      {
      KERR("ERROR: cloonix_ovs is not suid root");
      KERR("sudo chmod u+s %s", pthexec_cloonfs_ovs_vswitchd());
      ovs_destroy();
      }
    else if (!strcmp(line, "ovs_resp_ovsdb_ko"))
      {
      KERR("ERROR ovs_resp_ovsdb_ko");
      ovs_destroy();
      }
    else if (!strncmp(line, "ovs_resp_ovsdb_ok", strlen("ovs_resp_ovsdb_ok")))
      {
      event_print("Start OpenVSwitch %s open_ovsdb = 1", cur->name);
      cur->open_ovsdb_ok = 1;
      }
    else if (!strcmp(line, "ovs_resp_ovs_ko"))
      {
      KERR("ERROR cloonix-ovs-vswitchd FAIL");
      ovs_destroy();
      }
    else if (!strncmp(line, "ovs_resp_ovs_ok", strlen("ovs_resp_ovs_ok")))
      {
      event_print("Start OpenVSwitch %s open_ovs = 1", cur->name);
      cur->open_ovs_ok = 1;
      }
    else
      fmt_rx_rpct_recv_sigdiag_msg(llid, tid, line);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int ovs_find_with_llid(int llid)
{
  int result = 0;
  t_ovs *mu = _ovs_find_with_llid(llid);
  if (mu)
    {
    result = 1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int ovs_try_send_sigdiag_msg(int tid, char *cmd)
{
  int result = -1;
  t_ovs *ovsdb = ovs_find();
  if ((ovsdb) && (ovsdb->ovs_pid_ready))
    result = try_send_msg_ovs(ovsdb, msg_type_diag, tid, cmd);
  else
    KERR("ERROR MSG DROPPED: %s", cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *ovs_format_ethv(t_vm *vm, int eth, char *ifname)
{
  static char cmd[MAX_PATH_LEN*3];
  int len = 0;
  unsigned char *mc = vm->kvm.eth_table[eth].mac_addr;
  memset(cmd, 0, MAX_PATH_LEN*3);
  len += sprintf(cmd+len,
         " -device virtio-net-pci-non-transitional,netdev=nvhost%d", eth);
  len += sprintf(cmd+len,",mac=%02X:%02X:%02X:%02X:%02X:%02X",
                         mc[0] & 0xFF, mc[1] & 0xFF, mc[2] & 0xFF,
                         mc[3] & 0xFF, mc[4] & 0xFF, mc[5] & 0xFF);
  len += sprintf(cmd+len,",addr=0x%x", eth+5);
  len += sprintf(cmd+len," -netdev type=tap,id=nvhost%d,vhost=on,", eth);
  len += sprintf(cmd+len,"ifname=%s,script=no,downscript=no", ifname);
  return cmd;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int ovs_still_present(void)
{
  int result = 0;
  if (g_head_ovs != NULL)
    {
    if (g_head_ovs->last_msg_req_destroy_sent == 0)
      result = 1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_start_openvswitch(void *data)
{
  ovs_start_openvswitch_if_not_done();
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int get_daemon_done(void)
{
  int result = 0;
  t_ovs *cur = g_head_ovs;
  if (cur)
    {
    if ((cur->daemon_done) && init_xwy_done() && (cur->ovs_pid_ready == 1))
      result = 1;
    } 
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int ovs_process_still_on(void)
{
  int i, nb, result = 0;
  t_pid_lst *lst = create_list_pid(&nb);
  if (nb)
    {
    for (i=0; i<nb; i++)
      {
      if ((strcmp(lst[i].name, "doors")) &&
          (strcmp(lst[i].name, "xwy")) &&
          (strcmp(lst[i].name, "suid_power")) &&
          (strcmp(lst[i].name, "cloonix_server")))
        {
        result = 1;
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void ovs_destroy_test(void)
{
  char *sock;
  t_ovs *cur = g_head_ovs;
  static int count_fails = 0;
  int keep_waiting = 1;
  if (!cur)
    KERR("ERROR: NULL ovs");
  else if (cur->destroy_requested)
    {
    /*---------------------------------------------------*/
    if (count_fails < 20)
      {
      if (!ovs_process_still_on())
        {
        keep_waiting = 0;
        }
      else
        {
        count_fails += 1;
        if (count_fails > 20)
          {
          KERR("ERROR NB FAILS MUST LOOK");
          keep_waiting = 0;
          }
        }
      }
    else
      keep_waiting = 1;
    /*---------------------------------------------------*/
    if (keep_waiting == 0)
      {
      if (cur->destroy_requested > 2)
        {
        cur->destroy_requested -= 1;
        }
      else if (cur->destroy_requested == 2)
        {
        if (cur->last_msg_req_destroy_sent != 1)
          {
          cur->last_msg_req_destroy_sent = 1;
          try_send_msg_ovs(cur, msg_type_diag, 0, "ovs_req_destroy");
          suid_power_self_kill();
          }
        cur->destroy_requested -= 1;
        } 
      else if (cur->destroy_requested == 1)
        { 
        if (cur->llid)
          {
          llid_trace_free(cur->llid, 0, __FUNCTION__);
          cur->llid = 0;
          }
        if (cur->ovsdb_pid > 0)
          {
          if (!kill(cur->ovsdb_pid, SIGKILL))
            KERR("ERROR: ovsdb was alive");
          cur->ovsdb_pid = 0;
          }
        if (cur->ovs_pid > 0)
          {
          if (!kill(cur->ovs_pid, SIGKILL))
            KERR("ERROR: ovs was alive");
          cur->ovs_pid = 0;
          }
        sock = utils_get_ovs_path(cur->name);
        if (!access(sock,R_OK))
          unlink(sock);
        }
      }
    /*---------------------------------------------------*/
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_destroy(void)
{
  t_ovs *cur = g_head_ovs;
  if (!cur)
    KERR("ERROR: NULL ovs");
  else if ((!cur->last_msg_req_destroy_sent) && (!cur->destroy_requested))
    {
    if (cur->llid == 0)
      {
      KERR("ERROR: LINK TO OVS DEAD");
      cur->destroy_requested = 4;
      }
    else
      {
      cur->destroy_requested = 6;
      }
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_llid_closed(int llid, int from_clone)
{
  t_ovs *cur = g_head_ovs;
  if ((!from_clone) && (cur) && (cur->llid == llid))
    {
    cur->llid = 0;
    ovs_destroy();
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_init(void)
{
  g_head_ovs = NULL;
  g_system_promisc = 0;
  clownix_timeout_add(5, timer_ovs_beat, NULL, NULL, NULL);
  clownix_timeout_add(1, timer_start_openvswitch, NULL, NULL, NULL);
  kvm_init();
  msg_init();
}
/*--------------------------------------------------------------------------*/

