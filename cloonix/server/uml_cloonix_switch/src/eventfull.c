/*****************************************************************************/
/*    Copyright (C) 2006-2022 clownix@clownix.net License AGPL-3             */
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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sysinfo.h>

#include "io_clownix.h"
#include "commun_daemon.h"
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "event_subscriber.h"
#include "machine_create.h"
#include "utils_cmd_line_maker.h"
#include "ovs_snf.h"
#include "cnt.h"


/*****************************************************************************/
typedef struct t_eventfull_subs
{
  int llid;
  int tid;
  struct t_eventfull_subs *prev;
  struct t_eventfull_subs *next;
} t_eventfull_subs;

static t_eventfull_subs *head_eventfull_subs;
static int glob_rss_shift;
static unsigned long glob_total_ram;
/*---------------------------------------------------------------------------*/

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
static unsigned long get_totalram_kb(void)
{
  unsigned long result;
  struct sysinfo sys_info;
  if (sysinfo(&sys_info))
    {
    printf("ERROR %s\n", __FUNCTION__);
    KERR(" ");
    exit(-1);
    }
  result = (unsigned long) sys_info.totalram;
  result >>= 10;
  result *= sys_info.mem_unit;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_pid_info *read_proc_pid_stat(int pid)
{
  static t_pid_info pid_info;
  t_pid_info *result = NULL;
  FILE *fp;
  char filename[MAX_PATH_LEN];
  char *ptr;
  memset(&pid_info, 0, sizeof(t_pid_info));
  sprintf(filename,  "/proc/%u/stat", pid);
  if ((fp = fopen(filename, "r")) != NULL)
    {
    fscanf(fp, STAT_FORMAT, pid_info.comm,
                            &(pid_info.utime),  &(pid_info.stime),
                            &(pid_info.cutime), &(pid_info.cstime),
                            &(pid_info.rss));
    fclose(fp);
    pid_info.rss = ((pid_info.rss) << glob_rss_shift);
    ptr = strchr(pid_info.comm, ')');
    if (ptr)
      {
      *ptr = 0;
      result = &pid_info;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timeout_delete_vm(void *data)
{
  char *name = data;
  t_vm *vm = cfg_get_vm(name);
  if (vm)
    {
    if (!vm->vm_to_be_killed)
      {
      poweroff_vm(0, 0, vm);
      }
    }
  clownix_free(data, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_pid_info *read_proc_all_pids_stat(t_vm *vm)
{
  t_pid_info *pifo;
  pifo = read_proc_pid_stat(vm->pid);
  return pifo;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int update_pid_infos(t_vm *vm)
{
  static int not_first_time = 0;
  int result = -1;
  char *name;
  unsigned long mem, tot_utime;
  t_pid_info *pid_info;
  if (!vm->pid)
    result = 0;
  else
    {
    pid_info = read_proc_all_pids_stat(vm);
    if (!pid_info)
      {
      name = (char *) clownix_malloc(MAX_NAME_LEN, 13);
      memset(name, 0, MAX_NAME_LEN);
      strncpy(name, vm->kvm.name, MAX_NAME_LEN-1);
      clownix_timeout_add(1, timeout_delete_vm, (void *)name, NULL, NULL);
      }
    else
      {
      result = 0;
      mem = pid_info->rss;
      tot_utime =  (pid_info->utime  - vm->previous_utime)  +
                   (pid_info->cutime - vm->previous_cutime) +
                   (pid_info->stime  - vm->previous_stime)  +
                   (pid_info->cstime - vm->previous_cstime) ;
      vm->previous_utime   = pid_info->utime;
      vm->previous_cutime  = pid_info->cutime;
      vm->previous_stime   = pid_info->stime;
      vm->previous_cstime  = pid_info->cstime;
      if (not_first_time) 
        {
        vm->ram = (mem*40)/(glob_total_ram/100);
        vm->cpu = (int) 2*tot_utime;
        vm->mem_rss = mem;
        }
      else
        not_first_time = 1;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void refresh_ram_cpu_vm(int nb, t_vm *head_vm)
{
  int i;
  t_vm *cur = head_vm;
  for (i=0; i<nb; i++)
    {
    if (!cur)
      KOUT(" ");
    if (!cur->vm_to_be_killed)
      {
      if (update_pid_infos(cur))
        event_print("PROBLEM FOR %s", cur->kvm.name);
      }
    cur = cur->next;
    }
  if (cur)
    KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timeout_collect_eventfull(void *data)
{
  static int i, count = 0;
  int nb, llid, tid, nb_vm, nb_cnt;
  t_vm *vm = cfg_get_first_vm(&nb_vm);
  t_eventfull_subs *cur = head_eventfull_subs;
  int tot_evt = ovs_snf_get_qty(); 
  t_eventfull_endp *eventfull_endp;

  cnt_get_first_cnt(&nb_cnt);
  eventfull_endp =
  (t_eventfull_endp *) clownix_malloc(tot_evt * sizeof(t_eventfull_endp), 13);
  memset(eventfull_endp, 0, tot_evt * sizeof(t_eventfull_endp));

  count++;
  if (count == 10)
    {
    refresh_ram_cpu_vm(nb_vm, vm);
    count = 0;
    }
  nb = ovs_snf_collect_snf(&(eventfull_endp[0]));
  while (cur)
    {
    llid = cur->llid;
    tid = cur->tid;
    if (msg_exist_channel(llid))
      {
      for (i=0; i<nb; i++)
        {
        if (eventfull_endp[i].num == 0)
          {
          vm = cfg_get_vm(eventfull_endp[i].name);
          if (vm)
            {
            eventfull_endp[i].ram  = vm->ram;
            eventfull_endp[i].cpu  = vm->cpu;
            }
          }
        }
      send_eventfull(llid, tid, nb, eventfull_endp);
      }
    else
      event_print ("EVENTFULL ERROR!!!!!!");
    cur = cur->next;
    }
  clownix_free(eventfull_endp, __FUNCTION__);
  if ((nb_vm + nb_cnt) < 50)
    clownix_timeout_add(10, timeout_collect_eventfull, NULL, NULL, NULL);
  else if ((nb_vm + nb_cnt) < 200)
    clownix_timeout_add(20, timeout_collect_eventfull, NULL, NULL, NULL);
  else
    clownix_timeout_add(40, timeout_collect_eventfull, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_eventfull_sub(int llid, int tid)
{
  t_eventfull_subs *sub;
  sub = (t_eventfull_subs *) clownix_malloc(sizeof(t_eventfull_subs), 13);
  memset(sub, 0, sizeof(t_eventfull_subs));
  sub->llid = llid;
  sub->tid = tid;
  if (head_eventfull_subs)
    head_eventfull_subs->prev = sub;
  sub->next = head_eventfull_subs;
  head_eventfull_subs = sub;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void eventfull_llid_delete(int llid)
{
  t_eventfull_subs *next, *cur = head_eventfull_subs;
  while(cur)
    {
    next = cur->next;
    if (cur->llid == llid)
      {
      if (cur->prev)
        cur->prev->next = cur->next;
      if (cur->next)
        cur->next->prev = cur->prev;
      if (cur == head_eventfull_subs)
        head_eventfull_subs = cur->next;
      clownix_free(cur, __FUNCTION__);
      }
    cur = next;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void eventfull_init(void)
{
  head_eventfull_subs = NULL;
  clownix_timeout_add(500, timeout_collect_eventfull, NULL, NULL, NULL);
  glob_rss_shift = get_rss_shift();
  glob_total_ram = get_totalram_kb();
}
/*---------------------------------------------------------------------------*/



