/*****************************************************************************/
/*    Copyright (C) 2006-2018 cloonix@cloonix.net License AGPL-3             */
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
#include "io_clownix.h"

/*****************************************************************************/
#define LONG_CPU_STORE    30
#define MEDIUM_CPU_STORE 10
#define SHORT_CPU_STORE   3
#define MAX_TIMEOUT_BEATS 0x3FFFF
#define MIN_TIMEOUT_BEATS 1
/*---------------------------------------------------------------------------*/
typedef struct t_timeout_elem
{
  int inhibited;
  t_fct_timeout cb;
  void *data;
  int ref;
  struct t_timeout_elem *prev;
  struct t_timeout_elem *next;
} t_timeout_elem; 
/*---------------------------------------------------------------------------*/

static t_timeout_elem *grape[MAX_TIMEOUT_BEATS]; 
static int current_ref;
static long long current_beat;
static int plugged_elem;

static int glob_cpu_idle[LONG_CPU_STORE];
static int glob_cpu_sum[LONG_CPU_STORE];
static int glob_current_cpu_idx = 0;
static int glob_sum_bogomips;

void doorways_sock_refresh_second_beat(void);

/*****************************************************************************/
/*                      clownix_get_sum_bogomips                             */
/*---------------------------------------------------------------------------*/
int clownix_get_sum_bogomips(void)
{
  return (glob_sum_bogomips);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/*                      get_sum_bogomips                             */
/*---------------------------------------------------------------------------*/
static int get_sum_bogomips(void)
{
  char line[1024];
  int val, result = -1;
  FILE *fp;
  char *ptr, *eptr;
  glob_sum_bogomips = 0;
  if ((fp = fopen("/proc/cpuinfo", "r")) != NULL)
    {
    while (fgets(line, 1024, fp) != NULL)
      {
      if (!strncmp(line, "bogomips", strlen("bogomips")))
        {
        ptr = strchr(line,':');
        if (ptr)
          {
          ptr++;
          eptr = strchr(line,'.');
          if (eptr)
            *eptr = 0;
          if (sscanf(ptr, "%d", &val) == 1)
            {
            glob_sum_bogomips += val;
            result = 0;
            }
          }
        }
      }
    fclose(fp);
    }
  return result;
}

/*****************************************************************************/
/*                      read_cpu_proc_stat                                   */
/*---------------------------------------------------------------------------*/
static int read_cpu_proc_stat(unsigned long long *cpu_user,
                              unsigned long long *cpu_nice,
                              unsigned long long *cpu_sys,
                              unsigned long long *cpu_idle,
                              unsigned long long *cpu_iowait,
                              unsigned long long *cpu_hardirq,
                              unsigned long long *cpu_softirq,
                              unsigned long long *cpu_steal,
                              unsigned long long *cpu_guest)
{
  int result = -1;
  FILE *fp;
  char line[1024];
  if ((fp = fopen("/proc/stat", "r")) != NULL)
    {
    while (fgets(line, 1024, fp) != NULL)
      {
      if (!strncmp(line, "cpu ", 4))
        {
        if (sscanf(line + 5, "%llu %llu %llu %llu %llu %llu %llu %llu %llu",
                             cpu_user, cpu_nice, cpu_sys, cpu_idle, cpu_iowait,
                             cpu_hardirq,cpu_softirq,cpu_steal,cpu_guest) == 9)
          {
          result = 0;
          }
        break;
        }
      }
    fclose(fp);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/*                      refresh_cpu_idle                                     */
/*---------------------------------------------------------------------------*/
static void refresh_cpu_idle(void)
{
  unsigned long long llsum;
  static unsigned long long prev_cpu_user;
  static unsigned long long prev_cpu_nice;
  static unsigned long long prev_cpu_sys;
  static unsigned long long prev_cpu_idle;
  static unsigned long long prev_cpu_iowait;
  static unsigned long long prev_cpu_hardirq;
  static unsigned long long prev_cpu_softirq;
  static unsigned long long prev_cpu_steal;
  static unsigned long long prev_cpu_guest;
  unsigned long long cpu_user, delta_cpu_user;
  unsigned long long cpu_nice, delta_cpu_nice;
  unsigned long long cpu_sys, delta_cpu_sys;
  unsigned long long cpu_idle, delta_cpu_idle;
  unsigned long long cpu_iowait, delta_cpu_iowait;
  unsigned long long cpu_hardirq, delta_cpu_hardirq;
  unsigned long long cpu_softirq, delta_cpu_softirq;
  unsigned long long cpu_steal, delta_cpu_steal;
  unsigned long long cpu_guest, delta_cpu_guest;
  if (!read_cpu_proc_stat(&cpu_user, &cpu_nice, &cpu_sys,
                          &cpu_idle, &cpu_iowait, &cpu_hardirq,
                         &cpu_softirq, &cpu_steal, &cpu_guest))
    {
    delta_cpu_user    = cpu_user    - prev_cpu_user;
    delta_cpu_nice    = cpu_nice    - prev_cpu_nice;
    delta_cpu_sys     = cpu_sys     - prev_cpu_sys;
    delta_cpu_idle    = cpu_idle    - prev_cpu_idle;
    delta_cpu_iowait  = cpu_iowait  - prev_cpu_iowait;
    delta_cpu_hardirq = cpu_hardirq - prev_cpu_hardirq;
    delta_cpu_softirq = cpu_softirq - prev_cpu_softirq;
    delta_cpu_steal   = cpu_steal   - prev_cpu_steal;
    delta_cpu_guest   = cpu_guest   - prev_cpu_guest;
    prev_cpu_user    = cpu_user;
    prev_cpu_nice    = cpu_nice;
    prev_cpu_sys     = cpu_sys;
    prev_cpu_idle    = cpu_idle;
    prev_cpu_iowait  = cpu_iowait;
    prev_cpu_hardirq = cpu_hardirq;
    prev_cpu_softirq = cpu_softirq;
    prev_cpu_steal   = cpu_steal;
    prev_cpu_guest   = cpu_guest;
    llsum = delta_cpu_user + delta_cpu_nice + delta_cpu_sys + delta_cpu_idle +
            delta_cpu_iowait + delta_cpu_hardirq + delta_cpu_softirq +
            delta_cpu_steal + delta_cpu_guest;
    glob_cpu_idle[glob_current_cpu_idx] = (int) delta_cpu_idle;
    glob_cpu_sum[glob_current_cpu_idx] = (int) llsum;
    glob_current_cpu_idx += 1;
    if (glob_current_cpu_idx == LONG_CPU_STORE)
      glob_current_cpu_idx = 0;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/*                      get_current_long_cpu                                 */
/*---------------------------------------------------------------------------*/
static int get_current_long_cpu(void)
{
  int result, i, idle = 0, sum = 0;
  for (i=0; i < LONG_CPU_STORE; i++)
    {
    idle += glob_cpu_idle[i];
    sum  += glob_cpu_sum[i];
    }
  if (sum)
    {
    idle *= 1000;
    result = idle/sum;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/*                      get_current_medium_cpu                               */
/*---------------------------------------------------------------------------*/
static int get_current_medium_cpu(void)
{
  int idx, result, i, idle = 0, sum = 0;
  idx = glob_current_cpu_idx - MEDIUM_CPU_STORE;
  if (idx < 0)
    idx += LONG_CPU_STORE;
  for (i=0; i < MEDIUM_CPU_STORE; i++)
    {
    idle += glob_cpu_idle[idx];
    sum  += glob_cpu_sum[idx];
    idx++;
    if (idx == LONG_CPU_STORE)
      idx = 0;
    }
  if (sum)
    {
    idle *= 1000;
    result = idle/sum;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/*                      get_current_short_cpu                                */
/*---------------------------------------------------------------------------*/
static int get_current_short_cpu(void)
{
  int idx, result, i, idle = 0, sum = 0;
  idx = glob_current_cpu_idx - SHORT_CPU_STORE;
  if (idx < 0)
    idx += LONG_CPU_STORE;
  for (i=0; i < SHORT_CPU_STORE; i++)
    {
    idle += glob_cpu_idle[idx];
    sum  += glob_cpu_sum[idx];
    idx++;
    if (idx == LONG_CPU_STORE)
      idx = 0;
    }
  if (sum)
    {
    idle *= 1000;
    result = idle/sum;
    }
  return result;
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
/*                      clownix_get_current_cpu                              */
/*---------------------------------------------------------------------------*/
void clownix_get_current_cpu(int *long_cpu, int *medium_cpu, int *short_cpu)
{
  *long_cpu   = get_current_long_cpu();
  *medium_cpu = get_current_medium_cpu();
  *short_cpu  = get_current_short_cpu();
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int clownix_get_nb_timeouts(void)
{
  return plugged_elem;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int plug_elem(t_timeout_elem **head, t_fct_timeout cb, void *data)
{
  int result = 0;
  t_timeout_elem *cur = *head;
  t_timeout_elem *last = *head;
  current_ref++;
  if (current_ref == 0xFFFFFFFF)
    current_ref = 1;
  result = current_ref;
  while(cur)
    {
    last = cur;
    cur = cur->next;
    }
  cur = (t_timeout_elem *) clownix_malloc(sizeof(t_timeout_elem), 1);
  memset(cur, 0, sizeof(t_timeout_elem));
  cur->prev = last;
  if (last) 
    last->next = cur;
  else
    *head = cur;
  plugged_elem++;
  cur->cb = cb;
  cur->data = data;
  cur->ref = result;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int unplug_elem(t_timeout_elem **head, t_timeout_elem *elem)
{
  int result = 0;
  if (elem->prev)
    elem->prev->next = elem->next;
  if (elem->next)
    elem->next->prev = elem->prev;
  if (*head == elem)
    *head = elem->next;
  plugged_elem--;
  if (plugged_elem < 0)
    KOUT(" ");
  clownix_free(elem, __FUNCTION__);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void process_all_current_grape_idx(t_timeout_elem **head)
{
  t_timeout_elem *cur = *head;
  t_timeout_elem *next;
  while(cur)
    {
    next = cur->next;
    if (!cur->inhibited)
      cur->cb(cur->data);
    unplug_elem(head, cur);
    cur = next;
    }
}
/*---------------------------------------------------------------------------*/

 
/*****************************************************************************/
void clownix_timeout_add(int nb_beats, t_fct_timeout cb, void *data,
                         long long *abs_beat, int *ref)
{
  long long abs;
  int ref1, ref2;
  if ((nb_beats < MIN_TIMEOUT_BEATS) || 
      (nb_beats >= MAX_TIMEOUT_BEATS))
    KOUT("%d", nb_beats);
  abs = current_beat + nb_beats;
  ref1 = (int) (abs % MAX_TIMEOUT_BEATS);
  ref2 = plug_elem(&(grape[ref1]), cb, data);
  if (ref)
    *ref = ref2;
  if (abs_beat)
    *abs_beat = abs;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int clownix_timeout_exists(long long abs_beat, int ref)
{
  int result = 0;
  int ref1, ref2;
  t_timeout_elem *cur;
  ref1 = (int) (abs_beat % MAX_TIMEOUT_BEATS);
  ref2 = ref;
  cur = grape[ref1];
  while(cur)
    {
    if (cur->ref == ref2)
      break;
    cur = cur->next;
    }
  if (cur)
    result = 1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void *clownix_timeout_del(long long abs_beat, int ref, 
                          const char *file, int line)
{
  int result = -1;
  void *data = NULL;
  int ref1, ref2;
  t_timeout_elem *cur;
  ref1 = (int) (abs_beat % MAX_TIMEOUT_BEATS);
  ref2 = ref;
  cur = grape[ref1];
  while(cur)
    {
    if (cur->ref == ref2)
      {
      data = cur->data;
      break;
      }
    cur = cur->next;
    }
  if (cur)
    {
    cur->inhibited = 1;
    result = 0;
    }
  if (abs_beat && result) 
    KOUT("%s line:%d    %lld %lld %d %d\n", file, line, 
         abs_beat, current_beat, ref1, ref2);
  return data;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void clownix_timeout_del_all(void)
{
  int i;
  t_timeout_elem *cur;
  for (i=0; i<MAX_TIMEOUT_BEATS; i++)
    {
    cur = grape[i];
    while(cur)
      {
      cur->inhibited = 1;
      cur = cur->next;
      }
    }
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void clownix_timer_beat(void)
{
  static int count = 0;
  int idx;
  count++;
  if (count == 10)
    {
    refresh_cpu_idle();
    count = 0;
    }
  current_beat++;
  idx = (int) (current_beat % MAX_TIMEOUT_BEATS);
  process_all_current_grape_idx(&(grape[idx]));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void clownix_timer_init(void)
{
  refresh_cpu_idle();
  get_sum_bogomips();
  memset(grape, 0, MAX_TIMEOUT_BEATS * sizeof(t_timeout_elem *));
  current_ref = 0;
  current_beat = 0;
  plugged_elem = 0;
}
/*---------------------------------------------------------------------------*/




