/*****************************************************************************/
/*    Copyright (C) 2006-2020 clownix@clownix.net License AGPL-3             */
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <time.h>
#include "ioc.h"

#define MAX_REAL_TIMER 4


enum
  {
  insert_at_head_for_empty_timer = 1,
  insert_at_head_for_non_empty_timer,
  rearm_after_timeout,
  };



/*****************************************************************************/
typedef struct t_chain_timer
{
  int inhibited;
  long long async_rx;
  t_fct_real_timer cb;
  void *data;
  long long date_us;
  struct t_chain_timer *prev;
  struct t_chain_timer *next;
} t_chain_timer;
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
typedef struct t_real_timer_item
{
  int id_check;
  int is_armed;
  int is_locked;
  timer_t initialized_timer;
  t_chain_timer *head_chain_timer;
} t_real_timer_item;
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
typedef struct t_real_timer
{
  int real_time_event_fd;
  t_real_timer_item tstore[MAX_REAL_TIMER];
  int fifo_free_index[MAX_REAL_TIMER];
  int read_idx;
  int write_idx;
} t_real_timer;
/*---------------------------------------------------------------------------*/


static t_real_timer *g_real_timer[2];

/*****************************************************************************/
static long long get_target_date_us(void)
{
  struct timespec ts;
  long long sec;
  long long nsec;
  long long result;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  sec = (long long) ts.tv_sec;
  sec *= 1000000;
  nsec = (long long) ts.tv_nsec;
  nsec /= 1000;
  result = sec + nsec;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static long long clownix_gettimeofday_ns(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ((ts.tv_sec * 1000000000LL + ts.tv_nsec));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void fifo_real_timer_init(t_real_timer *rt)
{
  int i;
  for(i = 0; i < MAX_REAL_TIMER; i++)
    rt->fifo_free_index[i] = i+1;
  rt->read_idx = 0;
  rt->write_idx =  MAX_REAL_TIMER - 1;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int fifo_real_timer_alloc(t_real_timer *rt)
{
  int tm_id_check = 0;
  if(rt->read_idx != rt->write_idx)
    {
    tm_id_check = rt->fifo_free_index[rt->read_idx];
    rt->read_idx = (rt->read_idx + 1) % MAX_REAL_TIMER;
    }
  return tm_id_check;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void fifo_real_timer_release(t_real_timer *rt, int tm_id_check)
{
  rt->fifo_free_index[rt->write_idx] =  tm_id_check;
  rt->write_idx=(rt->write_idx + 1) % MAX_REAL_TIMER;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int distance_real_timer(t_real_timer_item *rt)
{
  struct itimerspec timeout;
  long long nb_us;
  int result;
  memset(&timeout, 0, sizeof(struct itimerspec));
  if (timer_gettime(rt->initialized_timer, &timeout))
    KOUT(" ");
  nb_us = timeout.it_value.tv_sec * 1000000;
  nb_us += timeout.it_value.tv_nsec/1000;
  result = (int) nb_us;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void arm_real_timer(t_real_timer_item *rt, int type)
{
  struct itimerspec timeout;
  struct itimerspec current_timeout;
  long long current_us, date_us, delta_us;
  int delta_sec, delta_nsec, reload = 0;
  t_chain_timer *chtim;
  if (!rt)
    KOUT(" ");
  chtim = rt->head_chain_timer;
  if (!chtim)
    KOUT(" ");
  current_us = get_target_date_us();
  date_us = chtim->date_us;
  delta_us = 1;
  if (current_us < date_us)
    delta_us = date_us - current_us;
  delta_sec = delta_us / 1000000;
  delta_nsec = (delta_us % 1000000) * 1000;
  memset(&timeout, 0, sizeof(struct itimerspec));
  memset(&current_timeout, 0, sizeof(struct itimerspec));
  timeout.it_interval.tv_sec = 0;
  timeout.it_interval.tv_nsec = 0;
  timeout.it_value.tv_sec =  delta_sec;
  timeout.it_value.tv_nsec = delta_nsec;
  if (timer_gettime(rt->initialized_timer, &current_timeout))
    KOUT("%d ", errno);
  if (type == insert_at_head_for_empty_timer) 
    {
    if (rt->is_armed)
      KOUT(" ");
    rt->is_armed = 1;
    if (!((current_timeout.it_value.tv_sec == 0) && 
          (current_timeout.it_value.tv_nsec == 0)))
      KOUT(" ");
    reload = 1;
    }
  else if (type == rearm_after_timeout)
    {
    if (rt->is_armed)
      KOUT(" ");
    rt->is_armed = 1;
    reload = 1;
    }
  else if (type == insert_at_head_for_non_empty_timer)
    {
    if (!(rt->is_armed))
      KOUT(" ");
    if ((current_timeout.it_value.tv_sec > 0) || 
        (current_timeout.it_value.tv_nsec > 10000))
      reload = 1;
    }
  else
    KOUT("%d", type);
  if (reload)
    {
    if (timer_settime(rt->initialized_timer, 0, &timeout, NULL))
      KOUT(" ");
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_chain_timer *look_for_chain_timer(t_real_timer *timer, 
                                           long long date_us)
{
  int i;
  t_chain_timer *cur;
  t_chain_timer *chtim = NULL;
  for (i=1; i<MAX_REAL_TIMER; i++)
    {
    cur = timer->tstore[i].head_chain_timer;
    while(cur)
      {
      if (cur->date_us == date_us)
        {
        chtim = cur; 
        break;
        }
      cur = cur->next;
      }
    if (chtim)
      break;
    }
  return chtim;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int insert_chain_timer(t_real_timer_item *rt, long long date_us,
                              t_fct_real_timer cb, void *data) 
{
  t_chain_timer *cur = rt->head_chain_timer;
  t_chain_timer *prev = NULL;
  t_chain_timer *chtim;
  int result = 0;
  chtim = (t_chain_timer *) malloc(sizeof(t_chain_timer));
  memset(chtim, 0, sizeof(t_chain_timer));
  chtim->date_us = date_us;
  chtim->cb = cb;
  chtim->data = data;
  while(cur)
    {
    if (cur->date_us > date_us)
      break;
    prev = cur;
    cur = cur->next;
    }
  if (rt->head_chain_timer == NULL)
    {
    result = insert_at_head_for_empty_timer;
    rt->head_chain_timer = chtim;
    }
  else
    {
    if (prev == NULL)
      {
      result = insert_at_head_for_non_empty_timer;
      chtim->next = rt->head_chain_timer;
      rt->head_chain_timer->prev = chtim;
      rt->head_chain_timer = chtim;
      }
    else
      {
      chtim->prev = prev;
      chtim->next = cur;
      prev->next = chtim;
      if (cur)
        cur->prev = chtim;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static t_real_timer_item *alloc_real_timer(t_real_timer *timer)
{
  int i, dist, id_check = fifo_real_timer_alloc(timer);
  t_real_timer_item *cur = NULL;
  int start;
  if (id_check)
    {
    cur = &(timer->tstore[id_check]);
    cur->id_check = id_check;
    if (cur->is_locked)
      KOUT(" ");
    if (cur->head_chain_timer)
      KOUT(" ");
    }
  else
    {
    start = (rand() % (MAX_REAL_TIMER-1)) + 1;
    for (i=start; i<MAX_REAL_TIMER; i++)
      {
      if (timer->tstore[i].is_locked)
        continue;
      dist = distance_real_timer (&(timer->tstore[i]));
      if (dist)
        {
        cur = &(timer->tstore[i]);
        break;
        }
      }
    if (!cur)
      {
      for (i=1; i<start; i++)
        {
        if (timer->tstore[i].is_locked)
          continue;
        dist = distance_real_timer (&(timer->tstore[i]));
        if (dist)
          {
          cur = &(timer->tstore[i]);
          break;
          }
        }
      }
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void free_real_timer(t_real_timer *timer, t_real_timer_item *rt)
{
  t_chain_timer *to_free = rt->head_chain_timer;
  if (rt->id_check == 0)
    KOUT(" ");
  if (rt->head_chain_timer == NULL)
    KOUT(" ");
  if (rt->head_chain_timer->next)
    rt->head_chain_timer->next->prev = NULL;
  rt->head_chain_timer = rt->head_chain_timer->next;
  free(to_free);
  if (rt->head_chain_timer == NULL)
    {
    fifo_real_timer_release(timer, rt->id_check);
    rt->head_chain_timer = NULL;
    rt->id_check = 0;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int clownix_real_timer_add(int idx, int nb_us, t_fct_real_timer cb, 
                             void *data, long long *date_us)
{
  int res, result = -1;
  t_real_timer_item *rt;
  long long loc_date_us;
  if (cb == NULL)
    KOUT(" ");
  if (nb_us < 1)
    KOUT(" ");
  if (nb_us > 600000000)
    KOUT("%d", nb_us);
  if (!g_real_timer[idx])
    KERR(" ");
  else
    {
    rt = alloc_real_timer(g_real_timer[idx]);
    if (rt)
      {
      loc_date_us = get_target_date_us();
      loc_date_us += nb_us;
      while(look_for_chain_timer(g_real_timer[idx], loc_date_us))
        loc_date_us += 100;
      result = 0;
      if (date_us)
        *date_us = loc_date_us;
      res = insert_chain_timer(rt, loc_date_us, cb, data);
      if (res)
        {
        arm_real_timer(rt, res);
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void err_event_cb(void *ptr, int llid, int err, int from)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void rx_real_time_event_timeout(int idx, int val)
{
  long long delta;
  long delta_ns = 0;;
  t_chain_timer *chtim;
  if (!g_real_timer[idx])
    KERR(" ");
  else
    {
    if ((val<=0) || (val>=MAX_REAL_TIMER))
      KOUT("%d", val);
    if (!(g_real_timer[idx]->tstore[val].is_armed))
      KOUT(" ");
    g_real_timer[idx]->tstore[val].is_armed = 0;
    g_real_timer[idx]->tstore[val].is_locked = 0;
    chtim = g_real_timer[idx]->tstore[val].head_chain_timer;
    if (!chtim)
      KOUT(" ");
    if (!(chtim->cb))
      KOUT("%d", val);
    if (!chtim->inhibited)
      {
      chtim->inhibited = 111;
      if (chtim->async_rx)
        {
        delta = clownix_gettimeofday_ns();
        delta -= chtim->async_rx;
        delta_ns = (long) delta;
        }
      chtim->cb(delta_ns, chtim->data);
      }
    free_real_timer(g_real_timer[idx], &(g_real_timer[idx]->tstore[val]));
    chtim = g_real_timer[idx]->tstore[val].head_chain_timer;
    if (chtim)
      arm_real_timer(&(g_real_timer[idx]->tstore[val]), rearm_after_timeout);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int rx_event_cb(int idx, int llid, int fd)
{
  char buffer[512];
  int i, len, val;
  do
    {
    len = read(fd, buffer, sizeof(buffer));
    if (len > 0)
      {
      if (len > 1000)
        KOUT("%d", len);
      for (i=0; i<len; i++)
        {
        val = (int) ((unsigned char) (buffer[i] & 0xFF));
        rx_real_time_event_timeout(idx, val);
        }
      }
    } while ((len == -1 && errno == EINTR));
  return len;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int rx0_event_cb(void *ptr, int llid, int fd)
{
  return rx_event_cb(0, llid, fd);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int rx1_event_cb(void *ptr, int llid, int fd)
{
  return rx_event_cb(1, llid, fd);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void real_time_handler(int signum, siginfo_t *info, void *nouse)
{
  int val, idx;
  char sival;
  sival = (char) (info->_sifields._rt.si_sigval.sival_int);
  if (signum != SIGRTMIN+7)
    KOUT(" ");
  val = (int) ((unsigned char) (sival & 0xFF));
  if ((val<=0) || (val>=2*MAX_REAL_TIMER))
    KOUT("%d", val);
  if (val < MAX_REAL_TIMER)
    idx = 0;
  else
    {
    val -= MAX_REAL_TIMER;
    idx = 1;
    sival = (char) val;
    }
  if (!(g_real_timer[idx]->tstore[val].is_armed))
    KOUT(" ");
  g_real_timer[idx]->tstore[val].is_locked = 1;
  if (write(g_real_timer[idx]->real_time_event_fd,&sival,sizeof(sival))!=1)
    KOUT("%d", errno);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void start_real_timer_item(int sival, t_real_timer_item *rt)
{
  struct itimerspec timeout;
  struct sigevent ev;
  struct sigaction act;
  memset(&timeout, 0, sizeof(struct itimerspec));
  memset(&act, 0, sizeof(struct sigaction));
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_SIGINFO;
  act.sa_sigaction = real_time_handler;
  if (sigaction(SIGRTMIN+7, &act, NULL) < 0)
    KOUT(" ");
  memset(&ev, 0, sizeof(ev));
  ev.sigev_value.sival_int = sival;
  ev.sigev_notify = SIGEV_SIGNAL;
  ev.sigev_signo = SIGRTMIN+7;
  if (timer_create(CLOCK_MONOTONIC, &ev, &(rt->initialized_timer)))
    KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void clownix_real_timer_init(int idx, t_all_ctx *all_ctx)
{ 
  int i, fds[2];
  if (g_real_timer[idx])
    KOUT("GLOBAL CTX ALREADY DEFINED");
  g_real_timer[idx] = (t_real_timer *) malloc(sizeof(t_real_timer));
  memset(g_real_timer[idx], 0, sizeof(t_real_timer));
  fifo_real_timer_init(g_real_timer[idx]);
  memset(g_real_timer[idx]->tstore,0,MAX_REAL_TIMER*sizeof(t_real_timer_item));
  if (pipe(fds))
    KOUT(" ");
  nonblock_fd(fds[0]);
  nonblock_fd(fds[1]);
  (g_real_timer[idx])->real_time_event_fd = fds[1];
  if ((fds[0] < 0) || (fds[0] >= MAX_SELECT_CHANNELS-1))
    KOUT("%d", fds[0]);
  if (idx == 0)
    {
    msg_watch_fd(all_ctx, fds[0], rx0_event_cb, err_event_cb);
    for (i=1; i<MAX_REAL_TIMER; i++)
      start_real_timer_item(i, &((g_real_timer[idx])->tstore[i]));
    }
  else if (idx == 1)
    {
    msg_watch_fd(all_ctx, fds[0], rx1_event_cb, err_event_cb);
    for (i=1; i<MAX_REAL_TIMER; i++)
      start_real_timer_item(i+MAX_REAL_TIMER, 
                            &((g_real_timer[idx])->tstore[i]));
    }
  else
    KOUT("%d", idx);
}
/*---------------------------------------------------------------------------*/


