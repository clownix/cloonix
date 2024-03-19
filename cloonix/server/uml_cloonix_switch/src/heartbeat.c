/*****************************************************************************/
/*    Copyright (C) 2006-2024 clownix@clownix.net License AGPL-3             */
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
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <dirent.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "heartbeat.h"
#include "machine_create.h"
#include "event_subscriber.h"
#include "pid_clone.h"
#include "automates.h"
#include "llid_trace.h"
#include "stats_counters.h"

#define MAX_TIME_SAMPLES 1000

static int glob_above50ms = 0;
static int glob_above20ms = 0; 
static int glob_above15ms = 0; 

/*****************************************************************************/
static int32_t time_samples[MAX_TIME_SAMPLES];
static int time_samples_idx;
static int last_delta;

/*****************************************************************************/
static void store_time_sample(int delta)
{
  if (delta > 500)
    glob_above50ms += 1;
  else if (delta > 200)
    glob_above20ms += 1;
  else if (delta > 150)
    glob_above15ms += 1;
  time_samples[time_samples_idx] = (int32_t) delta;
  time_samples_idx++;
  if (time_samples_idx == MAX_TIME_SAMPLES)
    time_samples_idx = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void extract_values_from_time_samples(int *max_time,   int *avg_time,
                                             int *above50ms, int *above20ms,
                                             int *above15ms)
{
  int i;
  unsigned long long sum = 0;
  *max_time   = 0;
  for (i=0; i<MAX_TIME_SAMPLES; i++)
    {
    if (*max_time < time_samples[i])
      *max_time = time_samples[i];
    sum += time_samples[i];
    }
  *above50ms = glob_above50ms;
  *above20ms = glob_above20ms;
  *above15ms = glob_above15ms;
  *max_time = *max_time/10;
  sum = sum/MAX_TIME_SAMPLES;
  sum = sum/10;
  *avg_time = (int) sum; 
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_sys_info *create_sys_event(void)
{
  unsigned long *mallocs;
  int win, wout, werr, waked_count, fd, i, pqsiz, qsiz, idx;
  t_sys_info *sys = (t_sys_info *) clownix_malloc(sizeof(t_sys_info),15);
  int nb_chan = get_clownix_channels_nb();
  t_queue_tx *qtx=(t_queue_tx *) clownix_malloc(nb_chan*sizeof(t_queue_tx),15);
  memset (sys, 0, sizeof(t_sys_info));
  memset (qtx, 0, nb_chan*sizeof(t_queue_tx));
  for (idx = 0, i=0; i<=nb_chan; i++)
    {
    qtx[idx].llid = channel_get_llid_with_cidx(i);
    if (qtx[idx].llid)
      {
      fd = get_fd_with_llid(qtx[idx].llid);
      win = get_waked_in_with_cidx(i);
      wout = get_waked_out_with_cidx(i);
      werr = get_waked_err_with_cidx(i);
      waked_count = win+wout+werr;
      if ((fd + 1) != i) 
        KERR("%d %d", fd, i);
      pqsiz = (int) msg_get_tx_peak_queue_len(qtx[idx].llid);
      qsiz = (int) channel_get_tx_queue_len(qtx[idx].llid);
      if (qsiz > pqsiz)
        pqsiz = qsiz;

      if ((pqsiz) || (waked_count > 100))
        {
        qtx[idx].fd = fd;
        qtx[idx].waked_count_in = win;
        qtx[idx].waked_count_out = wout;
        qtx[idx].waked_count_err = werr;
        qtx[idx].out_bytes = get_out_bytes_with_cidx(i);
        qtx[idx].in_bytes = get_in_bytes_with_cidx(i);
        qtx[idx].peak_size = pqsiz;
        qtx[idx].size = (int) channel_get_tx_queue_len(qtx[idx].llid);
        qtx[idx].type = llid_get_type_of_con(qtx[idx].llid, qtx[idx].name, 
                                             &(qtx[idx].id));
        idx++;
        }
      }
    }
  sys->queue_tx     = qtx;
  sys->nb_queue_tx  = idx;
  sys->selects      = get_counter_select_speed();
  mallocs = get_clownix_malloc_nb();
  for (i=0; i<MAX_MALLOC_TYPES; i++)
    sys->mallocs[i] = mallocs[i];
  for (i=0; i<type_llid_max; i++)
    sys->fds_used[i] = (int) cfg_get_trace_fd_qty(i);
  sys->cur_channels = get_clownix_channels_nb();
  sys->max_channels = get_clownix_channels_max();
  sys->cur_channels_recv = get_channel_cur_recv();
  sys->cur_channels_send = get_channel_cur_send();
  sys->clients      =  (int) cfg_get_trace_fd_qty(type_llid_trace_client_unix);
  extract_values_from_time_samples(&(sys->max_time), &(sys->avg_time), 
                                   &(sys->above50ms), &(sys->above20ms), 
                                   &(sys->above15ms));
  return sys;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int heart_get_last_delta(void)
{
  return (last_delta);
}
/*---------------------------------------------------------------------------*/

/******************************************************************************/
static int get_my_open_fds_qty(void)
{
  int result=0;
  char dir[MAX_PATH_LEN];
  DIR *dirptr;
  struct dirent *ent;
  sprintf(dir, "/proc/%d/fd", getpid());
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void loss_of_fd_trap(void)
{
  int open_fds, declared_fds;
  open_fds = get_my_open_fds_qty();
  declared_fds =  (int) cfg_get_trace_fd_qty(0);
  if (open_fds - declared_fds > 8)
    event_print("open fds:%d declared fds:%d Diff:%d",
                open_fds, declared_fds, open_fds - declared_fds);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void heartbeat (int delta)
{
  static int count = 0;
  static int seconde_count = 0;
  static int ten_seconde_count = 0;
  t_sys_info *sys;
  store_time_sample(delta);
  count++;
  last_delta += delta;
  if (!(count%HALF_SEC_BEAT))
    {
    count = 0;
    seconde_count++;
    ten_seconde_count++;
    stats_counters_heartbeat();
    if (seconde_count == 2)
      {
      seconde_count = 0;
      sys = create_sys_event();
      event_subscriber_send(sub_evt_sys, (void *)sys);
      last_delta = 0;
      llid_count_beat();
      }
    if (ten_seconde_count == 20)
      {
      ten_seconde_count = 0;
      loss_of_fd_trap();
      }
    automates_safety_heartbeat();
    }
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
void init_heartbeat(void)
{
  int i;
  for (i=0; i<MAX_TIME_SAMPLES; i++)
    time_samples[i] = 0;
  time_samples_idx = 0;
  last_delta = 0;
  msg_mngt_heartbeat_init(heartbeat);
}
