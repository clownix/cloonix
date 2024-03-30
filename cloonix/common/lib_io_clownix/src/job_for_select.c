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
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>

#include "io_clownix.h"


#define MEMORY_BARRIER()  asm volatile ("": : :"memory")

#define MAX_JOBS_MASK (0xFF)



/*****************************************************************************/
typedef struct t_job
{
  t_fct_job cb;
  void *data;
} t_job;
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
typedef struct t_job_idx_pool
  {
  int fifo_free_index[MAX_JOBS_MASK+1];
  uint32_t volatile read_idx;
  uint32_t volatile write_idx;
  } t_job_idx_pool;
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
typedef struct t_jfs_obj
  {
  int magic_number;
  int ref_id;
  t_job_idx_pool job_idx_pool;
  int fd_request;
  int fd_ack;
  t_job glob_jobs[MAX_JOBS_MASK+2];
  short buffer[MAX_JOBS_MASK+2];
  int llid_request;
  int llid_ack;
  struct t_jfs_obj *prev;
  struct t_jfs_obj *next;
  } t_jfs_obj;
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_jfs_obj *g_head_jfs;
static t_jfs_obj *g_jfs_obj_tab[CLOWNIX_MAX_CHANNELS];
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void job_idx_pool_init(t_jfs_obj *jfs)
{
  int i;
  for(i = 0; i < MAX_JOBS_MASK+1; i++)
    jfs->job_idx_pool.fifo_free_index[i] = i+1;
  jfs->job_idx_pool.read_idx = 0;
  jfs->job_idx_pool.write_idx = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int job_idx_pool_alloc(t_jfs_obj *jfs)
{
  int job_idx = 0;
  uint32_t read_idx, write_idx, new_read_idx;
  read_idx  = jfs->job_idx_pool.read_idx;
  write_idx = jfs->job_idx_pool.write_idx;
  if ((write_idx - (read_idx+1)) & MAX_JOBS_MASK)
    {
    job_idx = jfs->job_idx_pool.fifo_free_index[read_idx];
    if ((job_idx <= 0) || (job_idx > MAX_JOBS_MASK+1))
      KOUT("%d %d %d", job_idx, read_idx, write_idx); 
    new_read_idx = (read_idx + 1) & MAX_JOBS_MASK;
    MEMORY_BARRIER();
    if (__sync_val_compare_and_swap(&(jfs->job_idx_pool.read_idx),
                                    read_idx, new_read_idx) != read_idx)
      KOUT("%x %x", new_read_idx, read_idx);
    }
  return job_idx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void job_idx_pool_release(t_jfs_obj *jfs, int job_idx)
{
  uint32_t read_idx, write_idx, new_write_idx;
  read_idx  = jfs->job_idx_pool.read_idx;
  write_idx = jfs->job_idx_pool.write_idx;
  if ((read_idx - write_idx) & MAX_JOBS_MASK)
    {
    if ((job_idx <= 0) || (job_idx > MAX_JOBS_MASK+1))
      KOUT("%d %d %d", job_idx, read_idx, write_idx); 
    jfs->job_idx_pool.fifo_free_index[write_idx] =  job_idx;
    new_write_idx = (write_idx + 1) & MAX_JOBS_MASK;
    MEMORY_BARRIER();
    if (__sync_val_compare_and_swap(&(jfs->job_idx_pool.write_idx),
                                    write_idx, new_write_idx) != write_idx)
      KOUT("%x %x", new_write_idx, write_idx);
    }
  else
    KOUT("%d", job_idx);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void err_rx(int llid, int err, int from)
{
  KOUT(" %d ", llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int rx_err(int llid, int fd)
{
  KOUT(" %d ", llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int rx_ack(int llid, int fd)
{
  int i, len, val;
  t_jfs_obj *jfs = g_jfs_obj_tab[llid];
  if (!jfs)
    KOUT(" ");
  if (jfs->magic_number != 0xFECACAFE)
    KOUT("%0x", jfs->magic_number);
  if (jfs->llid_ack != llid)
    KOUT(" %d %d ", llid, jfs->llid_ack);
  do
    {
    len = read(fd, jfs->buffer, sizeof(jfs->buffer));
    if (len > 0)
      {
      len /= sizeof(short);
      if (len > MAX_JOBS_MASK+1)
        KOUT("%d", len);
      for (i=0; i<len; i++)
        {
        val = (int) (jfs->buffer[i]);
        val &= 0xFFFF;
        if ((val <= 0) || (val > MAX_JOBS_MASK+1))
          KOUT("%d", val);
        if (!(jfs->glob_jobs[val].cb))
          KOUT(" "); 
        jfs->glob_jobs[val].cb(jfs->ref_id, jfs->glob_jobs[val].data);
        memset(&(jfs->glob_jobs[val]), 0, sizeof(t_job));
        job_idx_pool_release(jfs, val);
        }
      }
    } while ((len == -1 && errno == EINTR));
  return len;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int job_for_select_request(void *hand, t_fct_job cb, void *data)
{
  int idx, result = -1;
  short cidx;
  t_jfs_obj *jfs = (t_jfs_obj *) hand;
  if ((jfs) && (jfs->magic_number == 0xFECACAFE))
    {
    idx = job_idx_pool_alloc(jfs);
    if (idx)
      {
      result = 0;
      if (jfs->glob_jobs[idx].cb || jfs->glob_jobs[idx].data)
        KOUT("%d", idx); 
      jfs->glob_jobs[idx].cb = cb;
      jfs->glob_jobs[idx].data = data;
      cidx = (short) (idx & 0xFFFF);
      if (write(jfs->fd_request, &cidx, sizeof(cidx)) != sizeof(cidx))
        KERR("ERROR %d", errno);
      }
    else
      KERR("ERROR ALLOC");
    }
  return result;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void *job_for_select_alloc(int ref_id, int *llid1, int *llid2)
{
  t_jfs_obj *result;
  int fds[2];
  result = (t_jfs_obj *) clownix_malloc(sizeof(t_jfs_obj), 21); 
  memset(result, 0, sizeof(t_jfs_obj));
  result->magic_number = 0xFECACAFE;
  result->ref_id = ref_id;
  if (g_head_jfs)
    g_head_jfs->prev = result;
  result->next = g_head_jfs;
  g_head_jfs = result;
  job_idx_pool_init(result);
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds))
    KOUT(" ");
  result->fd_request = fds[1];
  result->fd_ack = fds[0];
  result->llid_ack = msg_watch_fd(result->fd_ack, rx_ack, err_rx, "jfs_ack");
  result->llid_request = msg_watch_fd(result->fd_request, rx_err, err_rx,
                                      "jfs_req");
  *llid1 = result->llid_ack;
  *llid2 = result->llid_request;
  g_jfs_obj_tab[result->llid_ack] = result;
  return ((void *) result);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void job_for_select_free(void *hand, int *llid1, int *llid2)
{
  t_jfs_obj *jfs = (t_jfs_obj *) hand;
  if (jfs)
    {
    if (jfs->magic_number != 0xFECACAFE)
      KOUT("%0x", jfs->magic_number);
    jfs->magic_number = 0;
    if (g_jfs_obj_tab[jfs->llid_ack] != jfs)
      KOUT(" %d %p %p ", jfs->llid_ack, g_jfs_obj_tab[jfs->llid_ack], jfs);
    *llid1 = jfs->llid_ack;
    *llid2 = jfs->llid_request;
    if (msg_exist_channel(jfs->llid_ack))
      msg_delete_channel(jfs->llid_ack);
    if (msg_exist_channel(jfs->llid_request))
      msg_delete_channel(jfs->llid_request);
    g_jfs_obj_tab[jfs->llid_ack] = NULL;
    if (jfs->prev)
      jfs->prev->next = jfs->next;
    if (jfs->next)
      jfs->next->prev = jfs->prev;
    if (jfs == g_head_jfs)
      g_head_jfs = jfs->next;
    clownix_free(jfs, __FUNCTION__);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void job_for_select_close_fd(void)
{
  int llid1, llid2;
  t_jfs_obj *next, *cur = g_head_jfs;
  while (cur)
    {
    next = cur->next;
    job_for_select_free((void *) cur, &llid1, &llid2);
    cur = next;
    } 
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void job_for_select_init(void)
{
  g_head_jfs = NULL;
  memset(g_jfs_obj_tab, 0, CLOWNIX_MAX_CHANNELS * (sizeof(t_jfs_obj *)));
}
/*---------------------------------------------------------------------------*/



