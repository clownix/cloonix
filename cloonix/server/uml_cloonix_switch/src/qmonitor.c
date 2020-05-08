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
#include <string.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

#include "io_clownix.h"
#include "rpc_qmonitor.h"
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "util_sock.h"
#include "llid_trace.h"
#include "machine_create.h"
#include "utils_cmd_line_maker.h"
#include "pid_clone.h"
#include "commun_daemon.h"
#include "event_subscriber.h"
#include "qmonitor.h"
#include "qmp.h"
#include "suid_power.h"






#define MAX_QMON_LEN 5000



/*****************************************************************************/
typedef struct t_qmonitor_sub
{
  int llid_sub;
  struct t_qmonitor_sub *prev;
  struct t_qmonitor_sub *next;
} t_qmonitor_sub;
/*--------------------------------------------------------------------------*/
typedef struct t_qmonitor_vm
{
  char name[MAX_NAME_LEN];
  int vm_qmonitor_llid;
  int vm_qmonitor_llid_quit_sent;
  int vm_qmonitor_fd;
  int connect_count;
  long long connect_abs_beat_timer;
  int connect_ref_timer;
  long long delete_abs_beat_timer;
  int delete_ref_timer;
  long long stop_abs_beat_timer;
  int stop_ref_timer;
  int pid;
  t_qmonitor_sub *sub_head;
  struct t_qmonitor_vm *prev;
  struct t_qmonitor_vm *next;
} t_qmonitor_vm;
/*--------------------------------------------------------------------------*/


static void timer_qvm_connect_qmonitor(void *data);
static t_qmonitor_vm *vm_get_with_name(char *name);
static void vm_release(t_qmonitor_vm *qvm);

static t_qmonitor_vm *head_qvm;
static int nb_qmonitor;

/*****************************************************************************/
static t_qmonitor_sub *findsub(t_qmonitor_vm *qvm, int llid)
{
  t_qmonitor_sub *cur;
  if (!qvm)
    KOUT(" ");
  cur = qvm->sub_head;
  while (cur && (cur->llid_sub != llid))
    cur = cur->next;
  return cur;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void addsub(t_qmonitor_vm *qvm, int llid)
{
  t_qmonitor_sub *cur;
  if (!findsub(qvm, llid))
    {
    cur = (t_qmonitor_sub *) clownix_malloc(sizeof(t_qmonitor_sub), 6); 
    memset(cur, 0, sizeof(t_qmonitor_sub));
    cur->llid_sub = llid;
    if (qvm->sub_head)
      qvm->sub_head->prev = cur;
    cur->next = qvm->sub_head;
    qvm->sub_head = cur;
    llid_set_qmonitor(llid, qvm->name, 1);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void delsub(t_qmonitor_vm *qvm, int llid, int tell_llid_watch)
{
  t_qmonitor_sub *cur = findsub(qvm, llid);
  if (cur)
    {
    if (cur->prev)
      cur->prev->next = cur->next;
    if (cur->next)
      cur->next->prev = cur->prev;
    if (cur == qvm->sub_head)
      qvm->sub_head = cur->next;
    llid_set_qmonitor(llid, qvm->name, 0);
    if (tell_llid_watch)
      llid_trace_free(llid, 0, __FUNCTION__);
    clownix_free(cur, __FUNCTION__);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void vm_release(t_qmonitor_vm *qvm)
{
  t_qmonitor_sub *cur, *next;
  if (!qvm)
    KOUT(" ");
  cur = qvm->sub_head;
  if (qvm->vm_qmonitor_llid)
    llid_trace_free(qvm->vm_qmonitor_llid, 0, __FUNCTION__);
  while (cur)
    {
    next = cur->next;
    delsub(qvm, cur->llid_sub, 1);
    cur = next;
    }
  if (qvm->connect_abs_beat_timer)
    clownix_timeout_del(qvm->connect_abs_beat_timer, qvm->connect_ref_timer,
                        __FILE__, __LINE__);
  qvm->connect_abs_beat_timer = 0;
  qvm->connect_ref_timer = 0;
  if (qvm->delete_abs_beat_timer)
    clownix_timeout_del(qvm->delete_abs_beat_timer, qvm->delete_ref_timer,
                        __FILE__, __LINE__);
  qvm->delete_abs_beat_timer = 0;
  qvm->delete_ref_timer = 0;
  if (qvm->stop_abs_beat_timer)
    clownix_timeout_del(qvm->stop_abs_beat_timer, qvm->stop_ref_timer,
                        __FILE__, __LINE__);
  qvm->stop_abs_beat_timer = 0;
  qvm->stop_ref_timer = 0;
  if (qvm->prev)
    qvm->prev->next = qvm->next;
  if (qvm->next)
    qvm->next->prev = qvm->prev;
  if (qvm == head_qvm)
    head_qvm = qvm->next;
  if (nb_qmonitor == 0)
    KOUT(" ");
  nb_qmonitor--;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_qmonitor_vm *vm_alloc(char *name)
{
  t_qmonitor_vm *qvm = NULL;
  qvm = (t_qmonitor_vm *) clownix_malloc(sizeof(t_qmonitor_vm), 5);
  memset(qvm, 0, sizeof(t_qmonitor_vm));
  strncpy(qvm->name, name, MAX_NAME_LEN-1);
  if (head_qvm)
    head_qvm->prev = qvm;
  qvm->next = head_qvm;
  head_qvm = qvm;
  nb_qmonitor++;
  return qvm;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_qmonitor_vm *vm_get_with_name(char *name)
{
  t_qmonitor_vm *qvm = head_qvm;
  while (qvm && (strcmp(qvm->name, name)))
    qvm = qvm->next;
  return qvm;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_qmonitor_vm *vm_get_with_llid(int llid)
{
  t_qmonitor_vm *qvm = head_qvm;
  while (qvm && (qvm->vm_qmonitor_llid != llid))
    qvm = qvm->next;
  return qvm;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_too_soon_qmonitor(void *data)
{
  unsigned long val = (unsigned long) data;
  int llid = (int) val;
  llid_trace_free(llid, 0, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_sub2qmonitor(int llid, int tid, char *name, int on_off)
{
  unsigned long ul = (unsigned long) llid;
  t_qmonitor_vm *qvm = vm_get_with_name(name);
  if (qvm)
    {
    if (on_off)
      {
      if (qvm->vm_qmonitor_llid) 
        addsub(qvm, llid);
      else
        {
        send_qmonitor(llid, 0, name, "NOT YET READY1");
        clownix_timeout_add(2,timer_too_soon_qmonitor,(void *)ul,NULL,NULL);
        }
      }
    else
      delsub(qvm, llid, 1);
    }
  else
    {
    send_qmonitor(llid, 0, name, "NOT YET READY2");
    clownix_timeout_add(2,timer_too_soon_qmonitor,(void *)ul,NULL,NULL);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int working_vm_qmonitor_llid(t_qmonitor_vm *qvm)
{
  int result = 0;
  if (qvm->vm_qmonitor_llid)
    {
    if (msg_exist_channel(qvm->vm_qmonitor_llid))
      result = 1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_qmonitor(int llid, int tid, char *name, char *data)
{
  t_qmonitor_vm *qvm = vm_get_with_name(name);
  if (qvm)
    {
    if ((findsub(qvm, llid)) && (working_vm_qmonitor_llid(qvm)))
      watch_tx(qvm->vm_qmonitor_llid, strlen(data), data);
    }
  else
    KERR(" %s", name);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void process_llid_error( t_qmonitor_vm *qvm)
{
  if (qvm->vm_qmonitor_llid)
    llid_trace_free(qvm->vm_qmonitor_llid, 0, __FUNCTION__);
  qvm->vm_qmonitor_llid = 0;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void vm_err_cb (void *ptr, int llid, int err, int from)
{
  t_qmonitor_vm *qvm;
  qvm = vm_get_with_llid(llid);
  if (qvm)
    process_llid_error(qvm);
  else
    KERR(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int vm_rx_cb(void *ptr, int llid, int fd)
{
  int len;
  t_qmonitor_vm *qvm;
  t_qmonitor_sub *cur;
  static char buf[MAX_QMON_LEN];
  memset(buf, 0, MAX_QMON_LEN);
  len = util_read(buf, MAX_QMON_LEN, fd);
  qvm = vm_get_with_llid(llid);
  if (!qvm)
    KERR(" ");
  else
    {
    if ((len < 0) || (len > MAX_QMON_LEN))
      {
      KERR("%s ",qvm->name );
      process_llid_error(qvm);
      len = 0;
      }
    else
      {
      cur = qvm->sub_head;
      while(cur)
        {
        send_qmonitor(cur->llid_sub, 0, qvm->name, buf);
        cur = cur->next;
        }
      }
    }
  return len;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void rearm_timer_qvm_connect_qmonitor(t_qmonitor_vm *qvm)
{
  qvm->connect_count += 1;
  if (qvm->connect_count < 50)
    clownix_timeout_add(10, timer_qvm_connect_qmonitor, (void *) qvm,
                        &(qvm->connect_abs_beat_timer),
                        &(qvm->connect_ref_timer));
  else if (qvm->connect_count < 100)
    clownix_timeout_add(200, timer_qvm_connect_qmonitor, (void *) qvm,
                        &(qvm->connect_abs_beat_timer),
                        &(qvm->connect_ref_timer));
  else if (qvm->connect_count < 200)
    clownix_timeout_add(500, timer_qvm_connect_qmonitor, (void *) qvm,
                        &(qvm->connect_abs_beat_timer),
                        &(qvm->connect_ref_timer));
  else
    {
    KERR(" %s", qvm->name);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_qvm_connect_qmonitor(void *data)
{ 
  t_qmonitor_vm *qvm = (t_qmonitor_vm *) data;
  char *qmon;
  int fd, llid;
  t_vm *vm;
  if ((!qvm) || (!qvm->name))
    KOUT(" ");
  qvm->connect_abs_beat_timer = 0;
  qvm->connect_ref_timer = 0;
  vm = cfg_get_vm(qvm->name);
  if (vm)
    { 
    if (qvm->vm_qmonitor_llid)
      {
      qvm->pid = suid_power_get_pid(vm->kvm.vm_id);
      if (!qvm->pid)
        rearm_timer_qvm_connect_qmonitor(qvm);
      }
    else
      {
      qmon = utils_get_qmonitor_path(vm->kvm.vm_id);
      if (!util_nonblock_client_socket_unix(qmon, &fd))
        {
        if (fd <= 0)
          KOUT(" ");
        qvm->vm_qmonitor_fd = fd;
        llid=msg_watch_fd(qvm->vm_qmonitor_fd, vm_rx_cb, vm_err_cb, "qmon");
        if (llid == 0)
          KOUT(" ");
        llid_trace_alloc(llid,"QMONITOR",0,0, type_llid_trace_unix_qmonitor);
        qvm->vm_qmonitor_llid = llid;
        }
      rearm_timer_qvm_connect_qmonitor(qvm);
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void qmonitor_llid_free(char *name, int llid)
{
  t_qmonitor_vm *qvm = vm_get_with_name(name);
  if (qvm)
    delsub(qvm, llid, 0);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void qmonitor_begin_qemu_unix(char *name)
{
  t_qmonitor_vm *qvm = vm_alloc(name);
  clownix_timeout_add(2, timer_qvm_connect_qmonitor, (void *) qvm,
                      &(qvm->connect_abs_beat_timer),
                      &(qvm->connect_ref_timer));
}
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
void qmonitor_end_qemu_unix(char *name)
{
  t_qmonitor_vm *qvm = vm_get_with_name(name);
  if (qvm)
    vm_release(qvm);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int qmonitor_still_present(void)
{
  return nb_qmonitor;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void qmonitor_send_string_to_cli(char *name, char *data)
{
  t_qmonitor_vm *qvm = vm_get_with_name(name);
  if (qvm)
    {
    if (working_vm_qmonitor_llid(qvm))
      watch_tx(qvm->vm_qmonitor_llid, strlen(data), data);
    }
  else
    KERR(" %s", name);
}
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
void init_qmonitor(void)
{
  head_qvm = NULL;
  nb_qmonitor = 0;
}
/*--------------------------------------------------------------------------*/

