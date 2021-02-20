/*****************************************************************************/
/*    Copyright (C) 2006-2021 clownix@clownix.net License AGPL-3             */
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
#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>


#include "ioc.h"
#include "clo_tcp.h"
#include "main.h"
#include "machine.h"
#include "utils.h"
#include "packets_io.h"
#include "llid_slirptux.h"

#define MAX_INFO_LEN 200

enum {
  state_none = 0,
  state_wait_info,
  state_wait_arp_resp,
  state_wait_syn_ack,
  state_running_half,
  state_running_full,
  state_finack,
};

typedef struct t_ctx_unix2inet
{
  int state;
  int llid;
  t_all_ctx *all_ctx;
  char remote_user[MAX_NAME_LEN];
  char remote_ip[MAX_NAME_LEN];
  uint16_t remote_port;
  long long timeout_abs_beat;
  int timeout_ref;
  t_tcp_id tcpid;
} t_ctx_unix2inet;

static t_ctx_unix2inet *llid_to_ctx[CLOWNIX_MAX_CHANNELS];

typedef struct t_waiting_for_vmtx
{
  char ip[MAX_NAME_LEN];
  int llid;
  uint16_t port;
  struct t_waiting_for_vmtx *prev;
  struct t_waiting_for_vmtx *next;
} t_waiting_for_vmtx;



typedef struct t_timeout_info
{
  int llid;
  char ip[MAX_NAME_LEN];
  int is_synack;
} t_timeout_info;

static t_waiting_for_vmtx *g_head_waiting_for_arp_reply;
static t_waiting_for_vmtx *g_head_waiting_for_syn_ack;
static int free_ctx_waiting_for_vmtx(int is_syn_ack, char *ip, 
                                     int *llid, uint16_t *port);
static t_ctx_unix2inet *find_ctx(int llid);
static void free_ctx(t_all_ctx *all_ctx, int llid, int line);

void req_unix2inet_conpath_evt(t_all_ctx *all_ctx, int llid,  char *name);

/*****************************************************************************/
static void doorways_tx_resp(t_all_ctx *all_ctx, int llid, int val)
{
  char *ack;
  switch(val)
    {
    case 0:
      ack="UNIX2INET_ACK_OPEN";
      break;

    case 1:
      ack="UNIX2INET_NACK_TIMEOUT";
      break;

    default:
      ack="UNIX2INET_NACK_NOREASON";
      break;
    }
  data_tx(all_ctx, llid, strlen(ack) + 1, ack);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void timeout_free_ctx(t_all_ctx *all_ctx, void *data)
{
  t_timeout_info *ti = (t_timeout_info *) data;
  t_ctx_unix2inet *ctx = find_ctx(ti->llid);
  if (ctx && ti->is_synack)
    clo_high_free_tcpid(&(ctx->tcpid));
  free_ctx(all_ctx, ti->llid, __LINE__);
  free(ti);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timeout_waiting_for_vmtx(t_all_ctx *all_ctx, void *data)
{
  t_timeout_info *ti = (t_timeout_info *) data;
  t_ctx_unix2inet *ctx;
  int llid;
  uint16_t port;
  if (free_ctx_waiting_for_vmtx(ti->is_synack, ti->ip, &llid, &port))
    {
    KERR("%s", ti->ip);
    free(ti);
    }
  else
    {
    ctx = find_ctx(ti->llid);
    if (!ctx)
      {
      if (ti->is_synack)
        KERR("TIMEOUT WAIT SYNACK %s %d", ti->ip, port);
      else
        KERR("TIMEOUT WAIT ARP %s %d", ti->ip, port);
      free(ti);
      }
    else
      {
      doorways_tx_resp(ctx->all_ctx, llid, 1);
      clownix_timeout_add(all_ctx,10,timeout_free_ctx,(void *)ti,NULL,NULL);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int arm_timeout_waiting_for_vmtx(t_all_ctx *all_ctx, int is_synack, 
                                       t_ctx_unix2inet *ctx, int llid)
{
  int result = -1;
  t_timeout_info *ti;
  if ((ctx->timeout_abs_beat == 0) && (ctx->timeout_ref == 0)) 
    {
    ti = (t_timeout_info *) malloc(sizeof(t_timeout_info));
    memset(ti, 0, sizeof(t_timeout_info));
    ti->llid = llid;
    strncpy(ti->ip, ctx->remote_ip, MAX_NAME_LEN);
    ti->is_synack = is_synack;
    clownix_timeout_add(all_ctx, 250, timeout_waiting_for_vmtx, (void *) ti,
                        &(ctx->timeout_abs_beat), &(ctx->timeout_ref));
    result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int alloc_ctx_waiting_for_vmtx(t_all_ctx *all_ctx, int is_syn_ack, 
                                     t_ctx_unix2inet *ctx, int llid)
{
  int result = -1;
  t_waiting_for_vmtx *cur;
  if (!arm_timeout_waiting_for_vmtx(all_ctx, is_syn_ack, ctx, llid))
    {
    cur = (t_waiting_for_vmtx *) malloc(sizeof(t_waiting_for_vmtx));
    memset(cur, 0, sizeof(t_waiting_for_vmtx));
    strncpy(cur->ip, ctx->remote_ip, MAX_NAME_LEN-1);
    cur->llid = llid;
    cur->port = ctx->remote_port;
    if (is_syn_ack)
      {
      cur->next = g_head_waiting_for_syn_ack;
      if (g_head_waiting_for_syn_ack)
        g_head_waiting_for_syn_ack->prev = cur;
      g_head_waiting_for_syn_ack = cur;
      }
    else
      {
      cur->next = g_head_waiting_for_arp_reply;
      if (g_head_waiting_for_arp_reply)
        g_head_waiting_for_arp_reply->prev = cur;
      g_head_waiting_for_arp_reply = cur;
      }
    result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int free_ctx_waiting_for_vmtx(int is_syn_ack, char *ip, 
                                     int *llid, uint16_t *port)
{
  int result = -1;
  t_waiting_for_vmtx *cur;
  if (is_syn_ack)
    cur = g_head_waiting_for_syn_ack;
  else
    cur = g_head_waiting_for_arp_reply;
  while(cur)
    {
    if (!strcmp(cur->ip, ip))
      break;
    cur = cur->next;
    } 
  if (cur)
    {
    *llid = cur->llid;
    *port = cur->port;
    result = 0;
    if (cur->prev)
      cur->prev->next = cur->next;
    if (cur->next)
      cur->next->prev = cur->prev;
    if (is_syn_ack)
      {
      if (cur == g_head_waiting_for_syn_ack)
        g_head_waiting_for_syn_ack = cur->next;
      }
    else
      {
      if (cur == g_head_waiting_for_arp_reply)
        g_head_waiting_for_arp_reply = cur->next;
      }
    free(cur);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_ctx_unix2inet *find_ctx(int llid)
{
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  return (llid_to_ctx[llid]);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_ctx_unix2inet *alloc_ctx(t_all_ctx *all_ctx, int llid)
{
  t_ctx_unix2inet *ctx = (t_ctx_unix2inet *) malloc(sizeof(t_ctx_unix2inet));
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  memset(ctx, 0, sizeof(t_ctx_unix2inet));
  ctx->all_ctx = all_ctx;
  ctx->llid = llid;
  llid_to_ctx[llid] = ctx;
  DOUT((void *) all_ctx, FLAG_HOP_APP, "%s %d", __FUNCTION__, llid);
  return ctx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void free_ctx(t_all_ctx *all_ctx, int llid, int line)
{
  t_ctx_unix2inet *ctx = find_ctx(llid);
  int is_blkd;
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d %d", llid, line);
  if (!ctx)
    {
    if (line)
      KERR("%d %d", llid, line);
    }
  else
    {
    if (llid_slirptux_tcp_close_llid(llid, 0))
      KERR("%d", llid);
    free(ctx);
    llid_to_ctx[llid] = 0;
    DOUT((void *) all_ctx, FLAG_HOP_APP, "%s %d", __FUNCTION__, llid);
    }
  if (msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__))
    msg_delete_channel(all_ctx, llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void change_state(t_ctx_unix2inet *ctx, int state)
{
//  KERR("STATE: %d -> %d", ctx->state, state);
  ctx->state = state;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int tcpid_are_the_same(t_tcp_id *id1, t_tcp_id *id2)
{
  int i, result = 1;
  for (i=0; i<MAC_ADDR_LEN; i++)
    {
    if ((((int) id1->local_mac[i]) & 0xFF) != (id2->local_mac[i] & 0xFF))
      result = 0;
    if ((((int) id1->remote_mac[i]) & 0xFF) != (id2->remote_mac[i] & 0xFF))
      result = 0;
    }
  if ((id1->local_ip    != id2->local_ip)   ||
      (id1->remote_ip   != id2->remote_ip)  ||
      (id1->local_port  != id2->local_port) ||
      (id1->remote_port != id2->remote_port))
    result = 0;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void unix2inet_init_tcp_id(t_tcp_id *tcpid, 
                              char *remote_mac_ascii, char *remote_ip, 
                              int llid, uint16_t remote_port)
{
  int i;
  int local_mac[MAC_ADDR_LEN];
  int remote_mac[MAC_ADDR_LEN];
  memset(tcpid, 0, sizeof(t_tcp_id));
  if (sscanf(OUR_MAC_CISCO, "%02X:%02X:%02X:%02X:%02X:%02X",
             &(local_mac[0]), &(local_mac[1]),
             &(local_mac[2]), &(local_mac[3]),
             &(local_mac[4]), &(local_mac[5])) != 6)
    KOUT(" ");
  if (sscanf(remote_mac_ascii, "%02X:%02X:%02X:%02X:%02X:%02X",
             &(remote_mac[0]), &(remote_mac[1]),
             &(remote_mac[2]), &(remote_mac[3]),
             &(remote_mac[4]), &(remote_mac[5])) != 6)
    KOUT(" ");
  if (ip_string_to_int (&(tcpid->local_ip), get_unix2inet_ip()))
    KOUT(" ");
  if (ip_string_to_int (&(tcpid->remote_ip), remote_ip))
    KOUT(" ");
  for (i=0; i<MAC_ADDR_LEN; i++)
    {
    tcpid->local_mac[i]  = local_mac[i] & 0xFF;
    tcpid->remote_mac[i] = remote_mac[i] & 0xFF;
    }
  tcpid->local_port  = OFFSET_PORT + llid;
  tcpid->remote_port = remote_port;
}
/*---------------------------------------------------------------------------*/

#define CLOONIX_INFO "user %s ip %s port %d "

/*****************************************************************************/
static int get_info_from_buf(t_ctx_unix2inet *ctx, int len, char *ibuf)
{
  int port, result = -1;
  char *ptr;
  char *buf = (char *) malloc(len);
  memcpy(buf, ibuf, len);
  DOUT(get_all_ctx(), FLAG_HOP_APP, "INIT: %s", ibuf); 
  if (len >= TCP_MAX_SIZE)
    KERR("%d %s %d", len, buf, TCP_MAX_SIZE); 
  else
    {
    ptr = strchr(buf, '=');
    while (ptr)
      {
      *ptr = ' ';
      ptr = strchr(buf, '=');
      }
    ptr = (strstr(buf, "cloonix_info_end"));
    if (!(ptr))
      KERR("%d %s", len, buf); 
    else
      {
      *ptr = 0;
      if (sscanf(buf,CLOONIX_INFO,ctx->remote_user,ctx->remote_ip,&port) != 3)
        KERR("%d %s", len, buf); 
      else
        {
        ctx->remote_port = (port & 0xFFFF);
        ptr = ptr + strlen("cloonix_info_end");
        if ((*ptr) != 0)
          KOUT("%d %s", len, buf); 
        result = 0;
        }
      }
    }
  free(buf);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int send_first_tcp_syn(t_ctx_unix2inet *ctx,
                              char *remote_mac, char *remote_ip,
                              uint16_t remote_port)
{
  int result = -1;
  t_clo *clo;
  t_machine *machine;
  unix2inet_init_tcp_id(&(ctx->tcpid), remote_mac, remote_ip,
                        ctx->llid, remote_port);
  if (clo_high_syn_tx(&(ctx->tcpid)))
    {
    KERR(" ");
    free_ctx(ctx->all_ctx, ctx->llid, __LINE__);
    }
  else
    {
    clo = clo_mngt_find(&(ctx->tcpid));
    if (clo)
      {
      machine = look_for_machine_with_ip(remote_ip);
      if (!machine)
        {
        KERR(" ");
        free_ctx(ctx->all_ctx, ctx->llid, __LINE__);
        }
      else
        {
        if (strlen(machine->name))
          req_unix2inet_conpath_evt(ctx->all_ctx, ctx->llid, machine->name);
        else
          KERR(" ");
        result = 0;
        }
      }
    else
      {
      KERR(" ");
      free_ctx(ctx->all_ctx, ctx->llid, __LINE__);
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void request_mac_with_ip(char *remote_ip)
{
  int resp_len;
  char *resp_data;
  resp_len = format_arp_req(get_unix2inet_ip(), remote_ip, &resp_data);
  packet_output_to_slirptux(resp_len, resp_data);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void unix2inet_ssh_rx_cb(t_all_ctx *all_ctx, int llid, 
                                int len, char *buf)
{
  int is_blkd;
  void *ptr = (void *) all_ctx;
  t_ctx_unix2inet *ctx = find_ctx(llid);
  t_clo *clo;
  if (!ctx)
    {
    KERR("%d", llid);
    if (msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__))
      msg_delete_channel(all_ctx, llid);
    DOUT(ptr, FLAG_HOP_APP, "%s NO CTX", __FUNCTION__);
    }
  else
    {
    DOUT(ptr, FLAG_HOP_APP, "%s CTX STATE:%d", __FUNCTION__, ctx->state);
    if (ctx->state == state_wait_info) 
      {
      if (get_info_from_buf(ctx, len, buf)) 
        {
        KERR("%s", buf);
        free_ctx(all_ctx, llid, __LINE__);
        }
      else
        {
        if (alloc_ctx_waiting_for_vmtx(all_ctx, 0, ctx, llid))
          {
          KERR("%s", buf);
          free_ctx(all_ctx, llid, __LINE__);
          }
        else
          {
          DOUT(ptr, FLAG_HOP_APP, "%s REMOTE:%s %d", __FUNCTION__, 
                                  ctx->remote_ip, ctx->remote_port);
          request_mac_with_ip(ctx->remote_ip);
          change_state(ctx, state_wait_arp_resp);
          }
        }
      }
    else if (ctx->state == state_running_half) 
      {
      clo = clo_mngt_find(&(ctx->tcpid));
      if (!clo)
        {
        KERR(" ");
        free_ctx(all_ctx, llid, __LINE__);
        }
      else
        {
        util_attach_llid_clo(ctx->llid, clo);
        clo->tcpid.llid_unlocked = 1;
        change_state(ctx, state_running_full);
        DOUT(ptr, FLAG_HOP_APP, "%s 1DATA OF LEN: %d", __FUNCTION__, len);
        if (clo_high_data_tx(&(ctx->tcpid), len, (u8_t *) buf))
          KERR("%s %s", ctx->remote_user, ctx->remote_ip);
        }
      }
    else if (ctx->state == state_running_full)
      {
      DOUT(ptr, FLAG_HOP_APP, "%s 1DATA OF LEN: %d", __FUNCTION__, len);
      if (clo_high_data_tx(&(ctx->tcpid), len, (u8_t *) buf))
        KERR("%s %s", ctx->remote_user, ctx->remote_ip);
      }
    else if (ctx->state == state_finack)
      {
        KERR("FINACK VM SIDE");
      }
   else
      {
      KERR(" ");
      free_ctx(all_ctx, llid, __LINE__);
      DOUT(ptr, FLAG_HOP_APP, "%s NO RUNNING", __FUNCTION__);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void unix2inet_ssh_err_cb(void *ptr, int llid, int err, int from)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  free_ctx(all_ctx, llid, __LINE__);
  DOUT(ptr, FLAG_HOP_APP, "%s", __FUNCTION__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void unix2inet_ssh_connect(void *ptr, int llid, int llid_new)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  t_ctx_unix2inet *ctx;
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  if ((llid_new <= 0) || (llid_new >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid_new);
  ctx = find_ctx(llid_new);
  if (ctx)
    KOUT(" ");
  ctx = alloc_ctx(all_ctx, llid_new);
  change_state(ctx, state_wait_info);
  msg_mngt_set_callbacks(all_ctx, llid_new,unix2inet_ssh_err_cb,
                                  unix2inet_ssh_rx_cb);
  DOUT(ptr, FLAG_HOP_APP, "%s", __FUNCTION__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void unix2inet_arp_resp(char *mac, char *ip)
{
  int llid;
  uint16_t port;
  t_ctx_unix2inet *ctx;
  void *ti;
  if (free_ctx_waiting_for_vmtx(0, ip, &llid, &port))
    KERR("%s %s", mac, ip);
  else
    {
    ctx = find_ctx(llid);
    if (!ctx)
      KERR("%s %d %s", ip, port, mac);
    else if (ctx->llid != llid)
      KERR("%d %d %s %d %s", ctx->llid, llid, ip, port, mac);
    else
      {
      ti = clownix_timeout_del(ctx->all_ctx, ctx->timeout_abs_beat, 
                               ctx->timeout_ref, __FILE__, __LINE__);
      ctx->timeout_abs_beat = 0;
      ctx->timeout_ref = 0;
      free(ti);
      if (send_first_tcp_syn(ctx, mac, ip, port))
        KERR("%s %d %s", ip, port, mac);
      else
        {
        if (alloc_ctx_waiting_for_vmtx(ctx->all_ctx, 1, ctx, llid))
          {
          KERR("%s %d %s", ip, port, mac);
          free_ctx(ctx->all_ctx, llid, __LINE__);
          }
        else
          {
          change_state(ctx, state_wait_syn_ack);
          }
        }
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void unix2inet_finack_state(t_tcp_id *tcpid, int line)
{
  int llid;
  t_ctx_unix2inet *ctx;
  if (!tcpid)
    KOUT(" ");
  llid = tcpid->local_port - OFFSET_PORT;
  if ((llid > 0) && (llid < CLOWNIX_MAX_CHANNELS))
    {
    ctx = find_ctx(llid);
    if (ctx)
      {
      if (!tcpid_are_the_same(tcpid, &(ctx->tcpid)))
        KERR("%d", llid);
      change_state(ctx, state_finack);
      }
    }
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
int unix2inet_ssh_syn_ack_arrival(t_tcp_id *tcpid)
{
  int llid, result = -1;
  t_ctx_unix2inet *ctx;
  t_clo *clo;
  void *ti;
  if (!tcpid)
    KOUT(" ");
  clo = clo_mngt_find(tcpid);
  llid = tcpid->local_port - OFFSET_PORT;
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  ctx = find_ctx(llid);
  if (!ctx)
    KERR("%d", llid);
  else if (!tcpid_are_the_same(tcpid, &(ctx->tcpid))) 
    KERR("%d", llid);
  else if (!clo)
    KERR("%d", llid);
  else if (clo->tcpid.llid != 0)
    KERR("%d", clo->tcpid.llid);
  else if (ctx->state != state_wait_syn_ack) 
    KERR("%d %d", llid, ctx->state);
  else
    {
    ti = clownix_timeout_del(ctx->all_ctx, ctx->timeout_abs_beat,
                             ctx->timeout_ref, __FILE__, __LINE__);
    ctx->timeout_abs_beat = 0;
    ctx->timeout_ref = 0;
    free(ti);
    doorways_tx_resp(ctx->all_ctx, ctx->llid, 0);
    change_state(ctx, state_running_half);
    result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void unix2inet_close_tcpid(t_tcp_id *tcpid)
{
  int llid, i, local_mac[MAC_ADDR_LEN];
  t_ctx_unix2inet *ctx;
  uint32_t addr;
  int our_concern = 1;
  if (sscanf(OUR_MAC_CISCO, "%02X:%02X:%02X:%02X:%02X:%02X",
             &(local_mac[0]), &(local_mac[1]),
             &(local_mac[2]), &(local_mac[3]), 
             &(local_mac[4]), &(local_mac[5])) != 6)
    KOUT(" ");
  if (ip_string_to_int (&(addr), get_unix2inet_ip()))
    KOUT(" ");
  if (tcpid->local_ip != addr)
    our_concern = 0;
  for (i=0; i<MAC_ADDR_LEN; i++)
    {
    if (tcpid->local_mac[i] != (local_mac[i] & 0xFF))
      our_concern = 0;
    }
  if (our_concern)
    {
    llid = tcpid->local_port - OFFSET_PORT;
    if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
      KOUT("%d", llid);
    ctx = find_ctx(llid);
    if (ctx)
      {
      if (!tcpid_are_the_same(tcpid, &(ctx->tcpid)))
        KERR("%d", llid);
      else
        {
        KERR("%d", llid);
        free_ctx(ctx->all_ctx, llid, __LINE__);
        }
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void free_unix2inet_conpath(t_all_ctx *all_ctx, int llid)
{
  free_ctx(all_ctx, llid, 0);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void unix2inet_init(t_all_ctx *all_ctx)
{
  char tcp_path[MAX_PATH_LEN];
  void *ptr = (void *) all_ctx;
  memset(llid_to_ctx, 0, CLOWNIX_MAX_CHANNELS * sizeof(t_ctx_unix2inet *));
  if ((strlen(all_ctx->g_path) + 10) > MAX_PATH_LEN) 
    KOUT("%s", all_ctx->g_path);
  sprintf(tcp_path, "%s", all_ctx->g_path);
  strcat(tcp_path, "_u2i");
  if (rawdata_server_unix(all_ctx, tcp_path, unix2inet_ssh_connect) == 0)
    KOUT("PROBLEM %s", tcp_path);
  DOUT(ptr, FLAG_HOP_APP, "%s %s", __FUNCTION__, tcp_path);
}
/*--------------------------------------------------------------------------*/

