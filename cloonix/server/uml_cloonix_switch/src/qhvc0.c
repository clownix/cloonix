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
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "util_sock.h"
#include "llid_trace.h"
#include "machine_create.h"
#include "utils_cmd_line_maker.h"
#include "pid_clone.h"
#include "commun_daemon.h"
#include "event_subscriber.h"
#include "qhvc0.h"
#include "doorways_mngt.h"
#include "doors_rpc.h"
#include "header_sock.h"
#include "suid_power.h"





#define END_HVCO_CMD_MARKER "end_of_hvc0_cloonix_cmd_marker"
#define MAX_LEN_DROPBEAR 3000
#define CMD_START_DROPBEAR_CLOONIX " "\
  "cat > /tmp/dropbear_cloonix_agent.sh << \"INSIDE_EOF\"\n"\
  "#!/bin/sh\n"\
  "set +e\n"\
  "echo try >> /tmp/try_dropbear_cloonix_agent\n"\
  "if [ ! -d /dev/virtio-ports ]; then\n"\
    "mkdir -p /dev/virtio-ports\n"\
    "cd  /dev/virtio-ports\n"\
    "ln -s ../vport0p1 net.cloonix.0\n"\
    "ln -s ../vport0p2 net.cloonix.1\n"\
    "ln -s ../vport0p3 com.redhat.spice.0\n"\
    "cd -\n"\
  "fi\n"\
  "CONFIG=/mnt/cloonix_config_fs\n"\
  "AGD=sr0\n"\
  "if [ \"$(uname -m)\" = \"x86_64\" ]; then\n"\
    "AGT=cloonix_agent\n"\
    "DRP=dropbear_cloonix_sshd\n"\
  "else\n"\
    "AGT=cloonix_agent_i386\n"\
    "DRP=dropbear_cloonix_sshd_i386\n"\
  "fi\n"\
  "APID=\"$(pidof $AGT )\"\n"\
  "DPID=\"$(pidof $DRP )\"\n"\
  "if [ \"$APID\" != \"\" ]; then\n"\
  "  kill $APID\n"\
  "fi\n"\
  "if [ \"$DPID\" != \"\" ]; then\n"\
  "  kill $DPID\n"\
  "fi\n"\
  "mkdir -p /mnt/cloonix_config_fs\n"\
  "umount /dev/${AGD}\n"\
  "umount /dev/${AGD}\n"\
  "mount /dev/${AGD} /mnt/cloonix_config_fs\n"\
  "mount -o remount,exec /dev/${AGD}\n"\
  "${CONFIG}/$AGT\n"\
  "${CONFIG}/$DRP\n"\
  "APID=\"$(pidof $AGT )\"\n"\
  "DPID=\"$(pidof $DRP )\"\n"\
  "if [ \"$DPID\" != \"\" ]; then\n"\
  "  if [ \"$APID\" != \"\" ]; then\n"\
  "    echo i_think_cloonix_agent_is_up\n"\
  "  fi\n"\
  "fi\n"\
  "p9_host_share=$(cat ${CONFIG}/cloonix_vm_p9_host_share)\n"\
  "if [ \"${p9_host_share}\" = \"yes\" ]; then\n"\
  "  name=$(cat ${CONFIG}/cloonix_vm_name)\n"\
  "  SHRED=/mnt/p9_host_share\n"\
  "  mkdir -p ${SHRED}\n"\
  "  mount -t 9p -o trans=virtio,version=9p2000.L $name ${SHRED}\n"\
  "fi\n"\
  "WLAN_NB=%d \n"\
  "if [ \"$WLAN_NB\" != \"0\" ]; then \n"\
    "modprobe mac80211_hwsim radios=$WLAN_NB\n"\
    "if [ \"$(uname -m)\" = \"x86_64\" ]; then\n"\
      "export LD_LIBRARY_PATH=/mnt/cloonix_config_fs/lib_x86_64\n"\
      "${CONFIG}/cloonix_hwsim $WLAN_NB\n"\
    "else\n"\
      "export LD_LIBRARY_PATH=/mnt/cloonix_config_fs/lib_i386\n"\
      "${CONFIG}/cloonix_hwsim_i386 $WLAN_NB\n"\
    "fi\n"\
  "fi\n"\
  "INSIDE_EOF\n"\
  "chmod +x /tmp/dropbear_cloonix_agent.sh\n"\
  "/tmp/dropbear_cloonix_agent.sh\n"

#define MAX_QHVC_LEN 50000
#define MAX_TOT_RX_LEN 150000


enum {
  state_min = 0,
  state_waiting_resp_first_try,
  state_waiting_after_first_try_success,
  state_waiting_resp_launch_agent,
  state_vport_is_cloonix_backdoor,
  state_failure,
  state_max,
};

typedef struct t_rx_pktbuf
{
  char buf[MAX_A2D_LEN];
  int  offset;
  int  paylen;
  char *payload;
} t_rx_pktbuf;



/*--------------------------------------------------------------------------*/
typedef struct t_qhvc0_vm
{
  char name[MAX_NAME_LEN];
  int vm_config_flags;
  int vm_id;
  int nb_wlan;
  int vm_qhvc0_llid;
  int vm_qhvc0_fd;
  long long heartbeat_abeat;
  int heartbeat_ref;
  long long connect_abs_beat_timer;
  int connect_ref_timer;
  int pid;
  char tot_rx[MAX_TOT_RX_LEN];
  char tot_rx_cmd[MAX_QHVC_LEN];
  int auto_state;
  int tot_rx_offset;
  int backdoor_connected;
  int ready_to_connect_hvc0;
  int not_first_time_read;

  int timeout_cloonix_agent_handshake;
  int in_guest_ls_done;
  int in_guest_cloonix_agent_start_done;

  t_rx_pktbuf rx_pktbuf_fag;
  t_rx_pktbuf rx_pktbuf_tag;

  struct t_qhvc0_vm *prev;
  struct t_qhvc0_vm *next;
} t_qhvc0_vm;
/*--------------------------------------------------------------------------*/

static void ga_qhvc0_heartbeat_init(t_qhvc0_vm *cvm);
static void clean_connect_timer(t_qhvc0_vm *cvm);
static void clean_heartbeat_timer(t_qhvc0_vm *cvm);
static void timer_cvm_connect_qhvc0(void *data);
static void change_to_state(t_qhvc0_vm *cvm, int state);

static t_qhvc0_vm *head_cvm;
static int nb_qhvc0;

/*****************************************************************************/
static int working_vm_qhvc0_llid(t_qhvc0_vm *cvm)
{
  int result = 0;
  if (cvm->vm_qhvc0_llid)
    {
    if (msg_exist_channel(cvm->vm_qhvc0_llid))
      result = 1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void protected_tx(t_qhvc0_vm *cvm, int len, char *buf)
{
  if (working_vm_qhvc0_llid(cvm))
    {
    watch_tx(cvm->vm_qhvc0_llid, len, buf);
    }
  else
    KERR("len:%d, Buf:%s", len, buf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void entry_in_state_automaton_1(t_qhvc0_vm *cvm)
{
  cvm->in_guest_ls_done = 0;
  change_to_state(cvm, state_waiting_resp_first_try);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_qhvc0_vm *vm_get_with_name(char *name)
{
  t_qhvc0_vm *cvm = head_cvm;
  while (cvm && (strcmp(cvm->name, name)))
    cvm = cvm->next;
  return cvm;
}
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
static void timer_entry_in_state_automaton_2(void *data)
{
  char *name = (char *) data;
  t_qhvc0_vm *cvm = vm_get_with_name(name);
  if (cvm)
    {
    if (cvm->auto_state != state_waiting_after_first_try_success)
      KERR("%d", cvm->auto_state);
    else
      {
      cvm->in_guest_cloonix_agent_start_done = 0;
      change_to_state(cvm, state_waiting_resp_launch_agent);
      }
    }
  clownix_free(name, __FUNCTION__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void arm_timer_entry_in_state_automaton_2(t_qhvc0_vm *cvm)
{
  char *name = (char *) clownix_malloc(MAX_NAME_LEN, 5);
  memset(name, 0, MAX_NAME_LEN);
  strncpy(name, cvm->name, MAX_NAME_LEN - 1);
  change_to_state(cvm, state_waiting_after_first_try_success);
  clownix_timeout_add(10, timer_entry_in_state_automaton_2, 
                      (void *) name, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void change_to_state(t_qhvc0_vm *cvm, int state)
{
//  KERR("%d -----> %d", cvm->auto_state, state);
  cvm->auto_state = state;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_qhvc0_vm *vm_get_with_llid(int llid)
{
  t_qhvc0_vm *cvm = head_cvm;
  while (cvm && (cvm->vm_qhvc0_llid != llid))
    cvm = cvm->next;
  return cvm;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void init_rx_buf(t_qhvc0_vm *cvm)
{
  memset(cvm->tot_rx, 0, MAX_TOT_RX_LEN);
  memset(cvm->tot_rx_cmd, 0, MAX_QHVC_LEN);
  cvm->tot_rx_offset = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void begin_and_send(t_qhvc0_vm *cvm, char *cmd)
{
  init_rx_buf(cvm);
  memset(cvm->tot_rx, ' ', strlen(END_HVCO_CMD_MARKER));
  cvm->tot_rx_offset = strlen(cvm->tot_rx);
  snprintf(cvm->tot_rx_cmd, MAX_QHVC_LEN-1, "%s\r\necho %s\r\n", 
           cmd, END_HVCO_CMD_MARKER);
  protected_tx(cvm, strlen(cvm->tot_rx_cmd), cvm->tot_rx_cmd);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int pattern_found_in_rx(t_qhvc0_vm *cvm, char  *buf, 
                               char *pattern1, char *pattern2)
{
  int len_left, result = 0;
  char *ptr;
  len_left = MAX_TOT_RX_LEN - cvm->tot_rx_offset;
  ptr = cvm->tot_rx + cvm->tot_rx_offset;
  cvm->tot_rx_offset += snprintf(ptr, len_left-1, "%s", buf);
  if (strstr(cvm->tot_rx, pattern1))
    {
    if (!pattern2)
      {
      result = 1;
      init_rx_buf(cvm);
      }
    else
      {
      if (strstr(cvm->tot_rx, pattern2))
        {
        result = 1;
        init_rx_buf(cvm);
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void qhvc0_rx(t_qhvc0_vm *cvm, int len, char  *buf)
{
  if (cvm->auto_state == state_waiting_resp_first_try)
    {
    if (pattern_found_in_rx(cvm, buf, END_HVCO_CMD_MARKER, "sbin"))
      arm_timer_entry_in_state_automaton_2(cvm);
    }
  else if (cvm->auto_state == state_waiting_resp_launch_agent)
    {
    if (pattern_found_in_rx(cvm, buf, END_HVCO_CMD_MARKER, 
                            "i_think_cloonix_agent_is_up"))
      {
      change_to_state(cvm, state_vport_is_cloonix_backdoor);
      doors_send_command(get_doorways_llid(),0,cvm->name,
                         CLOONIX_UP_VPORT_AND_RUNNING);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void clean_connect_timer(t_qhvc0_vm *cvm)
{
  if (cvm->connect_abs_beat_timer)
    clownix_timeout_del(cvm->connect_abs_beat_timer, cvm->connect_ref_timer,
                        __FILE__, __LINE__);
  cvm->connect_abs_beat_timer = 0;
  cvm->connect_ref_timer = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void clean_heartbeat_timer(t_qhvc0_vm *cvm)
{
  if (cvm->heartbeat_abeat)
    clownix_timeout_del(cvm->heartbeat_abeat, cvm->heartbeat_ref,
                        __FILE__, __LINE__);
  cvm->heartbeat_abeat = 0;
  cvm->heartbeat_ref = 0;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_hvc0_tx(void *data)
{ 
  char *name = (char *) data;
  t_qhvc0_vm *cvm = vm_get_with_name(name);
  if (cvm)
    {
    if (cvm->not_first_time_read == 0)
      KERR("%s may have a bad hvc0 configuration (exists?)", name);
    }
  clownix_free(name, __FUNCTION__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void arm_timer_hvc0_tx(t_qhvc0_vm *cvm, int val)
{
  char *nm = (char *) clownix_malloc(MAX_NAME_LEN, 13);
  memset(nm, 0, MAX_NAME_LEN);
  strncpy(nm, cvm->name, MAX_NAME_LEN - 1);
  clownix_timeout_add(val, timer_hvc0_tx, (void *) nm, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void vm_release(t_qhvc0_vm *cvm)
{
  if (!cvm)
    KOUT(" ");
  clean_connect_timer(cvm);
  clean_heartbeat_timer(cvm);
  if (cvm->vm_qhvc0_llid)
    llid_trace_free(cvm->vm_qhvc0_llid, 0, __FUNCTION__);
  if (cvm->prev)
    cvm->prev->next = cvm->next;
  if (cvm->next)
    cvm->next->prev = cvm->prev;
  if (cvm == head_cvm)
    head_cvm = cvm->next;
  if (nb_qhvc0 == 0)
    KOUT(" ");
  clownix_free(cvm, __FUNCTION__);
  nb_qhvc0--;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_qhvc0_vm *vm_alloc(char *name, t_vm *vm)
{
  int i;
  t_qhvc0_vm *cvm = NULL;
  cvm = (t_qhvc0_vm *) clownix_malloc(sizeof(t_qhvc0_vm), 5);
  memset(cvm, 0, sizeof(t_qhvc0_vm));
  strncpy(cvm->name, name, MAX_NAME_LEN-1);
  cvm->vm_id = vm->kvm.vm_id;
  cvm->vm_config_flags = vm->kvm.vm_config_flags;
  for (i = 0; i < vm->kvm.nb_tot_eth; i++)
    {
    if (vm->kvm.eth_table[i].eth_type == eth_type_wlan)
      cvm->nb_wlan += 1;
    }
  if (head_cvm)
    head_cvm->prev = cvm;
  cvm->next = head_cvm;
  head_cvm = cvm;
  nb_qhvc0++;
  return cvm;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void err_llid_retry(int llid, t_qhvc0_vm *cvm)
{
  if (!cvm->vm_qhvc0_llid)
    KOUT(" ");
  if (llid != cvm->vm_qhvc0_llid)
    KOUT(" ");
  llid_trace_free(cvm->vm_qhvc0_llid, 0, __FUNCTION__);
  cvm->vm_qhvc0_llid = 0;
  clean_connect_timer(cvm);
  clean_heartbeat_timer(cvm);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void vm_err_cb (void *ptr, int llid, int err, int from)
{
  t_qhvc0_vm *cvm;
  cvm = vm_get_with_llid(llid);
  if (cvm)
    {
    err_llid_retry(llid, cvm);
    }
  else
    KERR(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int vm_rx_cb(void *ptr, int llid, int fd)
{
  int len;
  t_qhvc0_vm *cvm;
  t_vm *vm;
  static char buf[MAX_QHVC_LEN];
  memset(buf, 0, MAX_QHVC_LEN);
  len = util_read(buf, MAX_QHVC_LEN, fd);
  cvm = vm_get_with_llid(llid);
  if (!cvm)
    KERR(" ");
  else
    {
    if (len < 0)
      {
      vm = cfg_get_vm(cvm->name);
      if (vm && (!vm->vm_to_be_killed))
        KERR("%s", cvm->name);
      err_llid_retry(llid, cvm);
      }
    else
      {
      if (cvm->not_first_time_read == 0)
        {
        cvm->not_first_time_read = 1;
        ga_qhvc0_heartbeat_init(cvm);
        }
      if (len > 0)
        {
        qhvc0_rx(cvm, len, buf);
        }
      }
    }
  return len;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void scheduler_heartbeat(t_qhvc0_vm *cvm)
{
  char script_start_dropbear[MAX_LEN_DROPBEAR]; 
  if (cvm->auto_state == state_waiting_resp_first_try)
    {
    if ((cvm->in_guest_ls_done == 0) || (cvm->in_guest_ls_done > 10))
      {
      cvm->in_guest_ls_done = 1;
      begin_and_send(cvm, "ls /"); 
      }
    cvm->in_guest_ls_done += 1;
    }
  else if (cvm->auto_state == state_waiting_resp_launch_agent)
    {
    if ((cvm->in_guest_cloonix_agent_start_done == 0) || 
        (cvm->in_guest_cloonix_agent_start_done > 20))
      {
      cvm->in_guest_cloonix_agent_start_done = 1;
      snprintf(script_start_dropbear, MAX_LEN_DROPBEAR, 
               CMD_START_DROPBEAR_CLOONIX, cvm->nb_wlan); 
      begin_and_send(cvm, script_start_dropbear);
      }
    cvm->in_guest_cloonix_agent_start_done++;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_ga_heartbeat(void *data)
{
  t_qhvc0_vm *cvm = (t_qhvc0_vm *) data;
  if (!cvm)
    KOUT(" ");
  cvm->heartbeat_abeat = 0;
  cvm->heartbeat_ref = 0;
  if (working_vm_qhvc0_llid(cvm))
    {
    scheduler_heartbeat(cvm);
    clownix_timeout_add(10, timer_ga_heartbeat, (void *) cvm,
                        &(cvm->heartbeat_abeat), &(cvm->heartbeat_ref));
    }
  else
    {
    change_to_state(cvm, state_failure);
    KERR("%s %s", cvm->name, "BAD SOCK TO HVC0");
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void ga_qhvc0_heartbeat_init(t_qhvc0_vm *cvm)
{
  if ((!cvm) || (!cvm->name))
    KOUT(" ");
  if (working_vm_qhvc0_llid(cvm))
    {
    clean_heartbeat_timer(cvm);
    clownix_timeout_add(10, timer_ga_heartbeat, (void *) cvm,
                        &(cvm->heartbeat_abeat), &(cvm->heartbeat_ref));
    entry_in_state_automaton_1(cvm);
    }
  else
    KERR("%s", cvm->name);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_cvm_connect_qhvc0(void *data)
{ 
  t_qhvc0_vm *cvm = (t_qhvc0_vm *) data;
  char *qmon;
  int fd, llid;
  t_vm *vm;
  if ((!cvm) || (!cvm->name))
    KOUT(" ");
  cvm->connect_abs_beat_timer = 0;
  cvm->connect_ref_timer = 0;
  vm = cfg_get_vm(cvm->name);
  if (vm)
    {
    cvm->pid = suid_power_get_pid(vm->kvm.vm_id);
    if (!cvm->pid)
      {
      clownix_timeout_add(30, timer_cvm_connect_qhvc0, (void *) cvm,
                          &(cvm->connect_abs_beat_timer),
                          &(cvm->connect_ref_timer));
      }
    else if (!cvm->ready_to_connect_hvc0)
      {
      clownix_timeout_add(30, timer_cvm_connect_qhvc0, (void *) cvm,
                          &(cvm->connect_abs_beat_timer),
                          &(cvm->connect_ref_timer));
      cvm->ready_to_connect_hvc0 = 1;
      }
    else
      {
      if (!(cvm->vm_qhvc0_llid))
        { 
        qmon = utils_get_qhvc0_path(vm->kvm.vm_id);
        if (!util_nonblock_client_socket_unix(qmon, &fd))
          {
          if (fd <= 0)
            KOUT(" ");
          cvm->vm_qhvc0_fd = fd;
          llid=msg_watch_fd(cvm->vm_qhvc0_fd, vm_rx_cb, vm_err_cb, "cloon");
          if (llid == 0)
            KOUT(" ");
          llid_trace_alloc(llid,"CLOON",0,0, type_llid_trace_unix_qmonitor);
          cvm->vm_qhvc0_llid = llid;
          arm_timer_hvc0_tx(cvm, 36000);
          protected_tx(cvm, strlen("\r\n"), "\r\n");
          }
        else
          {
          clownix_timeout_add(30, timer_cvm_connect_qhvc0, (void *) cvm,
                              &(cvm->connect_abs_beat_timer),
                              &(cvm->connect_ref_timer));
          }
        }
      else
        KERR("%s", cvm->name);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void flag_ping_to_cloonix_agent_ko(char *name)
{
  t_small_evt vm_evt;
  t_vm *vm;
  vm = cfg_get_vm(name);
  if ((vm) && (vm->kvm.vm_config_flags & VM_FLAG_CLOONIX_AGENT_PING_OK))
    {
    vm->kvm.vm_config_flags &= ~VM_FLAG_CLOONIX_AGENT_PING_OK;
    memset(&vm_evt, 0, sizeof(vm_evt));
    strncpy(vm_evt.name, name, MAX_NAME_LEN-1);
    vm_evt.evt = vm_evt_cloonix_ga_ping_ko;
    event_subscriber_send(topo_small_event, (void *) &vm_evt);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void flag_ping_to_cloonix_agent_ok(char *name)
{
  t_small_evt vm_evt;
  t_vm *vm;
  vm = cfg_get_vm(name);
  if ((vm) && 
      (!(vm->kvm.vm_config_flags & VM_FLAG_CLOONIX_AGENT_PING_OK)))
    {
    vm->kvm.vm_config_flags |= VM_FLAG_CLOONIX_AGENT_PING_OK;
    memset(&vm_evt, 0, sizeof(t_small_evt));
    strncpy(vm_evt.name, name, MAX_NAME_LEN-1);
    vm_evt.evt = vm_evt_cloonix_ga_ping_ok;
    event_subscriber_send(topo_small_event, (void *) &vm_evt);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void qhvc0_event_backdoor(char *name, int backdoor_evt)
{
  t_qhvc0_vm *cvm = vm_get_with_name(name);
  if (cvm)
    {
    if  (backdoor_evt == backdoor_evt_connected)
      {
      cvm->backdoor_connected = 1;
      }
    else if  (backdoor_evt == backdoor_evt_disconnected)
      {
      cvm->backdoor_connected = 0;
      }
    else if  (backdoor_evt == backdoor_evt_ping_ok)
      {
      flag_ping_to_cloonix_agent_ok(name);
      }
    else if  (backdoor_evt == backdoor_evt_ping_ko)
      {
      flag_ping_to_cloonix_agent_ko(name);
      if ((cvm->auto_state != state_waiting_resp_launch_agent)  &&
          (cvm->auto_state != state_waiting_resp_first_try)   &&
          (cvm->auto_state != state_waiting_after_first_try_success))
        {
        if (!cvm->timeout_cloonix_agent_handshake)
          {
          KERR("%s PING KO RESTART PROCEDURE", name);
          doors_send_command(get_doorways_llid(),0,cvm->name,
                             CLOONIX_DOWN_AND_NOT_RUNNING);
          entry_in_state_automaton_1(cvm);
          }
        }
      }
    else
      KOUT("%d", backdoor_evt);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void qhvc0_reinit_vm_in_doorways(void)
{
  int i, nb;
  t_vm *vm = cfg_get_first_vm(&nb);
  t_qhvc0_vm *cvm;
  for (i=0; i<nb; i++)
    {
    if (!vm)
      KOUT(" ");
    cvm = vm_get_with_name(vm->kvm.name);
    if (cvm)
      {
      doors_send_add_vm(get_doorways_llid(), 0, vm->kvm.name,
                        utils_get_qbackdoor_path(vm->kvm.vm_id));
      if (cvm->auto_state == state_vport_is_cloonix_backdoor)
        {
        doors_send_command(get_doorways_llid(),0,cvm->name,
                           CLOONIX_UP_VPORT_AND_RUNNING);
        KERR("%s", vm->kvm.name);
        }
      else
        entry_in_state_automaton_1(cvm);
      }
    vm =  vm->next;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void qhvc0_begin_qemu_unix(char *name)
{
  t_vm *vm;
  t_qhvc0_vm *cvm = vm_get_with_name(name);
  vm = cfg_get_vm(name);
  if (vm && !cvm)
    {
    if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_FULL_VIRT)
      KERR("Full virt, no hvc0 %s", name);
    else
      {
      cvm = vm_alloc(name, vm);
      clownix_timeout_add(10, timer_cvm_connect_qhvc0, (void *) cvm,
                          &(cvm->connect_abs_beat_timer),
                          &(cvm->connect_ref_timer));
      }
    }
  else if (cvm)
    KERR("%s %d", cvm->name, cvm->auto_state);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void qhvc0_end_qemu_unix(char *name)
{
  t_qhvc0_vm *cvm = vm_get_with_name(name);
  doors_send_command(get_doorways_llid(),0,name,CLOONIX_DOWN_AND_NOT_RUNNING);
  if (cvm)
    vm_release(cvm);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int qhvc0_still_present(void)
{
  return nb_qhvc0;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void init_qhvc0(void)
{
  head_cvm = NULL;
  nb_qhvc0 = 0;
}
/*--------------------------------------------------------------------------*/

