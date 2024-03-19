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
#include <sys/sysinfo.h>



#include "io_clownix.h"
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "event_subscriber.h"
#include "commun_daemon.h"
#include "header_sock.h"


typedef struct t_sysinfo_sub
{
  char name[MAX_NAME_LEN];
  char *df_payload;
  int llid;
  int tid;
  struct t_sysinfo_sub *prev;
  struct t_sysinfo_sub *next;
} t_sysinfo_sub;


static int glob_rss_shift;
static t_sysinfo_sub *g_head_sysinfo_sub;


/****************************************************************************/
static int read_proc_pid_stat(t_pid_info *pid_info, int pid)
{
  int result = -1;
  FILE *fp;
  char filename[MAX_PATH_LEN];
  char *ptr;
  sprintf(filename,  "/proc/%u/stat", pid);
  if ((fp = fopen(filename, "r")) != NULL)
    {
    fscanf(fp, STAT_FORMAT, pid_info->comm,
                            &(pid_info->utime),  &(pid_info->stime),
                            &(pid_info->cutime), &(pid_info->cstime),
                            &(pid_info->rss));
    fclose(fp);
    pid_info->rss = ((pid_info->rss) << glob_rss_shift);
    ptr = strchr(pid_info->comm, ')');
    if (ptr)
      {
      *ptr = 0;
      result = 0;
      }
    else
      KERR("%s", pid_info->comm); 
    }
  else
    KERR("%s", filename);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int get_rss_shift(void)
{
  int shift = 0;
  long size;
  if ((size = sysconf(_SC_PAGESIZE)) == -1)
    {
    printf("ERROR SYSCONF\n");
    KERR(" ");
    exit(-1);
    }
  size >>= 10;
  while (size > 1)
    {
    shift++;
    size >>= 1;
    }
  return shift;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_sysinfo_sub *find_sysinfo_sub_with_llid(int llid)
{
  t_sysinfo_sub *cur = g_head_sysinfo_sub;
  while (cur)
    {
    if (cur->llid == llid)
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_sysinfo_sub *find_sysinfo_sub_with_name(t_sysinfo_sub *head, char *name)
{
  t_sysinfo_sub *cur;
  if (head == NULL)
    cur = g_head_sysinfo_sub;
  else
    cur = head->next;
  while (cur)
    {
    if (!strcmp(cur->name, name))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static t_sysinfo_sub *find_sysinfo_sub(char *name, int llid)
{
  t_sysinfo_sub *cur = g_head_sysinfo_sub;
  while (cur)
    {
    if ((cur->llid == llid) && (!strcmp(cur->name, name)))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int alloc_sysinfo_sub(char *name, int llid, int tid)
{
  int result = -1;
  t_sysinfo_sub *cur = find_sysinfo_sub(name, llid);
  if (!cur)
    {
    result = 0;
    cur = (t_sysinfo_sub *) clownix_malloc(sizeof(t_sysinfo_sub), 7);
    memset(cur, 0, sizeof(t_sysinfo_sub));
    strncpy(cur->name, name, MAX_NAME_LEN-1);
    cur->llid = llid;
    cur->tid = tid;
    if (g_head_sysinfo_sub)
      g_head_sysinfo_sub->prev = cur;
    cur->next = g_head_sysinfo_sub;
    g_head_sysinfo_sub = cur;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_sysinfo_sub(t_sysinfo_sub *sysinfo_sub)
{
  if (sysinfo_sub->df_payload)
    {
    clownix_free(sysinfo_sub->df_payload, __FUNCTION__);
    sysinfo_sub->df_payload = NULL;
    }
  if (sysinfo_sub->next)
    sysinfo_sub->next->prev = sysinfo_sub->prev;
  if (sysinfo_sub->prev)
    sysinfo_sub->prev->next = sysinfo_sub->next;
  if (sysinfo_sub == g_head_sysinfo_sub)
    g_head_sysinfo_sub = sysinfo_sub->next;
  clownix_free(sysinfo_sub, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void recv_evt_stats_sysinfo_sub(int llid, int tid, char *name, int sub)
{
  char *network = cfg_get_cloonix_name();
  t_sysinfo_sub *cur;
  t_stats_sysinfo si;
  if (sub)
    {
    if (alloc_sysinfo_sub(name, llid, tid))
      {
      KERR("ERROR: %s", name);
      memset(&si, 0, sizeof(t_stats_sysinfo));
      send_evt_stats_sysinfo(llid, tid, network, name, &si, NULL, 1);
      }
    }
  else
    {
    cur = find_sysinfo_sub(name, llid);
    if (cur)
      free_sysinfo_sub(cur);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void stats_counters_sysinfo_vm_death(char *name)
{
  char *network = cfg_get_cloonix_name();
  t_sysinfo_sub *cur, *next;
  t_stats_sysinfo si;
  memset(&si, 0, sizeof(t_stats_sysinfo));
  cur = find_sysinfo_sub_with_name(NULL, name);
  while (cur)
    {
    next = find_sysinfo_sub_with_name(cur, name);
    send_evt_stats_sysinfo(cur->llid, cur->tid, network, 
                           cur->name, &si, NULL, 1);
    free_sysinfo_sub(cur);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void stats_counters_sysinfo_llid_close(int llid)
{
  t_sysinfo_sub *cur;
  cur = find_sysinfo_sub_with_llid(llid);
  while (cur)
    {
    free_sysinfo_sub(cur);
    cur = find_sysinfo_sub_with_llid(llid);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void stats_counters_sysinfo_update_df(char *name, char *payload)
{
  int len;
  t_vm *vm = cfg_get_vm(name);
  t_sysinfo_sub *cur, *next;
  if (vm)
    {
    cur = find_sysinfo_sub_with_name(NULL, name);
    while(cur)
      {
      next = find_sysinfo_sub_with_name(cur, name);
      if (cur->df_payload)
        {
        clownix_free(cur->df_payload, __FUNCTION__);
        cur->df_payload = NULL;
        }
      if (payload)
        {
        len = strlen(payload);
        cur->df_payload = (char *) clownix_malloc(len + 1, 5);
        memcpy(cur->df_payload, payload, len);
        cur->df_payload[len] = 0; 
        }
      cur = next;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void stats_counters_sysinfo_update(char *name, 
                                   unsigned long uptime,
                                   unsigned long load1, 
                                   unsigned long load5,
                                   unsigned long load15, 
                                   unsigned long totalram,
                                   unsigned long freeram, 
                                   unsigned long cachedram, 
                                   unsigned long sharedram,
                                   unsigned long bufferram, 
                                   unsigned long totalswap,
                                   unsigned long freeswap, 
                                   unsigned long procs,
                                   unsigned long totalhigh, 
                                   unsigned long freehigh,
                                   unsigned long mem_unit)
{
  char *network = cfg_get_cloonix_name();
  t_vm *vm = cfg_get_vm(name);
  t_pid_info pid_info;
  t_stats_sysinfo sysinfo;
  t_sysinfo_sub *cur, *next;
  if ((vm) && (vm->pid))
    {
    cur = find_sysinfo_sub_with_name(NULL, name);
    while(cur)
      {
      next = find_sysinfo_sub_with_name(cur, name);
      memset(&pid_info, 0, sizeof(t_pid_info));
      memset(&sysinfo, 0, sizeof(t_stats_sysinfo));
      if (!read_proc_pid_stat(&pid_info, vm->pid))
        {
        sysinfo.time_ms        = (unsigned int) cloonix_get_msec();
        sysinfo.uptime         = uptime;
        sysinfo.load1          = load1;
        sysinfo.load5          = load5;
        sysinfo.load15         = load15;
        sysinfo.totalram       = totalram;
        sysinfo.freeram        = freeram;
        sysinfo.cachedram      = cachedram;
        sysinfo.sharedram      = sharedram;
        sysinfo.bufferram      = bufferram;
        sysinfo.totalswap      = totalswap;
        sysinfo.freeswap       = freeswap;
        sysinfo.procs          = procs;
        sysinfo.totalhigh      = totalhigh;
        sysinfo.freehigh       = freehigh;
        sysinfo.mem_unit       = mem_unit;
        sysinfo.process_utime  = pid_info.utime;
        sysinfo.process_stime  = pid_info.stime;
        sysinfo.process_cutime = pid_info.cutime;
        sysinfo.process_cstime = pid_info.cstime;
        sysinfo.process_rss    = pid_info.rss;
        send_evt_stats_sysinfo(cur->llid, cur->tid, network,
                               name, &sysinfo, cur->df_payload, 0);
        }
      else
        KERR("%s", name);
      cur = next;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void stats_counters_sysinfo_init(void)
{
  g_head_sysinfo_sub = NULL;
  glob_rss_shift = get_rss_shift();
}
/*--------------------------------------------------------------------------*/

