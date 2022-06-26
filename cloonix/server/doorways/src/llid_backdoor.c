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
#include "doors_rpc.h"
#include "sock.h"
#include "llid_traffic.h"
#include "llid_x11.h"
#include "llid_backdoor.h"
#include "stats.h"
#include "dispach.h"


int get_doorways_llid(void);

enum
{
  ping_no_state = 0,
  ping_ok,
  ping_ko,
};

typedef struct t_llid_traf
{
  int dido_llid;
  struct t_llid_traf *prev;
  struct t_llid_traf *next;
} t_llid_traf;


typedef struct t_rx_pktbuf
{
  char buf[MAX_A2D_LEN];
  int  offset;
  int  paylen;
  int  dido_llid;
  int  type;
  int  val;
  char *payload;
} t_rx_pktbuf;

/*--------------------------------------------------------------------------*/
typedef struct t_backdoor_vm
{
  char name[MAX_NAME_LEN];
  char backdoor[MAX_PATH_LEN];
  int backdoor_llid;
  int backdoor_fd;
  int connect_count;
  long long heartbeat_abeat;
  int heartbeat_ref;
  long long connect_abs_beat_timer;
  int connect_ref_timer;
  t_llid_traf *llid_traf;
  t_rx_pktbuf rx_pktbuf;
  int ping_agent_rx;
  int last_ping_agent_rx;
  int fail_ping_agent_rx;
  int ping_status;
  int ping_id;
  int reboot_id;
  int halt_id;
  int cloonix_up_and_running;
  int cloonix_not_yet_connected;
  struct t_backdoor_vm *prev;
  struct t_backdoor_vm *next;
} t_backdoor_vm;
/*--------------------------------------------------------------------------*/


static void action_bvm_connect_backdoor(t_backdoor_vm *bvm);
static void timer_bvm_connect_backdoor(void *data);
static t_backdoor_vm *g_llid_backdoor[CLOWNIX_MAX_CHANNELS];
static t_backdoor_vm *head_bvm;
static int nb_backdoor;


/****************************************************************************/
void llid_backdoor_low_tx(int llid, int len, char *buf)
{
  t_backdoor_vm *bvm = g_llid_backdoor[llid];
  if (bvm)
    {
    if (msg_exist_channel(llid))
      watch_tx(llid, len, buf);
    }
  else
    KERR(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_backdoor_vm *vm_get_with_name(char *name)
{
  t_backdoor_vm *bvm = head_bvm;
  while (bvm && (strcmp(bvm->name, name)))
    bvm = bvm->next;
  return bvm;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_backdoor_vm *vm_get_with_backdoor_llid(int llid)
{
  t_backdoor_vm *bvm = g_llid_backdoor[llid];
  return bvm;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_backdoor_vm *check_llid_name(int llid, char *name, const char *fct)
{
  t_backdoor_vm *bvm = vm_get_with_backdoor_llid(llid);
  if (bvm)
    {
    if (bvm->backdoor_llid != llid)
      KOUT(" ");
    if (strcmp(bvm->name, name))
      KOUT("%s %s %s", bvm->name, name, fct);
    }
  return bvm;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void local_backdoor_tx(t_backdoor_vm *bvm, int dido_llid,
                              int len, int type, int val, char  *buf)
{
  char *payload;
  int llid, headsize = sock_header_get_size();
  if (len > MAX_A2D_LEN - headsize)
    KOUT("%d", len);
  if (bvm->cloonix_up_and_running)
    {
    sock_header_set_info(buf, dido_llid, len, type, val, &payload);
    if (payload != buf + headsize)
      KOUT("%p %p", payload, buf);
    llid = bvm->backdoor_llid;
    llid_backdoor_low_tx(llid, len + headsize, buf);
    }
  else
    KERR("%s", bvm->name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void llid_backdoor_tx(char *name, int llid_backdoor, int dido_llid,
                      int len, int type, int val, char  *buf)
{
  t_backdoor_vm *bvm;
  char *payload;
  int  headsize = sock_header_get_size();
  if (len > MAX_A2D_LEN - headsize)
    KOUT("%d", len);
  bvm = check_llid_name(llid_backdoor, name, __FUNCTION__);
  if (bvm) 
    {
    if (bvm->cloonix_up_and_running)
      {
      sock_header_set_info(buf, dido_llid, len, type, val, &payload);
      if (payload != buf + headsize)
        KOUT("%p %p", payload, buf);
      if (llid_backdoor != bvm->backdoor_llid)
        KOUT(" ");
      llid_backdoor_low_tx(llid_backdoor, len + headsize, buf);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void llid_backdoor_tx_reboot_to_agent(char *name)
{
  char *buf;
  int  headsize = sock_header_get_size();
  t_backdoor_vm *bvm = vm_get_with_name(name);
  if ((bvm) && (bvm->cloonix_up_and_running))
    {
    buf = get_gbuf();
    memset(buf, 0, MAX_A2D_LEN);
    bvm->reboot_id += 1;
    snprintf(buf+headsize, MAX_A2D_LEN-headsize-1, LABOOT, bvm->reboot_id);
    local_backdoor_tx(bvm, 0, strlen(buf+headsize) + 1,
                      header_type_ctrl, header_val_reboot, buf);
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void llid_backdoor_tx_halt_to_agent(char *name)
{
  char *buf;
  int  headsize = sock_header_get_size();
  t_backdoor_vm *bvm = vm_get_with_name(name);
  if ((bvm) && (bvm->cloonix_up_and_running))
    {
    buf = get_gbuf();
    memset(buf, 0, MAX_A2D_LEN);
    bvm->halt_id += 1;
    snprintf(buf+headsize, MAX_A2D_LEN-headsize-1, LAHALT, bvm->halt_id);
    local_backdoor_tx(bvm, 0, strlen(buf+headsize) + 1,
                      header_type_ctrl, header_val_halt, buf);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void llid_backdoor_tx_x11_open_to_agent(int backdoor_llid, 
                                        int dido_llid, int idx)
{
  char *buf;
  int  headsize = sock_header_get_size();
  t_backdoor_vm *bvm = vm_get_with_backdoor_llid(backdoor_llid);
  if ((bvm) && (bvm->cloonix_up_and_running))
    {
    buf = get_gbuf();
    memset(buf, 0, MAX_A2D_LEN);
    snprintf(buf+headsize, MAX_A2D_LEN-headsize-1, LAX11OPENOK, idx);
    local_backdoor_tx(bvm, dido_llid, strlen(buf+headsize) + 1, 
                      header_type_x11_ctrl, header_val_x11_open_serv, buf);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void llid_backdoor_tx_x11_close_to_agent(int backdoor_llid, 
                                         int dido_llid, int idx)
{
  char *buf;
  int  headsize = sock_header_get_size();
  t_backdoor_vm *bvm = vm_get_with_backdoor_llid(backdoor_llid);
  if ((bvm) && (bvm->cloonix_up_and_running))
    {
    buf = get_gbuf();
    memset(buf, 0, MAX_A2D_LEN);
    snprintf(buf+headsize, MAX_A2D_LEN-headsize-1, LAX11OPENKO, idx);
    local_backdoor_tx(bvm, dido_llid, strlen(buf+headsize) + 1, 
                      header_type_x11_ctrl, header_val_x11_open_serv, buf);
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static t_llid_traf *bvm_traf_find_llid(t_backdoor_vm *bvm, int dido_llid)
{
  t_llid_traf *cur;
  cur = bvm->llid_traf;
  while (cur)
    {
    if (dido_llid == cur->dido_llid)
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void bvm_traf_add_llid(t_backdoor_vm *bvm, int dido_llid)
{
  t_llid_traf *cur;
  cur = (t_llid_traf *) clownix_malloc(sizeof(t_llid_traf), 9);
  memset(cur, 0, sizeof(t_llid_traf));
  cur->dido_llid = dido_llid;
  if (bvm->llid_traf)
    bvm->llid_traf->prev = cur;
  cur->next = bvm->llid_traf;
  bvm->llid_traf = cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void bvm_traf_del_llid(t_backdoor_vm *bvm, t_llid_traf *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == bvm->llid_traf)
    bvm->llid_traf = cur->next;
  clownix_free(cur, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void llid_setup(t_backdoor_vm *bvm, int llid, int fd)
{
  if (g_llid_backdoor[llid])
    KOUT(" ");
  if (bvm->backdoor_llid)
    KOUT(" ");
  bvm->backdoor_llid = llid;
  bvm->backdoor_fd   = fd;
  g_llid_backdoor[llid] = bvm;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void llid_traf_end_all(t_backdoor_vm *bvm)
{
  t_llid_traf *cur, *next;
  cur = bvm->llid_traf;
  while(cur)
    {
    next = cur->next;
    llid_traf_delete(cur->dido_llid);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void llid_unsetup(t_backdoor_vm *bvm)
{
  if (g_llid_backdoor[bvm->backdoor_llid] != bvm)
    KOUT(" %p %p", g_llid_backdoor[bvm->backdoor_llid], bvm);
  llid_traf_end_all(bvm);
  if (msg_exist_channel(bvm->backdoor_llid))
    msg_delete_channel(bvm->backdoor_llid);
  g_llid_backdoor[bvm->backdoor_llid] = NULL;
  bvm->backdoor_llid = 0;
  bvm->backdoor_fd = 0;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void rx_from_agent_conclusion_zero_llid(t_backdoor_vm *bvm, 
                                               int type, int val, 
                                               int paylen, char *payload)
{
  int val_id;
  if (type == header_type_ctrl) 
    {
    switch (val)
      {
      case header_val_ping:
        if (sscanf(payload, LAPING, &val_id) == 1)
          {
          if (val_id == bvm->ping_id)
            {
            if (bvm->ping_status != ping_ok)
              {
              bvm->ping_status = ping_ok;
              doors_send_event(get_doorways_llid(), 0, bvm->name, PING_OK);
              }
            bvm->ping_agent_rx += 1;
            }
          }
        else
          KERR("%s", payload);
        break;

      default:
        KERR("%d", val);
        break;
      }
    }
  else if (type == header_type_stats)
    stats_from_agent_process(bvm->name, val, paylen, payload);
  else
    KERR("%d", type);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_from_agent_conclusion(t_backdoor_vm *bvm, 
                                     int type, int val,
                                     int dido_llid,
                                     int paylen, char *payload)
{
  int sub_dido_idx, llid;
  bvm->ping_agent_rx += 1;

  if ((dido_llid != 0) &&
      ((type == header_type_x11_ctrl) ||
       (type == header_type_ctrl) ||
       (type == header_type_ctrl_agent)))
    DOUT(FLAG_HOP_DOORS, "BACKDOOR_RX: %d %s", dido_llid, payload);

  if (dido_llid == 0)
    rx_from_agent_conclusion_zero_llid(bvm, type, val, paylen, payload);
  else if (type == header_type_x11_ctrl) 
    {
    switch (val)
      {
      case header_val_x11_open_serv:
        llid = bvm->backdoor_llid;
        x11_open_close(llid, dido_llid, payload);
      break;

      default:
        KERR("%d", val);
      break;
      }
    }
  else if (type == header_type_x11) 
    {
    sub_dido_idx = val;
    x11_write(dido_llid, sub_dido_idx, paylen, payload);
    }
  else if (type == header_type_ctrl) 
    {
    switch (val)
      {

      default:
        KERR("%d", val);
        break;
      }
    }
  else
    {
    llid_traf_tx_to_client(bvm->name, dido_llid, paylen, type, val, payload); 
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_pktbuf_fill(int *len, char  *buf, t_rx_pktbuf *rx_pktbuf)
{
  int result, headsize = sock_header_get_size();
  int len_chosen, len_desired, len_avail = *len;
  if (rx_pktbuf->offset < headsize)
    {
    len_desired = headsize - rx_pktbuf->offset;
    if (len_avail >= len_desired)
      {
      len_chosen = len_desired;
      result = 1;
      }
    else
      {
      len_chosen = len_avail;
      result = 2;
      }
    }
  else
    {
    if (rx_pktbuf->paylen <= 0)
      KOUT(" ");
    len_desired = headsize + rx_pktbuf->paylen - rx_pktbuf->offset;
    if (len_avail >= len_desired)
      {
      len_chosen = len_desired;
      result = 3;
      }
    else
      {
      len_chosen = len_avail;
      result = 2;
      }
    }
  if (len_chosen + rx_pktbuf->offset > MAX_A2D_LEN)
    KOUT("%d %d", len_chosen, rx_pktbuf->offset);
  memcpy(rx_pktbuf->buf+rx_pktbuf->offset, buf, len_chosen);
  rx_pktbuf->offset += len_chosen;
  *len -= len_chosen;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_pktbuf_get_paylen(t_backdoor_vm *bvm, t_rx_pktbuf *rx_pktbuf)
{
  int result = 0;
  if (sock_header_get_info(rx_pktbuf->buf, 
                           &(rx_pktbuf->dido_llid), &(rx_pktbuf->paylen),
                           &(rx_pktbuf->type), &(rx_pktbuf->val),
                           &(rx_pktbuf->payload)))
    {
    bvm->ping_status = ping_ko;
    doors_send_event(get_doorways_llid(), 0, bvm->name, PING_KO);
    llid_backdoor_cloonix_down_and_not_running(bvm->name);
    KERR("%s NOT IN SYNC LOST", bvm->name);
    rx_pktbuf->offset = 0;
    rx_pktbuf->paylen = 0;
    rx_pktbuf->payload = NULL;
    rx_pktbuf->dido_llid = 0;
    result = -1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_pktbuf_process(t_backdoor_vm *bvm, t_rx_pktbuf *rx_pktbuf)
{
  rx_from_agent_conclusion(bvm, rx_pktbuf->type, rx_pktbuf->val, 
                           rx_pktbuf->dido_llid, 
                           rx_pktbuf->paylen, rx_pktbuf->payload);
  rx_pktbuf->offset = 0;
  rx_pktbuf->paylen = 0;
  rx_pktbuf->payload = NULL;
  rx_pktbuf->dido_llid = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void backdoor_rx(t_backdoor_vm *bvm, int len, char  *buf)
{
  int res, len_done, len_left_to_do = len;
  while (len_left_to_do)
    {
    len_done = len - len_left_to_do;
    res = rx_pktbuf_fill(&len_left_to_do, buf + len_done, &(bvm->rx_pktbuf));
    if (res == 1)
      {
      if (rx_pktbuf_get_paylen(bvm, &(bvm->rx_pktbuf)))
        break;
      }
    else if (res == 2)
      {
      }
    else if (res == 3)
      {
      rx_pktbuf_process(bvm, &(bvm->rx_pktbuf));
      }
    else
      KOUT("%d", res);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void clean_connect_timer(t_backdoor_vm *bvm)
{
  if (bvm->connect_abs_beat_timer)
    clownix_timeout_del(bvm->connect_abs_beat_timer, bvm->connect_ref_timer,
                        __FILE__, __LINE__);
  bvm->connect_abs_beat_timer = 0;
  bvm->connect_ref_timer = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void clean_heartbeat_timer(t_backdoor_vm *bvm)
{
  if (bvm->heartbeat_abeat)
    clownix_timeout_del(bvm->heartbeat_abeat, bvm->heartbeat_ref,
                        __FILE__, __LINE__);
  bvm->heartbeat_abeat = 0;
  bvm->heartbeat_ref = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rearm_timer_bvm_connect_backdoor(t_backdoor_vm *bvm)
{
  clean_connect_timer(bvm);
  bvm->connect_count += 1;
  if (bvm->connect_count < 120)
    {
    clean_connect_timer(bvm);
    clownix_timeout_add(30, timer_bvm_connect_backdoor, (void *) bvm,
                        &(bvm->connect_abs_beat_timer),
                        &(bvm->connect_ref_timer));
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void vm_end_release(t_backdoor_vm *bvm)
{
  if (!bvm)
    KOUT(" ");
  if (bvm->backdoor_llid)
    llid_unsetup(bvm);
  if (bvm->prev)
    bvm->prev->next = bvm->next;
  if (bvm->next)
    bvm->next->prev = bvm->prev;
  if (bvm == head_bvm)
    head_bvm = bvm->next;
  if (nb_backdoor == 0)
    KOUT(" ");
  clownix_free(bvm, __FUNCTION__);
  nb_backdoor--;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void llid_backdoor_cloonix_down_and_not_running(char *name)
{
  t_backdoor_vm *bvm = vm_get_with_name(name);
  if (bvm)
    {
    llid_traf_end_all(bvm);
    clean_connect_timer(bvm);
    bvm->cloonix_up_and_running = 0;
    }
  else
    KERR("%s", name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
/*
static void timer_vm_release(void *data)
{
  unsigned long ul_llid = (unsigned long) data;
  t_backdoor_vm *bvm = vm_get_with_backdoor_llid((int)ul_llid);
  if (bvm)
    vm_end_release(bvm);
}
*/
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void differed_vm_release(t_backdoor_vm *bvm)
{
  unsigned long ul_llid;
  ul_llid = (unsigned long) bvm->backdoor_llid;
  clean_connect_timer(bvm);
  clean_heartbeat_timer(bvm);
  vm_end_release(bvm);
//  clownix_timeout_add(10, timer_vm_release, (void *) ul_llid, NULL, NULL);
}   
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_backdoor_vm *vm_alloc(char *name, char *path)
{
  t_backdoor_vm *bvm = NULL;
  bvm = (t_backdoor_vm *) clownix_malloc(sizeof(t_backdoor_vm), 5);
  memset(bvm, 0, sizeof(t_backdoor_vm));
  strncpy(bvm->name, name, MAX_NAME_LEN-1);
  strncpy(bvm->backdoor, path, MAX_PATH_LEN-1);
  if (head_bvm)
    head_bvm->prev = bvm;
  bvm->next = head_bvm;
  head_bvm = bvm;
  nb_backdoor++;
  return bvm;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void vm_err_cb (int llid, int err, int from)
{
  t_backdoor_vm *bvm;
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
  bvm = vm_get_with_backdoor_llid(llid);
  if (bvm)
    {
    if ((llid != bvm->backdoor_llid))
      KERR("%s", bvm->name);
    if (bvm->backdoor_llid)
      {
      if (llid != bvm->backdoor_llid)
        KOUT(" ");
      llid_backdoor_cloonix_down_and_not_running(bvm->name);
      }
    }
  else
    KERR(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int vm_rx_cb(int llid, int fd)
{
  int len;
  t_backdoor_vm *bvm;
  static char buf[MAX_A2D_LEN];
  memset(buf, 0, MAX_A2D_LEN);
  len = read (fd, buf, MAX_A2D_LEN);
  bvm = vm_get_with_backdoor_llid(llid);
  if (len == 0)
    {
    if (msg_exist_channel(llid))
       msg_delete_channel(llid);
    if (bvm)
      {
      llid_backdoor_cloonix_down_and_not_running(bvm->name);
      KERR(" BAD BACKDOOR RX LEN 0 %s", bvm->name);
      }
    }
  if (!bvm)
    KERR(" ");
  else if (llid != bvm->backdoor_llid)
    KERR("%d %s", len, bvm->name);
  else
    {
    if (len < 0)
      {
      len = 0;
      if ((errno != EAGAIN) && (errno != EINTR))
        {
        if (msg_exist_channel(llid))
          msg_delete_channel(llid);
        llid_backdoor_cloonix_down_and_not_running(bvm->name);
        KERR(" BAD BACKDOOR 4 %s", bvm->name);
        }
      }
    else
      {
      backdoor_rx(bvm, len, buf);
      }
    }
  return len;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int working_backdoor_llid(t_backdoor_vm *bvm)
{
  int result = 0;
  if (bvm->backdoor_llid)
    {
    if (msg_exist_channel(bvm->backdoor_llid))
      result = 1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void action_ga_heartbeat_on_working_llid(t_backdoor_vm *bvm)
{
  char *buf;
  int  headsize = sock_header_get_size();
  if (bvm->cloonix_up_and_running)
    {
    if (bvm->last_ping_agent_rx == bvm->ping_agent_rx)
      {
      bvm->fail_ping_agent_rx += 1;
      }
    else
      bvm->fail_ping_agent_rx = 0;
    if (bvm->fail_ping_agent_rx > 30)
      {
      bvm->fail_ping_agent_rx = 0;
      doors_send_event(get_doorways_llid(), 0, bvm->name, PING_KO);
      bvm->ping_status = ping_ko;
      }
    bvm->last_ping_agent_rx = bvm->ping_agent_rx;
    buf = get_gbuf();
    memset(buf, 0, MAX_A2D_LEN);
    bvm->ping_id += 1;
    snprintf(buf+headsize, MAX_A2D_LEN-headsize-1, LAPING, bvm->ping_id);
    local_backdoor_tx(bvm, 0, strlen(buf+headsize) + 1,
                      header_type_ctrl, header_val_ping, buf);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_ga_heartbeat(void *data)
{
  char *name = (char *) data;
  t_backdoor_vm *bvm = vm_get_with_name(name);
  if (bvm)
    {
    bvm->heartbeat_abeat = 0;
    bvm->heartbeat_ref = 0;
    if (bvm->cloonix_not_yet_connected)
      {
      bvm->cloonix_not_yet_connected = 0;
      action_bvm_connect_backdoor(bvm);
      }
    else
      {
      if (bvm->cloonix_up_and_running)
        {
        if (working_backdoor_llid(bvm))
          action_ga_heartbeat_on_working_llid(bvm);
        }
      }
    clownix_timeout_add(50, timer_ga_heartbeat, (void *) name,
                        &(bvm->heartbeat_abeat), &(bvm->heartbeat_ref));
    }
  else
    KERR("%p", data);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int llid_backdoor_ping_status_is_ok(char *name)
{
  int result = 0;
  t_backdoor_vm *bvm = vm_get_with_name(name);
  if ((bvm) && (bvm->ping_status == ping_ok))
    {
    if (bvm->ping_agent_rx > 1)
      result = 1;
    else
      KERR("%s %d", name, bvm->ping_agent_rx);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void plug_to_backdoor_llid(t_backdoor_vm *bvm)
{
  char nousebuf[MAX_PATH_LEN];
  int fd, llid, len;
  fd = sock_nonblock_client_unix(bvm->backdoor);
  if (fd >= 0)
    {
    do
      {
      len = read(fd, nousebuf, MAX_PATH_LEN);
      if (len && (len != -1))
        KERR("%d", len);
      }
    while (len > 0);
    llid = msg_watch_fd(fd, vm_rx_cb, vm_err_cb, "cloon");
    if (llid == 0)
      KOUT(" ");
    llid_setup(bvm, llid, fd);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void action_bvm_connect_backdoor(t_backdoor_vm *bvm)
{
  if (!(bvm->backdoor_llid))
    plug_to_backdoor_llid(bvm);
  if (!(working_backdoor_llid(bvm)))
    {
    rearm_timer_bvm_connect_backdoor(bvm);
    doors_send_event(get_doorways_llid(), 0, bvm->name, BACKDOOR_DISCONNECTED);
    }
  else
    {
    doors_send_event(get_doorways_llid(), 0, bvm->name, BACKDOOR_CONNECTED);
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void timer_bvm_connect_backdoor(void *data)
{ 
  t_backdoor_vm *bvm = (t_backdoor_vm *) data;
  if ((!bvm) || (!bvm->name))
    KOUT(" ");
  bvm->connect_abs_beat_timer = 0;
  bvm->connect_ref_timer = 0;
  action_bvm_connect_backdoor(bvm);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int llid_backdoor_get_info(char *name, int *llid_backdoor)
{
  int result = -1;
  t_backdoor_vm *bvm = vm_get_with_name(name);
  if (bvm)
    {
    result = -2;
    if (working_backdoor_llid(bvm))
      {
      *llid_backdoor = bvm->backdoor_llid;
      result = 0;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void llid_backdoor_add_traf(char *name, int llid_backdoor, int dido_llid)
{
  t_backdoor_vm *bvm;
  t_llid_traf *cur;
  if (llid_backdoor)
    {
    bvm = check_llid_name(llid_backdoor, name, __FUNCTION__);
    if (!bvm)
      KERR(" ");
    else
      {
      cur = bvm_traf_find_llid(bvm, dido_llid);
      if (cur)
        KOUT(" ");
      bvm_traf_add_llid(bvm, dido_llid);
      }
    }
  else
    KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void llid_backdoor_del_traf(char *name, int llid_backdoor, int dido_llid)
{
  t_backdoor_vm *bvm;
  t_llid_traf *cur;  
  if (llid_backdoor)
    {
    bvm = check_llid_name(llid_backdoor, name, __FUNCTION__);
    if (!bvm)
      KERR(" ");
    else
      {
      cur = bvm_traf_find_llid(bvm, dido_llid);
      if (!cur)
        KOUT(" ");
      bvm_traf_del_llid(bvm, cur);
      }
    }
  else
    KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void llid_backdoor_begin_unix(char *name, char *path)
{
  t_backdoor_vm *bvm = vm_get_with_name(name);
  char *vmname;
  if (!bvm)
    {
    bvm = vm_alloc(name, path);
    vmname = (char *) clownix_malloc(MAX_NAME_LEN, 8);
    memset(vmname, 0, MAX_NAME_LEN);
    strncpy(vmname, name, MAX_NAME_LEN-1);
    bvm->cloonix_not_yet_connected = 1;
    clownix_timeout_add(300, timer_ga_heartbeat, (void *) vmname,
                        &(bvm->heartbeat_abeat), &(bvm->heartbeat_ref));
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void llid_backdoor_end_unix(char *name)
{
  t_backdoor_vm *bvm = vm_get_with_name(name);
  if (bvm)
    {
    differed_vm_release(bvm);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void *llid_backdoor_get_first(char **name, int *backdoor_llid)
{
  t_backdoor_vm *bvm = head_bvm;
  void *ptr = (void *) bvm;
  if (ptr)
    {
    *name = bvm->name;
    *backdoor_llid = bvm->backdoor_llid;     
    }
  return ptr;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void *llid_backdoor_get_next(char **name, int *backdoor_llid, void *ptr_cur)
{
  t_backdoor_vm *bvm = ((t_backdoor_vm *)ptr_cur)->next; 
  void *ptr = (void *) bvm;
  if (ptr)
    {
    *name = bvm->name;
    *backdoor_llid = bvm->backdoor_llid;     
    }
  return ptr;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void llid_backdoor_cloonix_up_vport_and_running(char *name)
{
  t_backdoor_vm *bvm = vm_get_with_name(name);
  if (bvm)
    bvm->cloonix_up_and_running = 1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void llid_backdoor_init(void)
{
  head_bvm = NULL;
  nb_backdoor = 0;
}
/*--------------------------------------------------------------------------*/


