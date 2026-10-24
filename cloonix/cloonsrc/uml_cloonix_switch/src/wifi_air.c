/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* Airs of the net, the names radios are on: suid_power writes snf/<air>_0,
   served by a cloonix-dumpcap as capture "<air> 0"; cnt radios start on
   cntwifi, kvm radios on kvmwifi. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_daemon.h"
#include "cfg_store.h"
#include "cnt.h"
#include "lan_to_name.h"
#include "pid_clone.h"
#include "suid_power.h"
#include "utils_cmd_line_maker.h"
#include "wifi_air.h"
#include "doorways_mngt.h"
#include "doors_rpc.h"
#include "ovs_a2b.h"
#include "ovs_c2c.h"
#include "ovs_nat_main.h"
#include "ovs_phy.h"
#include "ovs_tap.h"

#define MAX_AIRS 32
#define AIR_RESTART_MAX 5
#define AIR_FIFO_WAIT 10

typedef struct t_air_cap
{
  char name[MAX_NAME_LEN];
  int pid;
  int serial;
  int restarts;
  int no_fifo;
  int snf;
  int settle;
} t_air_cap;

static t_air_cap g_air[MAX_AIRS];
static int g_serial;

/* A guest being deleted has left its air. */
static int cnt_radio_air(char *air)
{
  int i, nb;
  t_cnt *cur = cnt_get_first_cnt(&nb);
  while (cur)
    {
    for (i = 0; (!cur->poweroff_done) && (i < cur->cnt.nb_tot_nb_vwif); i++)
      if (!strcmp(cur->cnt.vwif_air[i], air))
        return 1;
    cur = cur->next;
    }
  return 0;
}

static int kvm_radio_air(char *air)
{
  int i, nb;
  t_vm *vm = cfg_get_first_vm(&nb);
  while (vm)
    {
    for (i = 0; (!vm->vm_to_be_killed) && (i < vm->kvm.nb_tot_nb_vwif); i++)
      if (!strcmp(vm->kvm.vwif_air[i], air))
        return 1;
    vm = vm->next;
    }
  return 0;
}

int wifi_air_in_use(char *air)
{
  return (air[0] && (cnt_radio_air(air) || kvm_radio_air(air)));
}

static t_air_cap *cap_find(char *air)
{
  int i;
  for (i = 0; i < MAX_AIRS; i++)
    if ((g_air[i].name[0]) && (!strcmp(g_air[i].name, air)))
      return &(g_air[i]);
  return NULL;
}

int wifi_air_is_up(char *air)
{
  t_air_cap *cap = cap_find(air);
  return ((cap) && (cap->pid != 0));
}

static void fifo_path(char *air, char *path);
static void air_snf_tell(char *air, int on);

/* the capture record of an air, born not captured, gone with the air */
static void cap_ensure(char *air)
{
  int i;
  if (cap_find(air))
    return;
  for (i = 0; (i < MAX_AIRS) && (g_air[i].name[0]); i++);
  if (i == MAX_AIRS)
    KERR("WARNING air %s: %d airs captured already, not this one",
         air, MAX_AIRS);
  else
    strncpy(g_air[i].name, air, MAX_NAME_LEN-1);
}

static void dumpcap_death(void *data, int status, char *name)
{
  int i, serial = (int) (intptr_t) data;
  char fifo[MAX_PATH_LEN];
  struct stat st;
  for (i = 0; i < MAX_AIRS; i++)
    {
    if ((g_air[i].name[0]) && (g_air[i].serial == serial))
      {
      g_air[i].pid = 0;
      fifo_path(g_air[i].name, fifo);
      /* the last radio left the air: suid_power took its fifo away */
      if (!wifi_air_in_use(g_air[i].name))
        continue;
      if (stat(fifo, &st))
        {
        KERR("WARNING air %s: its dumpcap ended (status %d), %s is gone "
             "while a radio is on the air", g_air[i].name, status, fifo);
        continue;
        }
      g_air[i].restarts += 1;
      KERR("WARNING air %s: its dumpcap ended (status %d)%s", g_air[i].name,
           status, (g_air[i].restarts >= AIR_RESTART_MAX) ?
           ", nothing captures it now" : ", started again");
      /* the writer is mid-stream: reopened, the next dumpcap gets a header */
      if (g_air[i].restarts < AIR_RESTART_MAX)
        {
        air_snf_tell(g_air[i].name, 1);
        g_air[i].settle = 1;
        }
      }
    }
}

static void fifo_path(char *air, char *path)
{
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "%s/%s_0", utils_get_snf_pcap_dir(), air);
}

static void dumpcap_start(t_air_cap *cap)
{
  static char *argv[4];
  static char fifo[MAX_PATH_LEN];
  char *bin = utils_get_dumpcap_bin_dir();
  struct stat st;
  fifo_path(cap->name, fifo);
  /* made by suid_power with the first radio on the air, within a beat */
  if ((stat(fifo, &st)) || (!S_ISFIFO(st.st_mode)))
    {
    cap->no_fifo += 1;
    if (cap->no_fifo == AIR_FIFO_WAIT)
      KERR("WARNING air %s: %s is not there after %d s, the air is not "
           "captured until it is", cap->name, fifo, AIR_FIFO_WAIT);
    return;
    }
  if (cap->no_fifo >= AIR_FIFO_WAIT)
    KERR("WARNING air %s: %s is there now, captured", cap->name, fifo);
  cap->no_fifo = 0;
  if (access(bin, X_OK))
    {
    KERR("ERROR air %s: %s not found, the air is not captured", cap->name,
         bin);
    cap->restarts = AIR_RESTART_MAX;
    return;
    }
  argv[0] = bin;
  argv[1] = "-i";
  argv[2] = fifo;
  argv[3] = NULL;
  g_serial += 1;
  cap->serial = g_serial;
  cap->pid = pid_clone_launch(utils_execv, dumpcap_death, NULL,
                              (void *) argv, (void *) (intptr_t) g_serial,
                              NULL, cap->name, -1, 1);
  if (cap->pid <= 0)
    {
    KERR("ERROR air %s: could not start %s -i %s", cap->name, bin, fifo);
    cap->pid = 0;
    cap->restarts += 1;
    }
}

/* Once a second from the heartbeat. */
void wifi_air_beat(void)
{
  int i, j, nb;
  t_cnt *cur = cnt_get_first_cnt(&nb);
  t_vm *vm = cfg_get_first_vm(&nb);
  t_air_cap *cap;
  for (i = 0; i < MAX_AIRS; i++)
    {
    if ((g_air[i].name[0]) && (!wifi_air_in_use(g_air[i].name)))
      {
      if ((g_air[i].pid) && (kill(g_air[i].pid, SIGTERM)))
        KERR("WARNING air %s: its dumpcap pid %d could not be stopped: %s",
             g_air[i].name, g_air[i].pid, strerror(errno));
      if (g_air[i].snf)
        air_snf_tell(g_air[i].name, 0);
      memset(&(g_air[i]), 0, sizeof(t_air_cap));
      }
    }
  while (cur)
    {
    for (j = 0; (!cur->poweroff_done) && (j < cur->cnt.nb_tot_nb_vwif); j++)
      {
      if (cur->cnt.vwif_air[j][0])
        cap_ensure(cur->cnt.vwif_air[j]);
      }
    cur = cur->next;
    }
  while (vm)
    {
    for (j = 0; (!vm->vm_to_be_killed) && (j < vm->kvm.nb_tot_nb_vwif); j++)
      {
      if (vm->kvm.vwif_air[j][0])
        cap_ensure(vm->kvm.vwif_air[j]);
      }
    vm = vm->next;
    }
  for (i = 0; i < MAX_AIRS; i++)
    {
    cap = &(g_air[i]);
    /* a beat skipped after a death: suid_power has closed its writer */
    if (cap->settle)
      cap->settle = 0;
    else if ((cap->name[0]) && (cap->snf) && (!cap->pid) &&
             (cap->restarts < AIR_RESTART_MAX))
      dumpcap_start(cap);
    }
}

static void refuse_air(int llid, int tid, char *txt);

static void air_snf_tell(char *air, int on)
{
  char req[MAX_PATH_LEN];
  memset(req, 0, MAX_PATH_LEN);
  snprintf(req, MAX_PATH_LEN-1, "cloonsuid_air_snf air=%s on=%d", air, on);
  if (suid_power_air_req(req))
    KERR("ERROR air %s: suid_power not reachable, capture %s not told", air,
         on ? "on" : "off");
}

/* cnf <air> sniff=on|off: the capture of an air, off at its birth; ok once
   its dumpcap runs, or is stopped */
void wifi_air_snf(int llid, int tid, char *air, int on)
{
  t_air_cap *cap;
  char err[MAX_PATH_LEN];
  memset(err, 0, MAX_PATH_LEN);
  cap_ensure(air);
  cap = cap_find(air);
  if (!cap)
    snprintf(err, MAX_PATH_LEN-1, "cnf %s sniff=on refused: %d airs captured "
             "already", air, MAX_AIRS);
  else if (on)
    {
    cap->snf = 1;
    cap->restarts = 0;
    cap->no_fifo = 0;
    /* a running dumpcap is kept: telling would cut its stream */
    if (!cap->pid)
      {
      air_snf_tell(air, 1);
      dumpcap_start(cap);
      }
    if (!cap->pid)
      snprintf(err, MAX_PATH_LEN-1, "cnf %s sniff=on: its capture did not "
               "start, the reason is in the syslog", air);
    }
  else
    {
    cap->snf = 0;
    air_snf_tell(air, 0);
    if ((cap->pid) && (kill(cap->pid, SIGTERM)))
      KERR("WARNING air %s: its dumpcap pid %d could not be stopped: %s",
           air, cap->pid, strerror(errno));
    cap->pid = 0;
    cap->serial = 0;
    }
  /* the topology carries it, in the radios on the air */
  cfg_hysteresis_send_topo_info();
  if (err[0])
    refuse_air(llid, tid, err);
  else
    send_status_ok(llid, tid, "OK");
}

int wifi_air_snf_on(char *air)
{
  t_air_cap *cap = cap_find(air);
  return ((cap) && (cap->snf));
}

/* the airs captured, for dump commands */
int wifi_air_snf_names(char names[][MAX_NAME_LEN], int max)
{
  int i, nb = 0;
  for (i = 0; (i < MAX_AIRS) && (nb < max); i++)
    if ((g_air[i].name[0]) && (g_air[i].snf))
      {
      memset(names[nb], 0, MAX_NAME_LEN);
      strncpy(names[nb], g_air[i].name, MAX_NAME_LEN-1);
      nb += 1;
      }
  return nb;
}

/* the eths and radios of a guest, 0 for no guest */
static int guest_ifaces(char *name, int *nb_eth, int *nb_vwif)
{
  t_cnt *cnt = find_cnt(name);
  t_vm *vm = cfg_get_vm(name);
  if (vm)
    {
    *nb_eth = vm->kvm.nb_tot_eth;
    *nb_vwif = vm->kvm.nb_tot_nb_vwif;
    return 1;
    }
  if (cnt)
    {
    *nb_eth = cnt->cnt.nb_tot_eth;
    *nb_vwif = cnt->cnt.nb_tot_nb_vwif;
    return 1;
    }
  return 0;
}

/* A wire num: wlan<i> is WLAN_NUM_BASE + i, made the number after the eths
   of the guest, -1 when it has no such radio; any other num as it is. */
int wifi_air_num(char *name, int num)
{
  int nb_eth, nb_vwif;
  if (num < WLAN_NUM_BASE)
    return num;
  if ((!guest_ifaces(name, &nb_eth, &nb_vwif)) ||
      (num - WLAN_NUM_BASE >= nb_vwif))
    return -1;
  return nb_eth + num - WLAN_NUM_BASE;
}

/* An interface as the user writes it: eth<i>/wlan<i> of a guest, A/B of an
   a2b, nothing for a tap, nat, c2c or phy. */
char *wifi_air_iface(char *name, int num)
{
  static char buf[4][MAX_NAME_LEN];
  static int turn;
  char *s = buf[turn];
  int nb_eth, nb_vwif;
  turn = (turn + 1) % 4;
  memset(s, 0, MAX_NAME_LEN);
  if (num >= WLAN_NUM_BASE)
    snprintf(s, MAX_NAME_LEN-1, " wlan%d", num - WLAN_NUM_BASE);
  else if (guest_ifaces(name, &nb_eth, &nb_vwif))
    {
    if ((num >= nb_eth) && (num < nb_eth + nb_vwif))
      snprintf(s, MAX_NAME_LEN-1, " wlan%d", num - nb_eth);
    else
      snprintf(s, MAX_NAME_LEN-1, " eth%d", num);
    }
  else if (ovs_a2b_exists(name) && ((num == 0) || (num == 1)))
    snprintf(s, MAX_NAME_LEN-1, " %c", 'A' + num);
  else if (num != 0)
    snprintf(s, MAX_NAME_LEN-1, " %d", num);
  return s;
}

/* A wlan: a number past the eths of a guest, up to its radios. */
int wifi_air_wlan(char *name, int num, int *idx)
{
  t_cnt *cur = find_cnt(name);
  t_vm *vm = cfg_get_vm(name);
  if ((vm) && (num >= vm->kvm.nb_tot_eth) &&
      (num < vm->kvm.nb_tot_eth + vm->kvm.nb_tot_nb_vwif))
    {
    *idx = num - vm->kvm.nb_tot_eth;
    return 1;
    }
  if ((!cur) || (num < cur->cnt.nb_tot_eth) ||
      (num >= cur->cnt.nb_tot_eth + cur->cnt.nb_tot_nb_vwif))
    return 0;
  *idx = num - cur->cnt.nb_tot_eth;
  return 1;
}

static void refuse_air(int llid, int tid, char *txt)
{
  KERR("ERROR %s", txt);
  send_status_ko(llid, tid, txt);
}

/* kvm airs */
/* An air change of a kvm radio waits for suid_power to say it is done. */
#define MAX_PEND 16
#define PEND_WAIT 300
typedef struct t_pend
{
  char name[MAX_NAME_LEN];
  int idx;
  int llid;
  int tid;
  long long abeat;
  int ref;
} t_pend;
static t_pend g_pend[MAX_PEND];

static t_pend *pend_find(char *name, int idx)
{
  int i;
  for (i = 0; i < MAX_PEND; i++)
    if ((g_pend[i].name[0]) && (g_pend[i].idx == idx) &&
        (!strcmp(g_pend[i].name, name)))
      return &(g_pend[i]);
  return NULL;
}

static void pend_timeout(void *data)
{
  t_pend *p = (t_pend *) data;
  char txt[MAX_PATH_LEN];
  snprintf(txt, MAX_PATH_LEN-1, "%s wlan%d: suid_power did not confirm the "
           "air in %d s", p->name, p->idx, PEND_WAIT / 100);
  KERR("ERROR %s", txt);
  send_status_ko(p->llid, p->tid, txt);
  memset(p, 0, sizeof(t_pend));
}

/* the radio made in suid_power the first time, its air set */
static int kvm_air_send(char *name, int idx, char *air)
{
  unsigned char m[6];
  char req[MAX_PATH_LEN];
  utils_derive_mac(m, 0x2, name, 0x80);
  memset(req, 0, MAX_PATH_LEN);
  snprintf(req, MAX_PATH_LEN-1, "cloonsuid_air_kvm name=%s idx=%d "
           "mac=%02x:%02x:%02x:%02x:%02x:%02x air=%s", name, idx, m[0], m[1],
           m[2], m[3], m[4], (m[5] + idx) & 0xff, air[0] ? air : "-");
  return suid_power_air_req(req);
}

/* the air set at once, the answer on the word of doorways */
static void kvm_air_ask(int llid, int tid, t_vm *vm, int idx, char *air,
                        char *err)
{
  int i;
  for (i = 0; (i < MAX_PEND) && (g_pend[i].name[0]); i++);
  if (pend_find(vm->kvm.name, idx))
    snprintf(err, MAX_PATH_LEN-1, "%s wlan%d: an air change is under way",
             vm->kvm.name, idx);
  else if (i == MAX_PEND)
    snprintf(err, MAX_PATH_LEN-1, "%s wlan%d: %d air changes under way",
             vm->kvm.name, idx, MAX_PEND);
  else if (kvm_air_send(vm->kvm.name, idx, air))
    snprintf(err, MAX_PATH_LEN-1, "suid_power not reachable, %s air fail",
             vm->kvm.name);
  else
    {
    memset(vm->kvm.vwif_air[idx], 0, MAX_NAME_LEN);
    strncpy(vm->kvm.vwif_air[idx], air, MAX_NAME_LEN-1);
    strncpy(g_pend[i].name, vm->kvm.name, MAX_NAME_LEN-1);
    g_pend[i].idx = idx;
    g_pend[i].llid = llid;
    g_pend[i].tid = tid;
    clownix_timeout_add(PEND_WAIT, pend_timeout, (void *) &(g_pend[i]),
                        &(g_pend[i].abeat), &(g_pend[i].ref));
    }
}

/* the answer of suid_power; unasked after a declaration, nominal */
void wifi_air_kvm_done(char *line)
{
  char name[MAX_NAME_LEN], air[MAX_NAME_LEN], txt[MAX_PATH_LEN];
  char *why = strstr(line, "reason=");
  t_pend *p;
  int idx, ok;
  memset(name, 0, MAX_NAME_LEN);
  memset(air, 0, MAX_NAME_LEN);
  if (sscanf(line, "cloonsuid_air_kvm_resp_ok name=%63s idx=%d air=%63s",
             name, &idx, air) == 3)
    ok = 1;
  else if (sscanf(line, "cloonsuid_air_kvm_resp_ko name=%63s idx=%d",
                  name, &idx) == 2)
    ok = 0;
  else
    {
    KERR("ERROR %s", line);
    return;
    }
  if (!ok)
    KERR("ERROR %s", line);
  cfg_hysteresis_send_topo_info();
  p = pend_find(name, idx);
  if (p)
    {
    clownix_timeout_del(p->abeat, p->ref, __FILE__, __LINE__);
    if (ok)
      send_status_ok(p->llid, p->tid, "OK");
    else
      {
      snprintf(txt, MAX_PATH_LEN-1, "%s wlan%d: %s", name, idx,
               why ? why + 7 : "ko");
      KERR("ERROR %s", txt);
      send_status_ko(p->llid, p->tid, txt);
      }
    memset(p, 0, sizeof(t_pend));
    }
}

/* At each declaration of the backdoor of a kvm: its radios and their airs
   told suid_power, then doorways, which asks the agent until it answers. */
void wifi_air_kvm_radio(t_vm *vm)
{
  unsigned char m[6];
  char cmd[MAX_PATH_LEN];
  int i;
  if (!vm->kvm.nb_tot_nb_vwif)
    return;
  for (i = 0; i < vm->kvm.nb_tot_nb_vwif; i++)
    if (kvm_air_send(vm->kvm.name, i, vm->kvm.vwif_air[i]))
      KERR("ERROR %s wlan%d: suid_power not reachable, the radio is on no "
           "air", vm->kvm.name, i);
  utils_derive_mac(m, 0x2, vm->kvm.name, 0x80);
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, KVMWIFI_ON_REQ, vm->kvm.nb_tot_nb_vwif,
           m[0], m[1], m[2], m[3], m[4], m[5]);
  doors_send_command(get_doorways_llid(), 0, vm->kvm.name, cmd);
}

/* a kvm deleted: its radios leave suid_power */
void wifi_air_kvm_gone(t_vm *vm)
{
  char req[MAX_PATH_LEN];
  if (!vm->kvm.nb_tot_nb_vwif)
    return;
  memset(req, 0, MAX_PATH_LEN);
  snprintf(req, MAX_PATH_LEN-1, "cloonsuid_air_kvm_del name=%s",
           vm->kvm.name);
  if (suid_power_air_req(req))
    KERR("ERROR %s: suid_power not reachable, its radios stay there",
         vm->kvm.name);
}

/* the air of a new kvm radio */
void wifi_air_kvm_default(t_topo_kvm *kvm)
{
  int i;
  memset(kvm->vwif_air, 0, sizeof(kvm->vwif_air));
  for (i = 0; (i < kvm->nb_tot_nb_vwif) && (i < MAX_VWIF); i++)
    strncpy(kvm->vwif_air[i], KVMWIFI_LAN, MAX_NAME_LEN-1);
}

/* plugging */
static int air_reserved(char *air)
{
  return ((!strcmp(air, CNTWIFI_LAN)) || (!strcmp(air, KVMWIFI_LAN)));
}

/* plug <guest> <wlan> <air> or unplug, <num> a wlan; ok on the answer
   of suid_power */
void wifi_air_plug(int llid, int tid, char *name, int num, char *air,
                   int add)
{
  t_cnt *cnt = find_cnt(name);
  t_vm *vm = cfg_get_vm(name);
  char err[MAX_PATH_LEN];
  char *now = NULL;
  int idx;
  memset(err, 0, MAX_PATH_LEN);
  if (wifi_air_wlan(name, num, &idx))
    {
    if (vm)
      now = vm->kvm.vwif_air[idx];
    else if (cnt)
      now = cnt->cnt.vwif_air[idx];
    }
  if (!now)
    snprintf(err, MAX_PATH_LEN-1, "%s%s is not a wlan", name,
             wifi_air_iface(name, num));
  else if ((!add) && strcmp(now, air))
    snprintf(err, MAX_PATH_LEN-1, "unplug %s wlan%d %s refused: wlan%d is "
             "on %s%s", name, idx, air, idx, now[0] ? "the air " : "no air",
             now);
  else if ((add) && (!name_is_sane(air)))
    snprintf(err, MAX_PATH_LEN-1, "plug %s wlan%d %s refused: bad air "
             "name", name, idx, air);
  else if ((add) && (lan_get_with_name(air)))
    snprintf(err, MAX_PATH_LEN-1, "plug %s wlan%d %s refused: %s is a lan "
             "of eths, a wlan goes on an air", name, idx, air, air);
  else if ((add) && cfg_name_is_iface(air))
    snprintf(err, MAX_PATH_LEN-1, "plug %s wlan%d %s refused: %s is the word "
             "of an interface, an air cannot take it", name, idx, air, air);
  else if ((add) && (!air_reserved(air)) && cfg_name_is_in_use(0, air, err))
    ;
  else if ((add) && (!strcmp(now, air)))
    {
    send_status_ok(llid, tid, "already on this air");
    return;
    }
  else if (vm)
    kvm_air_ask(llid, tid, vm, idx, add ? air : "", err);
  else
    suid_power_air_cnt(llid, tid, name, idx, add ? air : "", err);
  if (err[0])
    refuse_air(llid, tid, err);
}
