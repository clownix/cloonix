/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "commun_daemon.h"
#include "heartbeat.h"
#include "machine_create.h"
#include "event_subscriber.h"
#include "pid_clone.h"
#include "system_callers.h"
#include "automates.h"
#include "lan_to_name.h"
#include "utils_cmd_line_maker.h"
#include "file_read_write.h"
#include "qmp.h"
#include "doorways_mngt.h"
#include "doors_rpc.h"
#include "hop_event.h"
#include "ovs.h"
#include "kvm.h"
#include "suid_power.h"
#include "list_commands.h"
#include "cnt.h"
#include "ovs_snf.h"
#include "ovs_nat_main.h"
#include "ovs_a2b.h"
#include "ovs_tap.h"
#include "ovs_phy.h"
#include "ovs_c2c.h"
#include "msg.h"
#include "crun.h"
#include "proxymous.h"
#include "cloonix_conf_info.h"
#include "wifi_air.h"

static void local_add_lan(int llid, int tid, char *name, int num, char *lan);
int get_inhib_new_clients(void);
int get_uml_cloonix_started(void);

#define LAN_TRACE 0

#if LAN_TRACE
static int g_lan_seen;
static int g_lan_done;
#define LLOG(fmt, ...) KERR("LANTRACE " fmt, ##__VA_ARGS__)
#else
#define LLOG(fmt, ...) do {} while(0)
#endif

#define COHERENCY_WAIT_MAX 500

typedef struct t_coherency_delay
{   
  int llid;
  int tid;
  char name[MAX_NAME_LEN];
  int  num;
  char lan[MAX_NAME_LEN];
  int  count;
  struct t_coherency_delay *prev;
  struct t_coherency_delay *next;
} t_coherency_delay;
typedef struct t_timer_endp
{
  int existed_when_asked;
  int  timer_lan_ko_resp;
  int  llid;
  int  tid;
  char name[MAX_NAME_LEN];
  int  num;
  char lan[MAX_NAME_LEN];
  int  count;
  unsigned long asked_at_ms;
  /* Set when the name is not in the config at all (not merely unready). */
  int waited_while_absent;
  int  cancelled;
  struct t_timer_endp *prev;
  struct t_timer_endp *next;
} t_timer_endp;
typedef struct t_shadown_lan
{
  char name[MAX_NAME_LEN];
  int num;
  char lan[MAX_NAME_LEN];
  struct t_shadown_lan *prev;
  struct t_shadown_lan *next;
} t_shadown_lan;
static t_coherency_delay *g_head_coherency;
static t_timer_endp *g_head_timer;
static int glob_coherency;
static int glob_coherency_fail_count;
static long long g_coherency_abs_beat_timer;
static int g_coherency_ref_timer;
static t_shadown_lan *g_head_shadown_lan;
static int g_nb_shadown_lan;

static void shadown_lan_alloc(char *name, int num, char *lan)
{
  t_shadown_lan *cur = (t_shadown_lan *) malloc(sizeof(t_shadown_lan));
  memset(cur, 0, sizeof(t_shadown_lan));
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  strncpy(cur->lan, lan, MAX_NAME_LEN-1);
  cur->num = num;
  if (g_head_shadown_lan)
    g_head_shadown_lan->prev = cur;
  cur->next = g_head_shadown_lan;
  g_head_shadown_lan = cur;
  g_nb_shadown_lan += 1;
}

static void shadown_lan_free(t_shadown_lan *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (g_head_shadown_lan == cur)
    g_head_shadown_lan = g_head_shadown_lan->next;
  g_nb_shadown_lan -= 1;
  free(cur);
}

/* The shadow of a c2c goes with the c2c: kept across its restarts, freed  */
/* with its record, or the next c2c of that name is cabled to an old lan.  */
void shadown_lan_del_name(char *name)
{
  t_shadown_lan *next, *cur = g_head_shadown_lan;
  while (cur)
    {
    next = cur->next;
    if (!strcmp(cur->name, name))
      {
      KTRC("c2c %s: its shadow lan %s freed with it", name, cur->lan);
      shadown_lan_free(cur);
      }
    cur = next;
    }
}

void shadown_lan_del_all(void)
{
  t_shadown_lan *next, *cur = g_head_shadown_lan;
  while (cur)
    {
    next = cur->next;
    shadown_lan_free(cur);
    cur = next;
    }
}

static t_shadown_lan *shadown_lan_find(char *name, int num, char *lan)
{
  t_shadown_lan *cur = g_head_shadown_lan;
  while (cur)
    {
    if ((strlen(name)) && (strlen(lan)) &&
        (!strcmp(cur->name, name)) &&
        (cur->num == num) &&
        (!strcmp(cur->lan, lan)))
      break;
    cur = cur->next;
    }
  return cur;
}

int shadown_lan_exists(char *name, int num, char *lan)
{
  int result = 0;
  t_shadown_lan *cur = g_head_shadown_lan;
  memset(lan, 0, MAX_NAME_LEN);
  while (cur)
    {
    if ((!strcmp(cur->name, name)) &&
        (cur->num == num))
      break;
    cur = cur->next;
    }
  if (cur)
    {
    strncpy(lan, cur->lan, MAX_NAME_LEN-1); 
    result = 1;
    }
  return result;
}
 
void recv_coherency_unlock(void)
{
  glob_coherency -= 1;
  if (glob_coherency < 0)
    KOUT(" ");
} 
  
void recv_coherency_lock(void)
{
  glob_coherency += 1;
}

static t_timer_endp *timer_find(char *name, int num)
{
  t_timer_endp *cur = NULL;
  if (name[0])
    {
    cur = g_head_timer;
    while(cur)
      {
      if ((!strcmp(cur->name, name)) && (cur->num == num))
        break;
      cur = cur->next;
      }
    }
  return cur;
}

static void timer_free(t_timer_endp *timer)
{
  if (timer->prev)
    timer->prev->next = timer->next;
  if (timer->next)
    timer->next->prev = timer->prev;
  if (timer == g_head_timer)
    g_head_timer = timer->next;
  clownix_free(timer, __FUNCTION__);
}

static int recv_coherency_locked(void)
{
  if (glob_coherency > 0)
    glob_coherency_fail_count += 1;
  else 
    glob_coherency_fail_count = 0;
  if  ((glob_coherency_fail_count > 0) &&
       (glob_coherency_fail_count % 300 == 0))
    KERR("ERROR LOCK TOO LONG %d", glob_coherency_fail_count);
  return glob_coherency;
}

static t_coherency_delay *coherency_add_chain(int llid, int tid,
                                              char *name, int num,
                                              char *lan)
{
  t_coherency_delay *cur, *elem;
  elem = (t_coherency_delay *) clownix_malloc(sizeof(t_coherency_delay), 16);
  memset(elem, 0, sizeof(t_coherency_delay));
  elem->llid = llid;
  elem->tid = tid;
  elem->num = num;
  strcpy(elem->name, name);
  strcpy(elem->lan, lan);
  if (g_head_coherency)
    {
    cur = g_head_coherency;
    while (cur && cur->next)
      cur = cur->next;
    cur->next = elem;
    elem->prev = cur;
    }
  else
    g_head_coherency = elem;
  return g_head_coherency;
}

static void coherency_del_chain(t_coherency_delay *cd)
{
  if (cd->prev)
    cd->prev->next = cd->next;
  if (cd->next)
    cd->next->prev = cd->prev;
  if (cd == g_head_coherency)
    g_head_coherency = cd->next;
  clownix_free(cd, __FUNCTION__);
}

#define ENDP_POLL_BEATS 4
#define ENDP_NOT_READY_MAX 2500

#define ENDP_TRACE 0

static void trace_endp_expiry(t_timer_endp *te, char *what)
{
  char use[MAX_PATH_LEN];
  int known;
  unsigned long waited;
  if (!ENDP_TRACE)
    return;
  memset(use, 0, MAX_PATH_LEN);
  known = cfg_name_is_in_use(0, te->name, use);
  waited = te->asked_at_ms ? (cloonix_now_ms() - te->asked_at_ms) : 0;
  KTRC_ON(ENDP_TRACE,
          "%s %d lan %s %s after %lu ms and %d tries. At the request the "
          "endpoint was %s; the config now says %s; a delete arrived for it "
          "while waiting: %s",
          te->name, te->num, te->lan, what, waited, te->count,
          te->existed_when_asked ? "there" : "absent",
          known ? use : "nothing has that name",
          te->cancelled ? "yes" : "no");
  KTRC_ON(ENDP_TRACE, "%s: kvm=%d cnt=%d tap=%d phy=%d nat=%d a2b=%d",
          te->name,
          kvm_exists(te->name, te->num) ? 1 : 0,
          cnt_name_exists(te->name, &known) ? 1 : 0,
          ovs_tap_exists(te->name) ? 1 : 0,
          ovs_phy_exists_vhost(te->name) ? 1 : 0,
          ovs_nat_main_exists(te->name) ? 1 : 0,
          ovs_a2b_exists(te->name) ? 1 : 0);
}

/* How long to keep a lan waiting for an endpoint that is not in the config. */
#define ENDP_WAIT_C2C 250
#define ENDP_WAIT_OTHER 25

static int endp_wait_ceiling(t_timer_endp *te)
{
  if (ovs_c2c_exists(te->name))
    return ENDP_WAIT_C2C;
  return ENDP_WAIT_OTHER;
}

static unsigned long g_wait_appeared;
static unsigned long g_wait_expired;

static void timer_endp(void *data)
{
  t_timer_endp *te = (t_timer_endp *) data;
  int endp_kvm, nb_eth, nb_eth_unused, cnt_exists, cnt_present;
  t_eth_table *eth_tab;
  t_ovs_tap *tap_exists;
  t_ovs_phy *phy_exists;
  t_ovs_nat_main *nat_exists;
  t_ovs_a2b *a2b_exists;
  if (te->cancelled)
    {
    KERR("WARNING %s %d %s: the add was cancelled by a delete, not "
         "attaching", te->name, te->num, te->lan);
    timer_free(te);
    return;
    }
  endp_kvm = kvm_exists(te->name, te->num);
  cnt_exists = cnt_info(te->name, &nb_eth, &eth_tab);
  cnt_present = cnt_name_exists(te->name, &nb_eth_unused);
  tap_exists = ovs_tap_exists(te->name);
  phy_exists = ovs_phy_exists_vhost(te->name);
  nat_exists = ovs_nat_main_exists(te->name);
  a2b_exists = ovs_a2b_exists(te->name);

  if (endp_kvm || cnt_exists || tap_exists ||
      phy_exists || nat_exists || a2b_exists)
    {
    if (te->waited_while_absent)
      g_wait_appeared += 1;
    local_add_lan(te->llid, te->tid, te->name, te->num, te->lan);
    timer_free(te);
    }
  else
    {
    te->count++;
    if (cnt_present || kvm_eth_present(te->name, te->num))
      {
      /* Present but not ready yet: booting cnt, or an eth just declared. */
      if (te->count >= ENDP_NOT_READY_MAX)
        {
        KERR("ERROR %s %d %s: the endpoint exists but never became ready, "
             "the lan is NOT attached", te->name, te->num, te->lan);
        send_status_ko(te->llid, te->tid, "machine never became ready");
        timer_free(te);
        }
      else
        {
        if ((te->count % 500) == 0)
          KERR("WARNING %s %d %s: still waiting for the endpoint to be "
               "ready, try %d", te->name, te->num, te->lan, te->count);
        clownix_timeout_add(ENDP_POLL_BEATS, timer_endp, (void *) te,
                          NULL, NULL);
        }
      }
    else if (te->count >= endp_wait_ceiling(te))
      {
      te->waited_while_absent = 1;
      /* Deleted, or not made yet? */
      if (te->existed_when_asked)
        {
        KERR("ERROR %s %d %s: the endpoint was there when the lan was asked "
             "for and is gone now, so it was deleted underneath the request. "
             "The lan is REFUSED rather than filed in the shadow list",
             te->name, te->num, te->lan);
        trace_endp_expiry(te, "REFUSED");
        send_status_ko(te->llid, te->tid, "endpoint was deleted");
        timer_free(te);
        return;
        }
      /* The shadow list is read back only for a c2c whose far end has not
         connected yet. */
      if (ovs_c2c_exists(te->name))
        {
        g_wait_expired += 1;
        KERR("WARNING %s %d %s: the c2c is not connected yet after %d "
             "tries, the lan goes into the shadow list until it is. Of the "
             "waits like this one, %lu ended with the endpoint appearing "
             "and %lu ran out", te->name, te->num, te->lan, te->count,
             g_wait_appeared, g_wait_expired);
        trace_endp_expiry(te, "SHADOWED");
        send_status_ok(te->llid, te->tid, "ok");
        shadown_lan_alloc(te->name, te->num, te->lan);
        }
      else
        {
        char use[MAX_PATH_LEN];
        int known = cfg_name_is_in_use(0, te->name, use);
        g_wait_expired += 1;
        /* a name the config no longer has was deleted meanwhile: the
           client's own doing, a warning; a known name that never came
           is an error */
        KERR("%s %s %d %s: no endpoint of that name after %d tries and "
             "it is not a c2c, so this cable can never exist. The config "
             "says: %s. REFUSED rather than answered ok. Of the waits like "
             "this one, %lu ended with the endpoint appearing and %lu ran "
             "out", known ? "ERROR" : "WARNING",
             te->name, te->num, te->lan, te->count,
             known ? use : "nothing has that name",
             g_wait_appeared, g_wait_expired);
        trace_endp_expiry(te, "REFUSED, never appeared");
        send_status_ko(te->llid, te->tid, "endpoint never appeared");
        }
      timer_free(te);
      }
    else
      {
      te->waited_while_absent = 1;
      clownix_timeout_add(ENDP_POLL_BEATS, timer_endp, (void *) te,
                          NULL, NULL);
      }
    }
}

static int endpoint_is_there(char *name, int num)
{
  int nb_eth;
  t_eth_table *eth_tab;
  return (kvm_exists(name, num) ||
          cnt_info(name, &nb_eth, &eth_tab) ||
          cnt_name_exists(name, &nb_eth) ||
          (ovs_tap_exists(name) != NULL) ||
          (ovs_phy_exists_vhost(name) != NULL) ||
          (ovs_nat_main_exists(name) != NULL) ||
          (ovs_a2b_exists(name) != NULL));
}

static void timer_endp_init(int llid, int tid, char *name, int num, char *lan)
{       
  t_timer_endp *te = timer_find(name, num);
  if (te) 
    { 
    send_status_ko(llid, tid, "endpoint already waiting");
    KERR("WARNING %s %d %s refused: a plug on that eth is still "
         "waiting for its endpoint", name, num, lan);
    }
  else
    {
    te = (t_timer_endp *)clownix_malloc(sizeof(t_timer_endp), 4); 
    memset(te, 0, sizeof(t_timer_endp));
    strncpy(te->name, name, MAX_NAME_LEN-1);
    strncpy(te->lan, lan, MAX_NAME_LEN-1);
    te->num = num;
    te->llid = llid;
    te->tid = tid;
    te->existed_when_asked = endpoint_is_there(name, num);
    te->asked_at_ms = cloonix_now_ms();
    if (g_head_timer)
      g_head_timer->prev = te;
    te->next = g_head_timer;
    g_head_timer = te;
    clownix_timeout_add(ENDP_POLL_BEATS, timer_endp, (void *) te,
                          NULL, NULL);
    }
} 

static void delayed_coherency_cmd_timeout(void *data)
{
  t_coherency_delay *next, *cur;
  (void) data;
  g_coherency_abs_beat_timer = 0;
  g_coherency_ref_timer = 0;

  if (recv_coherency_locked())
    {
    clownix_timeout_add(20, delayed_coherency_cmd_timeout, NULL,
                        &g_coherency_abs_beat_timer, &g_coherency_ref_timer);
    return;
    }
  cur = g_head_coherency;
  while (cur)
    {
    next = cur->next;
    if (cfg_is_a_zombie(cur->name))
      {
      cur->count += 1;
      if (cur->count >= COHERENCY_WAIT_MAX)
        {
        KERR("ERROR %s %d %s: still a zombie after %d tries, the lan is "
             "NOT attached", cur->name, cur->num, cur->lan, cur->count);
        send_status_ko(cur->llid, cur->tid, "machine is a zombie");
        coherency_del_chain(cur);
        }
      else if ((cur->count % 100) == 0)
        KERR("WARNING %s %d %s: waiting for a zombie to go, try %d",
             cur->name, cur->num, cur->lan, cur->count);
      }
    else
      {
      timer_endp_init(cur->llid, cur->tid, cur->name, cur->num, cur->lan);
      coherency_del_chain(cur);
      }
    cur = next;
    }
  if (g_head_coherency)
    clownix_timeout_add(20, delayed_coherency_cmd_timeout, NULL,
                        &g_coherency_abs_beat_timer, &g_coherency_ref_timer);
}   

static void local_add_lan(int llid, int tid, char *name, int num, char *lan)
{
  t_vm *vm = cfg_get_vm(name);
  t_eth_table *eth_tab = NULL;
  int nb_eth;
  int endp_kvm = kvm_exists(name, num);
  int cnt_exists = cnt_info(name, &nb_eth, &eth_tab);
  t_ovs_tap *tap_exists = ovs_tap_exists(name);
  t_ovs_phy *phy_exists = ovs_phy_exists_vhost(name);
  t_ovs_nat_main *nat_exists = ovs_nat_main_exists(name);
  t_ovs_a2b *a2b_exists = ovs_a2b_exists(name);
  char info[MAX_PATH_LEN];
  if (vm != NULL)
    eth_tab = vm->kvm.eth_table;

  if (get_inhib_new_clients())
    {
    send_status_ko(llid, tid, "AUTODESTRUCT_ON");
    KERR("ERROR %s %d %s: refused, the net is going down", name, num, lan);
    }
  else if (cfg_name_is_in_use(1, lan, info))
    {
    send_status_ko(llid, tid, info);
    KERR("ERROR %s %d %s: %s", name, num, lan, info);
    }
  else if (cnt_exists)
    {
    if (nb_eth <= num)
      {
      send_status_ko(llid, tid, "eth does not exist");
      KERR("ERROR %s %d %s: eth %d does not exist", name, num, lan, num);
      }
    else
      {
#if LAN_TRACE
      g_lan_done += 1;
      LLOG("%d ATTACHING %s %d %s", g_lan_done, name, num, lan);
#endif
      if (cnt_add_lan(llid, tid, name, num, lan, info))
        {
        send_status_ko(llid, tid, info);
        }
      }
    }
  else if ((endp_kvm == endp_type_eths) ||
           (endp_kvm == endp_type_ethv))
    {
    if ((vm == NULL) ||
        ((eth_tab[num].endp_type != endp_type_eths) && 
         (eth_tab[num].endp_type != endp_type_ethv)) ||
        (num >= vm->kvm.nb_tot_eth))
      {
      send_status_ko(llid, tid, "eth does not exist or is not a lan eth");
      KERR("ERROR %s %d %s: eth %d of the kvm is absent or not a lan eth",
           name, num, lan, num);
      }
    else
      {
      if (kvm_add_lan(llid, tid, name, num, lan, endp_kvm, eth_tab, info))
        {
        send_status_ko(llid, tid, info);
        KERR("ERROR %s", info);
        }
      }
    }
  else if ((tap_exists) && (num == 0))
    ovs_tap_add_lan(llid, tid, name, lan);
  else if ((phy_exists) && (num == 0))
    ovs_phy_add_lan(llid, tid, name, lan);
  else if ((nat_exists) && (num == 0))
    ovs_nat_main_add_lan(llid, tid, name, lan);
  else if ((a2b_exists) && ((num == 0) || (num == 1)))
    ovs_a2b_add_lan(llid, tid, name, num, lan);
  else
    {
    KERR("ERROR %s %d %s: no endpoint of that name and eth", name, num, lan);
    send_status_ko(llid, tid, "no endpoint of that name and eth");
    }
}

void recv_add_lan_endp(int llid, int tid, char *name, int wire_num,
                       char *lan)
{
  char info[MAX_PATH_LEN];
  int idx, num = wifi_air_num(name, wire_num);
  t_shadown_lan *cur = shadown_lan_find(name, num, lan);
  t_ovs_c2c *c2c_exists = ovs_c2c_exists(name);

#if LAN_TRACE
  g_lan_seen += 1;
  LLOG("%d ARRIVED %s %d %s", g_lan_seen, name, num, lan);
#endif

  if (!get_uml_cloonix_started())
    {
    send_status_ko(llid, tid, "SERVER NOT READY");
    KERR("ERROR SERVER NOT READY");
    return;
    }

  if (get_inhib_new_clients())
    {
    send_status_ko(llid, tid, "AUTODESTRUCT_ON");
    KERR("ERROR AUTODESTRUCT_ON");
    }
  else if (num < 0)
    {
    snprintf(info, MAX_PATH_LEN-1, "plug %s%s %s refused: %s has no%s",
             name, wifi_air_iface(name, wire_num), lan, name,
             wifi_air_iface(name, wire_num));
    KERR("ERROR %s", info);
    send_status_ko(llid, tid, info);
    }
  else if (wifi_air_wlan(name, num, &idx))
    wifi_air_plug(llid, tid, name, num, lan, 1);
  else if (wifi_air_in_use(lan))
    {
    snprintf(info, MAX_PATH_LEN-1, "plug %s%s %s refused: %s is an air "
             "of wlans, an eth goes on a lan", name,
             wifi_air_iface(name, num), lan, lan);
    KERR("ERROR %s", info);
    send_status_ko(llid, tid, info);
    }
  else if (cfg_name_is_in_use(1, lan, info))
    {
    send_status_ko(llid, tid, info);
    KERR("ERROR %s", info);
    }
  else if ((!strlen(name)) || (!strlen(lan)))
    {
    snprintf(info, MAX_PATH_LEN-1, "plug refused: the %s is empty",
             strlen(name) ? "lan" : "endpoint name");
    KERR("ERROR %s", info);
    send_status_ko(llid, tid, info);
    }
  else if (cur)
    {
    strcpy(info, "Already exists in shadow_lan");
    send_status_ko(llid, tid, info);
    KERR("ERROR %s", info);
    }
  else if (c2c_exists && shadown_lan_exists(name, num, info))
    {
    char txt[MAX_PATH_LEN];
    snprintf(txt, MAX_PATH_LEN-1, "c2c %s is already cabled to lan %s, "
             "unplug it first", name, info);
    send_status_ko(llid, tid, txt);
    KERR("ERROR %s", txt);
    }
  else 
    {
    if (c2c_exists)
      {
      LLOG("%s %d %s -> c2c shadow", name, num, lan);
      send_status_ok(llid, tid, "ok");
      shadown_lan_alloc(name, num, lan);
      }
    else
      {
      if ((recv_coherency_locked()) || cfg_is_a_zombie(name))
        {
        LLOG("%s %d %s -> delayed (locked=%d zombie=%d)", name, num, lan,
             recv_coherency_locked(), (cfg_is_a_zombie(name) != NULL));
        g_head_coherency = coherency_add_chain(llid, tid, name, num, lan);
        if (!g_coherency_abs_beat_timer)
          {
          clownix_timeout_add(20, delayed_coherency_cmd_timeout, NULL,
                              &g_coherency_abs_beat_timer,
                              &g_coherency_ref_timer);
          }
        }  
      else
        {
        LLOG("%s %d %s -> timer_endp_init", name, num, lan);
        timer_endp_init(llid, tid, name, num, lan);
        }
      }
    }
}

/* A delete arriving while the container is busy is retried, not refused. */
typedef struct t_del_retry
{
  int llid;
  int tid;
  char name[MAX_NAME_LEN];
  int num;
  char lan[MAX_NAME_LEN];
  int count;
} t_del_retry;

static void timer_del_retry(void *data)
{
  t_del_retry *dr = (t_del_retry *) data;
  int nb_eth;
  t_eth_table *eth_tab = NULL;
  if (cnt_info(dr->name, &nb_eth, &eth_tab))
    {
    KERR("WARNING %s %d %s: the eth was busy, the delete is going through "
         "now after %d tries", dr->name, dr->num, dr->lan, dr->count);
    recv_del_lan_endp(dr->llid, dr->tid, dr->name, dr->num, dr->lan);
    clownix_free(dr, __FUNCTION__);
    }
  else if (dr->count >= 20)
    {
    KERR("ERROR %s %d %s: the eth stayed busy for %d tries, refusing the "
         "delete", dr->name, dr->num, dr->lan, dr->count);
    if (dr->llid)
      send_status_ko(dr->llid, dr->tid, "eth operation ongoing");
    clownix_free(dr, __FUNCTION__);
    }
  else
    {
    dr->count += 1;
    clownix_timeout_add(100, timer_del_retry, (void *) dr, NULL, NULL);
    }
}

static void del_retry_init(int llid, int tid, char *name, int num, char *lan)
{
  t_del_retry *dr = (t_del_retry *) clownix_malloc(sizeof(t_del_retry), 4);
  memset(dr, 0, sizeof(t_del_retry));
  dr->llid = llid;
  dr->tid = tid;
  strncpy(dr->name, name, MAX_NAME_LEN-1);
  dr->num = num;
  strncpy(dr->lan, lan, MAX_NAME_LEN-1);
  dr->count = 1;
  clownix_timeout_add(100, timer_del_retry, (void *) dr, NULL, NULL);
}

void recv_del_lan_endp(int llid, int tid, char *name, int wire_num,
                       char *lan)
{
  char info[MAX_PATH_LEN];
  int num = wifi_air_num(name, wire_num);
  int endp_kvm = kvm_exists(name, num);
  t_eth_table *eth_tab = NULL;
  int nb_eth;
  int cnt_exists = cnt_info(name, &nb_eth, &eth_tab);
  t_ovs_tap *tap_exists = ovs_tap_exists(name);
  t_ovs_phy *phy_exists = ovs_phy_exists_vhost(name);
  t_ovs_nat_main *nat_exists = ovs_nat_main_exists(name);
  t_ovs_a2b *a2b_exists = ovs_a2b_exists(name);
  t_shadown_lan *cur = shadown_lan_find(name, num, lan);
  int idx;

  if (!get_uml_cloonix_started())
    {
    send_status_ko(llid, tid, "SERVER NOT READY");
    KERR("ERROR SERVER NOT READY");
    return;
    }
  if (num < 0)
    {
    snprintf(info, sizeof(info), "unplug %s%s %s refused: %s has no%s",
             name, wifi_air_iface(name, wire_num), lan, name,
             wifi_air_iface(name, wire_num));
    KERR("ERROR %s", info);
    send_status_ko(llid, tid, info);
    return;
    }
  if (wifi_air_wlan(name, num, &idx))
    {
    wifi_air_plug(llid, tid, name, num, lan, 0);
    return;
    }

  if (cur)
    shadown_lan_free(cur);
  if (cnt_name_exists(name, &nb_eth))
    {
    if (!cnt_exists)
      {
      t_timer_endp *pending = timer_find(name, num);
      if (pending)
        {
        pending->cancelled = 1;
        KERR("WARNING %s %d %s: delete arrived while the add was still "
             "waiting, cancelling the add", name, num, lan);
        send_status_ok(llid, tid, "OK");
        }
      else
        {
        del_retry_init(llid, tid, name, num, lan);
        }
      }
    else if (nb_eth <= num)
      {
      send_status_ko(llid, tid, "eth does not exist");
      KERR("ERROR %s %d %s: eth %d does not exist", name, num, lan, num);
      }
    else
      {
      if (cnt_del_lan(llid, tid, name, num, lan, info))
        {
        send_status_ko(llid, tid, info);
        }
      }
    }
  else if ((endp_kvm == endp_type_eths) ||
           (endp_kvm == endp_type_ethv))
    {
    if (kvm_del_lan(llid, tid, name, num, lan, endp_kvm))
      {
      KERR("ERROR unplug %s from guest %s eth %d refused", lan, name, num);
      send_status_ko(llid, tid, "failure");
      }
    }
  else if ((tap_exists) && (num == 0))
    ovs_tap_del_lan(llid, tid, name, lan);
  else if ((phy_exists) && (num == 0))
    ovs_phy_del_lan(llid, tid, name, lan);
  else if ((nat_exists) && (num == 0))
    ovs_nat_main_del_lan(llid, tid, name, lan);
  else if ((a2b_exists) && ((num == 0) || (num == 1)))
    ovs_a2b_del_lan(llid, tid, name, num, lan);
  else
    {
    if (cur)
      send_status_ok(llid, tid, "ok");
    else
      {
      /* Neither an endpoint nor a lan of that name: unknown to the config. */
      KERR("ERROR unplug %s: %s %d is not an endpoint of any kind and no "
           "lan of that name is known", lan, name, num);
      snprintf(info, sizeof(info), "unplug %s%s %s failed", name,
               wifi_air_iface(name, num), lan);
      send_status_ko(llid, tid, info);
      }
    }
}

