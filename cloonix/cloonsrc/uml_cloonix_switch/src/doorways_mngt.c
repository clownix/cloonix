/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <errno.h>
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "pid_clone.h"
#include "lan_to_name.h"
#include "doors_rpc.h"
#include "event_subscriber.h"
#include "llid_trace.h"
#include "utils_cmd_line_maker.h"
#include "system_callers.h"
#include "automates.h"
#include "doorways_mngt.h"
#include "qmp.h"
#include "hop_event.h"
#include "uml_clownix_switch.h"
#include "suid_power.h"
#include "qga_dialog.h"
#include "commun_daemon.h"
#include "cnt.h"
#include "wifi_air.h"

void timer_utils_finish_vm_init(int vm_id, int val);

int get_killing_cloonix(void);

static void doorways_start(void);
static int g_killed;

void last_action_self_destruction(void *data);

static int g_nb_pid_resp;
static int g_nb_pid_resp_warning;
static int g_this_is_not_first_start;
static int g_doorways_pid;
static int g_doorways_llid;
static long long g_abs_beat_timer;
static int  g_ref_timer;
static char g_bin_doorways[MAX_PATH_LEN];
static char g_root_work[MAX_PATH_LEN];
static char g_password[MSG_DIGEST_LEN];
static char g_net_name[MAX_NAME_LEN];

void doors_recv_event(int llid, int tid, char *name, char *line)
{
  t_vm   *vm = cfg_get_vm(name);
  int nb_eth, cnt_exists = cnt_name_exists(name, &nb_eth);
  if ((!vm) && (!cnt_exists))
    {
    if (strcmp(line, BACKDOOR_DISCONNECTED))
      KERR("ERROR FROM DOORS, %s is not a guest of this net: %s", name, line);
    else
      KTRC("doors reports %s disconnected after it was deleted", name);
    doors_send_del_vm(get_doorways_llid(), 0, name);
    }
  else
    {
    if (!strcmp(line, BACKDOOR_CONNECTED))
      {
      if (vm == NULL)
        cnt_doorways_up_and_running(name);
      else
        qga_event_backdoor(name, backdoor_evt_connected);
      }
    else if (!strcmp(line, BACKDOOR_DISCONNECTED))
      {
      if (vm)
        qga_event_backdoor(name, backdoor_evt_disconnected);
      }
    else if (!strcmp(line, PING_OK))
      {
      if (vm == NULL)
        cnt_doorways_ping_ok(name);
      else
        {
        timer_utils_finish_vm_init(vm->kvm.vm_id, 1);
        qga_event_backdoor(name, backdoor_evt_ping_ok);
        }
      }
    else if (!strcmp(line, PING_KO))
      {
      if (vm == NULL)
        cnt_doorways_ping_ko(name);
      else
        qga_event_backdoor(name, backdoor_evt_ping_ko);
      }
    /* gated per guest by doorways: every line is written */
    else if (!strncmp(line, AGENT_KERR, strlen(AGENT_KERR)))
      cloonix_log(0, "KERR: %s guest %s: %s\n", cfg_get_cloonix_name(), name,
                  line + strlen(AGENT_KERR));
    else if (!strncmp(line, CLIPBOARD_TXT, strlen(CLIPBOARD_TXT)))
      {
      KERR("WARNING %s sent a clipboard reply nothing asked for", name);
      }
    else
      KOUT("ERROR %s", line);
    } 
}

static void killed(void *unused_data, int status, char *name)
{
  g_killed = 1;
}

void doorways_err_cb (int llid)
{
  if (llid == g_doorways_llid)
    {
    g_doorways_llid = 0;
    g_doorways_pid = 0;
    if (get_glob_req_self_destruction())
      return;
    if (get_killing_cloonix())
      return;
    KERR("ERROR Ctrl access to cloonix-doorways broken, re-launching\n");
    doorways_start();
    }
}

static void monitoring_doorways(void *data)
{
  static int old_nb_pid_resp;
  if (old_nb_pid_resp == g_nb_pid_resp)
    g_nb_pid_resp_warning++;
  else
    g_nb_pid_resp_warning = 0;
  if (g_nb_pid_resp_warning > 4) 
    {
    KERR("ERROR TRY KILLING DOORWAYS");
    if (!kill(old_nb_pid_resp, SIGKILL))
      KERR("ERROR NO SUCCESS KILLING DOORWAYS");
    }
  g_abs_beat_timer = 0;
  g_ref_timer = 0;
  clownix_timeout_add(500, monitoring_doorways, NULL, 
                   &(g_abs_beat_timer), &(g_ref_timer));
  rpct_send_pid_req(g_doorways_llid, type_hop_doors, "doors", 0);
  old_nb_pid_resp = g_nb_pid_resp;
}

/* A relaunched doorways knows no guest: every backdoor is declared again. */
static void doors_readd_all_guests(int llid)
{
  int nb;
  char sock[MAX_PATH_LEN];
  t_cnt *cnt = cnt_get_first_cnt(&nb);
  t_vm *vm = cfg_get_first_vm(&nb);
  while (cnt)
    {
    memset(sock, 0, MAX_PATH_LEN);
    snprintf(sock, MAX_PATH_LEN-1, "%s/mountbear/sock", cnt->mountbear);
    doors_send_add_vm(llid, 0, cnt->cnt.name, sock);
    cnt = cnt->next;
    }
  while (vm)
    {
    if (!(vm->kvm.vm_config_flags & VM_CONFIG_FLAG_NO_QEMU_GA))
      {
      doors_send_add_vm(llid, 0, vm->kvm.name,
                        utils_get_qbackdoor_path(vm->kvm.vm_id));
      wifi_air_kvm_radio(vm);
      }
    vm = vm->next;
    }
}

void doors_pid_resp(int llid, char *name, int pid)
{
  if (strcmp(name, "doors"))
    KERR("ERROR %s", name); 
  if (g_doorways_pid != pid)
    {
    if (g_this_is_not_first_start)
      {
      KERR("WARNING DOORWAYS PID CHANGE: %d to %d, telling it the guests "
           "again", g_doorways_pid, pid);
      doors_readd_all_guests(llid);
      }
    }
  g_doorways_pid = pid;
  g_nb_pid_resp++;
  if (g_this_is_not_first_start == 0)
    {
    g_this_is_not_first_start = 1;
    suid_power_first_start();
    }
}

static void timer_doorways_protect(void *data)
{
  if (!g_killed)
    KERR("ERROR ");
}

static void timer_doorways_connect(void *data)
{
  int llid;
  static int count = 0;
  char *ctrl = (char *) data;
  llid = string_client_unix(ctrl, uml_clownix_switch_error_cb,
                                  uml_clownix_switch_rx_cb, "doorways");
  if (llid)
    {
    g_doorways_llid = llid;
    llid_trace_alloc(llid, "DOORWAYS", 0,0,type_llid_trace_doorways);
    rpct_send_pid_req(llid, type_hop_doors, "doors", 0);
    if (g_abs_beat_timer)
      clownix_timeout_del(g_abs_beat_timer, g_ref_timer, __FILE__, __LINE__);
    clownix_timeout_add(500, monitoring_doorways, NULL, 
                   &(g_abs_beat_timer), &(g_ref_timer));
    g_doorways_pid = 0;
    g_nb_pid_resp_warning = 0;
    }
  else
    {
    count++;
    if (count < 100)
      clownix_timeout_add(50, timer_doorways_connect, data, NULL, NULL);
    else
      {
      KERR("ERROR doorswaitgivingup");
      last_action_self_destruction(NULL);
      }
    }
}

static void doorways_start()
{
  static char *argv[] ={
                       g_bin_doorways,
                       g_net_name,
                       g_root_work,
                       g_password,
                       NULL
                       };
  char *ctrl_doors_sock = cfg_get_ctrl_doors_sock();
  if ((!strlen(g_bin_doorways)) ||
      (!strlen(g_net_name))     ||
      (!strlen(g_root_work))    ||
      (!strlen(g_password)))
    KOUT("ERROR");
  if (get_glob_req_self_destruction())
      return;
  if (get_killing_cloonix())
      return;
  g_killed = 0;
  pid_clone_launch(utils_execv, killed, NULL, (void *) argv,
                   NULL, NULL, "doorways", -1, 1);
  clownix_timeout_add(500, timer_doorways_protect, NULL, NULL, NULL); 
  clownix_timeout_add(50, timer_doorways_connect, 
                      (void *) ctrl_doors_sock, NULL, NULL);
}

int doorways_get_distant_pid(void)
{
  return g_doorways_pid;
}

int get_doorways_llid(void)
{
  return g_doorways_llid;
}

void doorways_first_start(void)
{
  doorways_start();
}

void kill_doors(void)
{
  rpct_send_kil_req(g_doorways_llid, 0);
}

void doorways_init(char *net, char *root_work, char *password)
{
  memset(g_bin_doorways, 0, MAX_PATH_LEN);
  memset(g_root_work, 0, MAX_PATH_LEN);
  memset(g_net_name, 0, MAX_NAME_LEN);
  memset(g_password, 0, MSG_DIGEST_LEN);
  g_nb_pid_resp = 0;
  g_nb_pid_resp_warning = 0;
  g_this_is_not_first_start = 0;
  g_doorways_pid = 0;
  g_doorways_llid = 0;
  g_abs_beat_timer = 0;
  g_ref_timer = 0;
  sprintf(g_bin_doorways, "%s", pthexec_cloonfs_doorways());
  strncpy(g_root_work, root_work, MAX_PATH_LEN-1);
  snprintf(g_net_name, MAX_NAME_LEN-1, "%s", net);
  strncpy(g_password, password, MSG_DIGEST_LEN-1);
  doors_json_init();
}

