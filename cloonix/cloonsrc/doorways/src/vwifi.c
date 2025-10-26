/*****************************************************************************/
/*    Copyright (C) 2006-2025 clownix@clownix.net License AGPL-3             */
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
#include "util_sock.h"
#include "rpc_clownix.h"
#include "doors_rpc.h"
#include "llid_traffic.h"
#include "llid_backdoor.h"


typedef struct t_delayed_vwifi
{
  char name[MAX_NAME_LEN];
  int  type;
  int  val;
  int  vwifi_base;
  int  vwifi_cid;
  int  paylen;
  char *payload;
} t_delayed_vwifi;



typedef struct t_vwifi_record
{
  int llid;
  int vwifi_base;
  int vwifi_cid;
  char name[MAX_NAME_LEN];
  int type;
  int val_traf;
  int val_ok;
  int val_ko;
  struct t_vwifi_record *prev;
  struct t_vwifi_record *next;
} t_vwifi_record;

char *get_vwifi_server_path_cli(void);
char *get_vwifi_server_path_spy(void);
char *get_vwifi_server_path_ctr(void);
static t_vwifi_record *g_vwifi_record[CLOWNIX_MAX_CHANNELS];
static t_vwifi_record *g_head_record;

/****************************************************************************/
static void local_to_agent_tx(t_backdoor_vm *bvm, int vwifi_base, int vwifi_cid,
                              int type, int val, int len, char  *buf)
{   
  char *payload;
  int llid, headsize = sock_header_get_size();
  if (len > MAX_A2D_LEN - headsize)
    KOUT("ERROR %d", len);
  sock_header_set_info(buf, 0, vwifi_base, vwifi_cid, type, val, len, &payload);
  if (payload != buf + headsize)
    KOUT("ERROR %p %p", payload, buf);
  llid = bvm->backdoor_llid;
  llid_backdoor_low_tx(llid, len + headsize, buf);
}   
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int to_agent_tx(t_backdoor_vm *bvm, int type, int val,
                       int vwifi_base, int vwifi_cid,
                       int paylen, char *payload)
{ 
  int result = -1;
  char buf[MAX_A2D_LEN];
  int  headsize = sock_header_get_size();

  if (paylen > MAX_A2D_LEN - headsize)
    KOUT("ERROR %d %d %d", paylen, headsize, MAX_A2D_LEN);
  if ((bvm) && (bvm->cloonix_up_and_running))
    {
    memcpy(buf+headsize, payload, paylen);
    local_to_agent_tx(bvm, vwifi_base, vwifi_cid, type, val, paylen, buf);
    result = 0;
    }
  else
    KERR("ERROR %d %d %d", type, val, paylen);
  return result;
}  
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_vwifi_record *find_record(char *name, int vwifi_cid)
{
  t_vwifi_record *cur = g_head_record;
  while(cur)
    {
    if ((!strcmp(name, cur->name)) && (vwifi_cid == cur->vwifi_cid))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void create_record(t_backdoor_vm *bvm, int vwifi_base,
                          int vwifi_cid, int llid, int type)
{
  int len = strlen(LAVWIFISYN)+1;
  t_vwifi_record *cur;

  char tx[2];
  tx[0] = ((vwifi_cid & 0xFF00) >> 8) & 0xFF;
  tx[1] = vwifi_cid & 0xFF;

  if ((llid < 1) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("ERROR %d %d", vwifi_cid, llid);
  if (g_vwifi_record[llid])
    KERR("ERROR CREATE RECORD: %s llid:%d cid:%d", bvm->name, llid, vwifi_cid); 
  if (watch_tx_sync(llid, 2, tx) != 2) 
    KERR("ERROR %d %d errno: %d", vwifi_cid, llid, errno);
  else
    {
    cur = (t_vwifi_record *) malloc(sizeof(t_vwifi_record));
    memset(cur, 0, sizeof(t_vwifi_record));
    cur->llid = llid;
    cur->vwifi_base = vwifi_base;
    cur->vwifi_cid = vwifi_cid;
    strncpy(cur->name, bvm->name, MAX_NAME_LEN-1);
    cur->type = type;
    cur->next = g_head_record;
    if (g_head_record)
      g_head_record->prev = cur;
    g_head_record = cur;
    g_vwifi_record[llid] = cur;
    to_agent_tx(bvm, type, header_val_vwifi_syn_ok, vwifi_base,
                vwifi_cid, len, LAVWIFISYN);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void delete_record(t_vwifi_record *cur)
{
  t_backdoor_vm *bvm;
  int len = strlen(LAVWIFISYN)+1;
  if ((cur->llid < 1) || (cur->llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("ERROR %d", cur->llid);
  if (g_vwifi_record[cur->llid] != cur)
    KOUT("ERROR %d", cur->llid);
  g_vwifi_record[cur->llid] = NULL;
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  g_head_record = cur->next;
  bvm = llid_backdoor_vm_get_with_name(cur->name);
  if (bvm != NULL)
    to_agent_tx(bvm, cur->type, header_val_vwifi_trf_ko,
                cur->vwifi_base, cur->vwifi_cid,
                len, LAVWIFISYN);
  if (msg_exist_channel(cur->llid))
    msg_delete_channel(cur->llid);
  free(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int vwifi_rx_cb(int llid, int fd)
{
  t_backdoor_vm *bvm;
  int len, headsize = sock_header_get_size();
  t_vwifi_record *cur = g_vwifi_record[llid];
  char buf[MAX_A2D_LEN];

  if ((llid < 1) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("ERROR %d", llid);
  else if (cur == NULL)
    {
    KERR("ERROR RECORD NOT FOUND %d", llid);
    if (msg_exist_channel(llid))
      msg_delete_channel(llid);
    }
  else
    {
    if (llid != cur->llid)
      KOUT("ERROR llid:%d cur->llid:%d fd:%d", llid, cur->llid, fd);
    bvm = llid_backdoor_vm_get_with_name(cur->name);
    if (bvm == NULL)
      {
      delete_record(cur);
      }
    else
      {
      len = read(fd, buf, MAX_A2D_LEN-headsize);
      if (len == 0)
        {
        delete_record(cur);
        }
      else if (len == -1)
        {
        if ((errno != EINTR) && (errno != EAGAIN))
          {
          KERR("ERROR ERRNO WHILE READ %s llid:%d", cur->name, llid);
          delete_record(cur);
          }
        else
          KERR("WARNING EAGAIN %s llid:%d", cur->name, llid);
        }
      else
        {
        //KERR("DOORWAY TO KVM CID:%d %s llid:%d len:%d",
        //     cur->vwifi_cid, cur->name, llid, len); 
        to_agent_tx(bvm, cur->type, header_val_vwifi_trf_ok,
                    cur->vwifi_base, cur->vwifi_cid, len, buf);
        }
      }
    }

  return len;
}
/****************************************************************************/
static void vwifi_err_cb(int llid, int err, int from)
{
  t_vwifi_record *cur;
  if ((llid < 1) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("ERROR %d %d %d", llid, err, from);
  cur = g_vwifi_record[llid];
  if (cur == NULL)
    {
    KERR("ERROR NO RECORD llid:%d err:%d from:%d", llid, err, from);
    if (msg_exist_channel(llid))
      msg_delete_channel(llid);
    }
  else
    {
    KERR("ERROR ERROR CALLBACK %s llid:%d err:%d from:%d", cur->name, llid, err, from);
    if (llid != cur->llid)
      KOUT("ERROR %d %d %d", llid, err, from);
    delete_record(cur);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void local_rx_from_agent(t_backdoor_vm *bvm, char *path, int type,
                                int val, int vwifi_base, int vwifi_cid, 
                                int paylen, char *payload)
{
  int fd, llid;
  t_vwifi_record *cur = find_record(bvm->name, vwifi_cid);
  if (val == header_val_vwifi_syn_ok)
    {
    if (cur != NULL)
      KERR("ERROR SYN RECORD EXISTS  %s cid:%d", bvm->name, vwifi_cid);
    else
      {
      if (util_client_socket_unix(path, &fd))
        {
        KERR("ERROR SYN SOCKET %s cid:%d %s", bvm->name, vwifi_cid, path);
        to_agent_tx(bvm, type, header_val_vwifi_syn_ko,
                    vwifi_base, vwifi_cid,
                    strlen(LAVWIFISYN)+1, LAVWIFISYN);
        }
      else
        {
        llid = msg_watch_fd(fd, vwifi_rx_cb, vwifi_err_cb, "vwifi");
        if (llid == 0)
          KOUT("ERROR %s", bvm->name);
        create_record(bvm, vwifi_base, vwifi_cid, llid, type);
        }
      }
    }
  else if (val == header_val_vwifi_trf_ok)
    {
    if (cur == NULL)
      {
      KERR("ERROR %s cid:%d len:%d", bvm->name, vwifi_cid, paylen);
      to_agent_tx(bvm, type, header_val_vwifi_syn_ko, vwifi_base, vwifi_cid,
                  strlen(LAVWIFISYN)+1, LAVWIFISYN);
      }
    else
      {
      watch_tx_sync(cur->llid, paylen, payload);
      //KERR("KVM TO DOORWAY CID:%d %s llid:%d len:%d",
      //        cur->vwifi_cid, cur->name, cur->llid, paylen);
      }
    }
  else if (val == header_val_vwifi_trf_ko)
    {
    if (cur == NULL)
      KERR("ERROR %s cid:%d", bvm->name, vwifi_cid);
    else
      {
      delete_record(cur);
      }
    }
  else if (val == header_val_vwifi_bad_ko)
    {
    if (cur == NULL) 
      KERR("ERROR %s cid:%d", bvm->name, vwifi_cid);
    else
      {
      delete_record(cur);
      }
    }
  else
    KERR("ERROR %s %d %s", bvm->name, val, payload);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void delayed_rx_from_agent(void *data)
{
  char *path;
  t_delayed_vwifi *cur = (t_delayed_vwifi *) data;
  char *name      = cur->name;
  int  type       = cur->type;
  int  val        = cur->val;
  int  vwifi_base = cur->vwifi_base;
  int  vwifi_cid  = cur->vwifi_cid;
  int  paylen     = cur->paylen;
  char *payload   = cur->payload;
  t_backdoor_vm *bvm = llid_backdoor_vm_get_with_name(name);

  if (!bvm)
    {
    KERR("ERROR %s %d %d", name, vwifi_cid, paylen);
    free(cur->payload);
    free(cur);
    return;
    } 
  if (type == header_type_vwifi_cli)
    {
    path = get_vwifi_server_path_cli();
    local_rx_from_agent(bvm, path, type, val,
                        vwifi_base, vwifi_cid,
                        paylen, payload);
    }
  else if (type == header_type_vwifi_spy)
    {
    path = get_vwifi_server_path_spy();
    local_rx_from_agent(bvm, path, type, val,
                        vwifi_base, vwifi_cid,
                        paylen, payload);
    }
  else if (type == header_type_vwifi_ctr)
    {
    path = get_vwifi_server_path_ctr();
    local_rx_from_agent(bvm, path, type, val,
                        vwifi_base, vwifi_cid,
                        paylen, payload);
    } 
  else
    KERR("ERROR %d %d %d", type, val, paylen);
  free(cur->payload);
  free(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void vwifi_rx_from_agent(char *name, int type, int val,
                         int vwifi_base, int vwifi_cid,
                         int paylen, char *payload)
{
  t_delayed_vwifi *cur = (t_delayed_vwifi *) malloc(sizeof(t_delayed_vwifi));
  memset(cur, 0, sizeof(t_delayed_vwifi));
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  cur->type = type;
  cur->val  = val;
  cur->vwifi_base = vwifi_base;
  cur->vwifi_cid  = vwifi_cid;
  cur->paylen  = paylen;
  cur->payload = (char *) malloc(paylen);
  memcpy(cur->payload, payload, paylen);
  clownix_timeout_add(1, delayed_rx_from_agent, (void *)cur, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void vwifi_heartbeat(int count)
{
/*
  t_vwifi_record *cur = g_head_record;
  while(cur)
    {
    KERR("STORED RECORD: %s llid:%d cid:%d",
         cur->name, cur->llid, cur->vwifi_cid); 
    cur = cur->next;
    }
*/
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void vwifi_init(void)
{
  memset(g_vwifi_record, 0, CLOWNIX_MAX_CHANNELS * sizeof(t_vwifi_record *));
  g_head_record = NULL;
}
/*--------------------------------------------------------------------------*/


