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
#include "rpc_clownix.h"
#include "timeout_service.h"

#define MAX_JOBS 100


typedef struct t_timeout_service_item
  {
  int job_idx;
  t_timeout_service_cb cb;
  int has_been_triggered;
  void *opaque;
  } t_timeout_service_item;

static int pool_fifo_free_index[MAX_JOBS];
static int pool_read_idx;
static int pool_write_idx;

static t_timeout_service_item item_tab[MAX_JOBS+1];


/*****************************************************************************/
static void pool_init(void)
{
  int i;
  for(i = 0; i < MAX_JOBS; i++)
    pool_fifo_free_index[i] = i+1;
  pool_read_idx = 0;
  pool_write_idx =  MAX_JOBS - 1;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int pool_alloc(void)
{
  int job_idx = 0;
  if(pool_read_idx != pool_write_idx)
    {
    job_idx = pool_fifo_free_index[pool_read_idx];
    pool_read_idx = (pool_read_idx + 1)%MAX_JOBS;
    }
  return job_idx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void pool_release(int job_idx)
{
  pool_fifo_free_index[pool_write_idx] =  job_idx;
  pool_write_idx=(pool_write_idx + 1)%MAX_JOBS;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void timeout_service_trigger(int job_idx)
{
  if ((job_idx<=0) || (job_idx>MAX_JOBS))
    KOUT("%d", job_idx);
  if (!item_tab[job_idx].job_idx)
    KERR(" ");
  else
    {
    item_tab[job_idx].has_been_triggered = 1;
    item_tab[job_idx].cb(job_idx, 0, item_tab[job_idx].opaque);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timeout(void *data)
{
  unsigned long uljob_idx = (unsigned long) data; 
  int job_idx = (int) uljob_idx;
  if ((job_idx<=0) || (job_idx>MAX_JOBS))
    KOUT("%d", job_idx);
  if (!item_tab[job_idx].job_idx)
    KERR(" ");
  else
    {
    if (!item_tab[job_idx].has_been_triggered)
      item_tab[job_idx].cb(job_idx, 1, item_tab[job_idx].opaque);
    pool_release(job_idx);
    memset(&(item_tab[job_idx]), 0, sizeof(t_timeout_service_item));
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int timeout_service_alloc(t_timeout_service_cb cb, void *opaque, int delay)
{
  int result = 0;
  int job_idx = pool_alloc();
  unsigned long uljob_idx = (unsigned long) job_idx; 
  if (job_idx)
    {
    if ((job_idx<0) || (job_idx>MAX_JOBS))
      KOUT("%d", job_idx);
    if (item_tab[job_idx].job_idx)
      KERR(" ");
    else
      {
      result = job_idx; 
      item_tab[job_idx].job_idx = job_idx;
      item_tab[job_idx].cb = cb;
      item_tab[job_idx].opaque = opaque;
      item_tab[job_idx].has_been_triggered = 0;
      clownix_timeout_add(delay, timeout, (void *)uljob_idx, NULL, NULL); 
      }
    }
  return result;
} 
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void timeout_service_init(void)
{
  pool_init();
  memset(item_tab, 0, (MAX_JOBS+1) * sizeof(t_timeout_service_item));
}
/*--------------------------------------------------------------------------*/


