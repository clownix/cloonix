/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <dirent.h>

#include "io_clownix.h"
#include "pcap_relay.h"
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "wifi_air.h"
#include "heartbeat.h"
#include "machine_create.h"
#include "event_subscriber.h"
#include "pid_clone.h"
#include "automates.h"
#include "llid_trace.h"

static int get_my_open_fds_qty(void)
{
  int result=0;
  char dir[MAX_PATH_LEN];
  DIR *dirptr;
  struct dirent *ent;
  snprintf(dir, sizeof(dir), "/proc/%d/fd", getpid());
  dirptr = opendir(dir);
  if (dirptr)
    {
    while ((ent = readdir(dirptr)) != NULL)
      {
      if(ent->d_type != DT_DIR)
        {
        result++;
        }
      }
    if (closedir(dirptr))
      KOUT(" ");
    }
  return result;
}

static void loss_of_fd_trap(void)
{
  int open_fds, declared_fds;
  open_fds = get_my_open_fds_qty();
  declared_fds =  (int) cfg_get_trace_fd_qty(0);
  if (open_fds - declared_fds > 8)
    KERR("WARNING open fds:%d declared fds:%d diff:%d",
         open_fds, declared_fds, open_fds - declared_fds);
}

/* Malloc counters: reported on a new high, at most every ten minutes. */
/* Type 23 is a pool that oscillates and does not grow. */
static void malloc_watchdog(void)
{
  static unsigned long hi_total;
  static unsigned long first[MAX_MALLOC_TYPES];
  static int have_first;
  static int quiet_beats;
  unsigned long *nb = get_clownix_malloc_nb();
  char txt[MAX_PATH_LEN];
  char tmr[1200];
  int i, len = 0, worst = -1;
  unsigned long worst_growth = 0;

  memset(txt, 0, MAX_PATH_LEN);
  memset(tmr, 0, sizeof(tmr));
  if (!have_first)
    {
    for (i = 0; i < MAX_MALLOC_TYPES; i++)
      first[i] = nb[i];
    have_first = 1;
    hi_total = nb[0];
    return;
    }
  quiet_beats += 1;
  if ((nb[0] < hi_total + (hi_total / 2)) && (nb[0] < hi_total + 500))
    {
    if (quiet_beats < 60)
      return;
    }
  quiet_beats = 0;
  {
  int is_new_high = (nb[0] > hi_total);
  if (nb[0] > hi_total)
    hi_total = nb[0];
  for (i = 1; i < MAX_MALLOC_TYPES; i++)
    {
    if (nb[i] > first[i] + worst_growth)
      {
      worst_growth = nb[i] - first[i];
      worst = i;
      }
    }
  for (i = 1; i < MAX_MALLOC_TYPES; i++)
    {
    if ((nb[i] > 100) && (len < MAX_PATH_LEN - 40))
      len += sprintf(txt+len, "%d:%lu(%+ld) ", i, nb[i],
                     (long)nb[i] - (long)first[i]);
    }
  if (!is_new_high)
    {
    KTRC("live allocations %lu, steady. Per ident: %s", nb[0], txt);
    return;
    }
  if (worst >= 0)
    KTRC("live allocations %lu reached a NEW HIGH, biggest riser is "
         "ident %d, up %lu since the first sample. Per ident: %s",
         nb[0], worst, worst_growth, txt);
  else
    KTRC("live allocations %lu reached a NEW HIGH. Per ident: %s",
         nb[0], txt);
  if ((worst == 1) || (nb[1] > 500))
    {
    clownix_timer_report_top(tmr, (int)sizeof(tmr));
    KTRC("%s", tmr);
    msg_layer_report_queues(tmr, (int)sizeof(tmr));
    KTRC("%s", tmr);
    }
  }
}

static void heartbeat (int delta)
{
  static int count = 0;
  static int seconde_count = 0;
  static int ten_seconde_count = 0;
  count++;
  if (!(count%HALF_SEC_BEAT))
    {
    count = 0;
    seconde_count++;
    ten_seconde_count++;
    if (seconde_count == 2)
      {
      seconde_count = 0;
      llid_count_beat();
      pcap_relay_beat();
      wifi_air_beat();
      }
    if (ten_seconde_count == 20)
      {
      ten_seconde_count = 0;
      loss_of_fd_trap();
      malloc_watchdog();
      }
    automates_safety_heartbeat();
    }
}

void init_heartbeat(void)
{
  msg_mngt_heartbeat_init(heartbeat);
}
