/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* Peer gone vs process starved: off-cpu time beyond the expected sleep is
   credited to the peer deadline (capped), classed slow/starved/blocked. */
#ifndef STALL_DETECT_H
#define STALL_DETECT_H

#include <stdio.h>
#include <time.h>
#include <sys/resource.h>

#define STALL_FACTOR   3
#define STALL_EXTRA_MS 1000

typedef struct t_stall
{
  long long last_ms;
  long long credit_ms;
  long long credit_cap_ms;
  int period_ms;
  int nb_stalls;
  long long cpu_ms;
  long nvcsw;
  long nivcsw;
  long majflt;
  long long stat_ms;
  unsigned long long stat[8];
  char why[260];
} t_stall;

static inline long long stall_now_ms(void)
{
  struct timespec ts;
  /* MONOTONIC: a wall clock step (ntp, vm resume) is no stall. */
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (((long long) ts.tv_sec) * 1000) + (ts.tv_nsec / 1000000);
}

/* /proc/stat cpu during the gap: steal = hypervisor, softirq = network,
   idle+iowait = machine idle, process waiting. */
static inline void stall_machine(t_stall *st, int gap, char *out, int max)
{
  unsigned long long v[8], tot = 0;
  long long now = stall_now_ms();
  FILE *fh;
  int i, nb = 0;
  out[0] = 0;
  if ((!gap) && (st->stat_ms) && (now - st->stat_ms < 1000))
    return;
  fh = fopen("/proc/stat", "r");
  if (!fh)
    return;
  nb = fscanf(fh, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
              &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7]);
  fclose(fh);
  if (nb != 8)
    return;
  for (i = 0; i < 8; i++)
    tot += v[i] - st->stat[i];
  if ((gap) && (st->stat_ms) && (tot))
    snprintf(out, max, "; the machine over the last %lld ms: user %llu%% "
             "system %llu%% softirq %llu%% irq %llu%% steal %llu%% "
             "iowait %llu%% idle %llu%%", now - st->stat_ms,
             ((v[0]-st->stat[0]) + (v[1]-st->stat[1])) * 100 / tot,
             (v[2]-st->stat[2]) * 100 / tot, (v[6]-st->stat[6]) * 100 / tot,
             (v[5]-st->stat[5]) * 100 / tot, (v[7]-st->stat[7]) * 100 / tot,
             (v[4]-st->stat[4]) * 100 / tot, (v[3]-st->stat[3]) * 100 / tot);
  for (i = 0; i < 8; i++)
    st->stat[i] = v[i];
  st->stat_ms = now;
}

static inline void stall_usage(t_stall *st, int gap)
{
  struct timespec ts;
  struct rusage ru;
  long long cpu_ms;
  char machine[180];
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
  cpu_ms = (((long long) ts.tv_sec) * 1000) + (ts.tv_nsec / 1000000);
  getrusage(RUSAGE_SELF, &ru);
  stall_machine(st, gap, machine, sizeof(machine));
  if (gap)
    snprintf(st->why, sizeof(st->why), "cpu %lld ms, %ld voluntary and %ld "
             "forced switches, %ld major faults%s", cpu_ms - st->cpu_ms,
             ru.ru_nvcsw - st->nvcsw, ru.ru_nivcsw - st->nivcsw,
             ru.ru_majflt - st->majflt, machine);
  st->cpu_ms = cpu_ms;
  st->nvcsw = ru.ru_nvcsw;
  st->nivcsw = ru.ru_nivcsw;
  st->majflt = ru.ru_majflt;
}

static inline char *stall_why(t_stall *st)
{
  return st->why;
}

static inline void stall_init(t_stall *st, int period_ms, int limit_ms)
{
  st->why[0] = 0;
  st->stat_ms = 0;
  stall_usage(st, 0);
  st->last_ms = stall_now_ms();
  st->credit_ms = 0;
  st->credit_cap_ms = ((long long) limit_ms) * 3;
  st->period_ms = period_ms;
  st->nb_stalls = 0;
}

static inline int stall_beat(t_stall *st)
{
  long long now = stall_now_ms();
  long long delta, late;
  int gap = 0;
  if (st->last_ms == 0)
    {
    st->last_ms = now;
    return 0;
    }
  delta = now - st->last_ms;
  st->last_ms = now;
  late = (((long long) st->period_ms) * STALL_FACTOR) + STALL_EXTRA_MS;
  if (delta > late)
    {
    gap = (int) (delta - st->period_ms);
    st->nb_stalls += 1;
    st->credit_ms += gap;
    if (st->credit_ms > st->credit_cap_ms)
      st->credit_ms = st->credit_cap_ms;
    }
  stall_usage(st, gap);
  return gap;
}

static inline void stall_answered(t_stall *st)
{
  st->last_ms = stall_now_ms();
  st->credit_ms = 0;
  st->nb_stalls = 0;
}

static inline int stall_credit_ms(t_stall *st)
{
  return (int) st->credit_ms;
}

static inline int stall_credit_exhausted(t_stall *st)
{
  return (st->credit_ms >= st->credit_cap_ms);
}

#endif
