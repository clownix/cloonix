/*****************************************************************************/
/*    Copyright (C) 2006-2019 cloonix@cloonix.net License AGPL-3             */
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
#include <asm/types.h>
#include <stdlib.h>
#include <netinet/in.h>

#include "ioc.h"
#include "clo_tcp.h"
#include "clo_mngt.h"
#include "clo_low.h"
#include "clo_utils.h"
/*---------------------------------------------------------------------------*/

void clo_low_output(int mac_len, u8_t *mac_data);

#define MAX_HASH_IDX 0xFFF+2 

typedef struct t_fast_clo
{
  t_clo *clo;
  int idx;
  struct t_fast_clo *prev;
  struct t_fast_clo *next;
} t_fast_clo;

static t_fast_clo *g_head_fast_clo[MAX_HASH_IDX];
static t_clo *llid_to_clo[CLOWNIX_MAX_CHANNELS];

/*****************************************************************************/
void util_trace_tcpid(t_tcp_id *t, char *fct)
{
  KERR("%s %02X %02X %02X %02X %02X %02X   %02X %02X %02X %02X %02X %02X", fct,
    t->local_mac[0] & 0xFF, t->local_mac[1] & 0xFF, t->local_mac[2] & 0xFF,
    t->local_mac[3] & 0xFF, t->local_mac[4] & 0xFF, t->local_mac[5] & 0xFF,
    t->remote_mac[0] & 0xFF, t->remote_mac[1] & 0xFF, t->remote_mac[2] & 0xFF,
    t->remote_mac[3] & 0xFF, t->remote_mac[4] & 0xFF, t->remote_mac[5] & 0xFF);
  KERR("%s %08X  %08X", fct, t->local_ip, t->remote_ip);
  KERR("%s %d  %d", fct, t->local_port & 0xFFFF, t->remote_port & 0xFFFF);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int util_tcpid_comp(t_tcp_id *a, t_tcp_id *b)
{
  int result = -1;
  if ((!(memcmp(a->remote_mac, b->remote_mac, MAC_ADDR_LEN))) &&
        (a->remote_port    ==  b->remote_port)    &&
        (a->local_port     ==  b->local_port)   &&
        (a->remote_ip      ==  b->remote_ip)     &&
        (a->local_ip       ==  b->local_ip))
    result = 0;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_clo *util_get_clo(int llid)
{
  t_clo *result = NULL;
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT(" %d", llid);
  result = llid_to_clo[llid];
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int fast_clo_comp(t_tcp_id *a, t_tcp_id *b)
{
  int result = -1;
  if ((!(memcmp(a->remote_mac, b->remote_mac, MAC_ADDR_LEN))) &&
      (a->remote_port == b->remote_port)   &&
      (a->local_port  == b->local_port )   &&
      (a->remote_ip   == b->remote_ip  )   &&
      (a->local_ip    == b->local_ip   ))
    result = 0;
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int get_hash(u8_t *fmac, u32_t lip, u32_t fip, u16_t lport, u16_t fport)
{
  int result;
  int hash4;
  short hash2;
  hash4 = fmac[0]<<24 | fmac[1]<<16 | fmac[2]<<8 | fmac[3];
  hash2 = (fmac[4]<<8 | fmac[5]) & 0xFFFF;
  hash4 ^= lip;
  hash4 ^= fip;
  hash2 ^= hash4 & 0xFFFF;
  hash2 ^= (hash4 >> 16) & 0xFFFF;
  hash2 ^= lport;
  hash2 ^= fport;
  result = (hash2 ^ (hash2 >> 12)) & 0xFFF;
  result += 1;
  if ((result <= 0) || (result >= MAX_HASH_IDX))
    KOUT("%d", result);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_fast_clo *fast_get(t_tcp_id *tid)
{
  int idx;
  t_fast_clo *cur;
  idx = get_hash(tid->remote_mac, 
                 tid->local_ip, tid->remote_ip, 
                 tid->local_port, tid->remote_port);
  cur = g_head_fast_clo[idx];
  while (cur && (fast_clo_comp(&(cur->clo->tcpid), tid)))
    cur = cur->next;
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int create_fast(t_clo *clo)
{
  int idx, result = 0;
  t_fast_clo *fc = fast_get(&clo->tcpid);
  if (fc)
    result = -1;
  else
    {
    idx = get_hash(clo->tcpid.remote_mac, 
                   clo->tcpid.local_ip, clo->tcpid.remote_ip, 
                   clo->tcpid.local_port, clo->tcpid.remote_port);
    fc = (t_fast_clo *) malloc(sizeof(t_fast_clo));
    memset(fc, 0, sizeof(t_fast_clo));
    fc->idx = idx;
    fc->clo = clo;
    if (g_head_fast_clo[idx])
      g_head_fast_clo[idx]->prev = fc;
    fc->next = g_head_fast_clo[idx];
    g_head_fast_clo[idx] = fc;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void delete_fast(t_clo *clo)
{
  t_fast_clo *fc = fast_get(&(clo->tcpid));
  if (!fc)
    KOUT(" ");
  if (fc->prev)
    fc->prev->next = fc->next;
  if (fc->next)
    fc->next->prev = fc->prev;
  if (fc == g_head_fast_clo[fc->idx])
    g_head_fast_clo[fc->idx] = fc->next;
  memset(fc, 0, sizeof(t_fast_clo));
  free(fc);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
t_clo *util_get_fast_clo(t_tcp_id *tid)
{
  t_fast_clo *fc = fast_get(tid);
  t_clo *result = NULL;
  if (fc)
    result = fc->clo;
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void util_attach_llid_clo(int llid, t_clo *clo)
{
  t_clo *checkclo = util_get_fast_clo(&(clo->tcpid));
  if (checkclo != clo)
    KOUT(" ");
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT(" %d", llid);
  if (util_get_clo(llid))
    KOUT(" ");
  llid_to_clo[llid] = clo;
  clo->tcpid.llid = llid;
  clo->tcpid.history_llid = llid;
//  KERR("LLID%d ATTACH INFO: %d %d", clo->tcpid.history_llid,
//                                    clo->tcpid.local_port & 0xFFFF,
//                                    clo->tcpid.remote_port & 0xFFFF);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void util_detach_llid_clo(int llid, t_clo *clo)
{
  t_clo *checkclo;
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT(" %d", llid);
  checkclo = util_get_clo(llid);
  if (!checkclo)
    KOUT(" ");
  if (checkclo != clo)
    KOUT(" ");
  if (clo->tcpid.llid != llid)
    KOUT(" ");
  clo->tcpid.llid = 0;
  llid_to_clo[llid] = NULL;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *util_state2ascii(int state)
{
  switch(state)
    {
    case state_idle:
      return("IDLE");
      break;
    case state_created:
      return("CREATED");
      break;
    case state_first_syn_sent:
      return("SYN_SENT");
      break;
    case state_established:
      return("ESTABLISHED");
      break;
    case state_fin_wait_last_ack:
      return("FIN_WAIT_LASTACK");
      break;
    case state_closed:
      return("CLOSED");
      break;
    default:
      KOUT(" %d ", state);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int util_low_tcpid_comp(t_low *low, t_tcp_id *tcpid)
{
  int result = -1;
  if ((!(memcmp(tcpid->remote_mac, low->mac_src, MAC_ADDR_LEN))) &&
      (!(memcmp(tcpid->local_mac, low->mac_dest, MAC_ADDR_LEN))) &&
      (tcpid->remote_port    ==  low->tcp_src)    &&
      (tcpid->local_port     ==  low->tcp_dest)   &&
      (tcpid->remote_ip      ==  low->ip_src)     &&
      (tcpid->local_ip       ==  low->ip_dest))
    result = 0;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void util_low2tcpid(t_tcp_id *tcpid, t_low *low)
{
  memset(tcpid, 0, sizeof(t_tcp_id));
  memcpy(tcpid->remote_mac, low->mac_src, MAC_ADDR_LEN);
  memcpy(tcpid->local_mac,  low->mac_dest, MAC_ADDR_LEN);
  tcpid->remote_port    =  low->tcp_src;
  tcpid->local_port     =  low->tcp_dest;
  tcpid->remote_ip      =  low->ip_src;
  tcpid->local_ip       =  low->ip_dest;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void util_tcpid2low(t_low *low, t_tcp_id *tcpid)
{
  memcpy(low->mac_src, tcpid->local_mac, MAC_ADDR_LEN);
  memcpy(low->mac_dest, tcpid->remote_mac, MAC_ADDR_LEN);
  low->ip_src   = tcpid->local_ip;
  low->ip_dest  = tcpid->remote_ip;
  low->tcp_src  = tcpid->local_port;
  low->tcp_dest = tcpid->remote_port;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
t_ldata *util_insert_ldata(t_ldata **head, t_low *low)
{
  t_ldata *ldata = (t_ldata *) malloc(sizeof(t_ldata));
  t_ldata *cur = *head;
  memset(ldata, 0, sizeof(t_ldata));
  ldata->low = low;
  while(cur && cur->next)
    cur = cur->next;
  if (!cur)
    {
    if (*head)
      KOUT(" ");
    (*head) = ldata;
    }
  else
    {
    if (cur->next)
      KOUT(" ");
    cur->next = ldata;
    ldata->prev = cur;
    }
  return ldata;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void util_extract_ldata(t_ldata **head, t_ldata *ldata)
{
  if (ldata->prev)
    ldata->prev->next = ldata->next;
  if (ldata->next)
    ldata->next->prev = ldata->prev;
  if ((*head) == ldata)
    (*head) = ldata->next;
  free(ldata->low->data);
  free(ldata->low);
  free(ldata);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
t_hdata *util_insert_hdata(t_hdata **head, int max_len, int len, u8_t *data)
{
  t_hdata *hdata = (t_hdata *) malloc(sizeof(t_hdata));
  t_hdata *cur = (*head);
  memset(hdata, 0, sizeof(t_hdata));
  hdata->max_len = len;
  hdata->len = len;
  hdata->data = data;
  while(cur && cur->next) 
    cur = cur->next;
  if (!cur)
    {
    if (*head)
      KOUT(" ");
    (*head) = hdata;
    }
  else
    {
    if (cur->next)
      KOUT(" ");
    cur->next = hdata;
    hdata->prev = cur;
    }
  return hdata;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void util_extract_hdata(t_hdata **head, t_hdata *hdata)
{
  if (hdata->prev)
    hdata->prev->next = hdata->next;
  if (hdata->next)
    hdata->next->prev = hdata->prev;
  if ((*head) == hdata)
    (*head) = hdata->next;
  free(hdata->data);
  free(hdata);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
t_clo *util_insert_clo(t_clo **head, t_tcp_id *tcpid)
{
  static int id_tcpid = 0;
  t_clo *clo = (t_clo *) malloc(sizeof(t_clo));
  memset(clo, 0, sizeof(t_clo));
  if (tcpid->llid != 0)
    KOUT(" ");
  memcpy(&(clo->tcpid), tcpid, sizeof(t_tcp_id));
  if (create_fast(clo))
    {
    free(clo);
    clo = NULL;
    }
  else
    {
    id_tcpid += 1;
    clo->id_tcpid = id_tcpid;
    if (*head)
      (*head)->prev = clo; 
    clo->next = (*head);
    (*head) = clo;
    }
  return clo;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int util_purge_hdata(t_clo *clo)
{
  int result = 0;
  if (clo->head_hdata)
    result = -1;
  while (clo->head_hdata)
    util_extract_hdata(&(clo->head_hdata), clo->head_hdata);
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int util_purge_ldata(t_clo *clo)
{
  int result = 0;
  if (clo->head_ldata)
    result = -1;
  while (clo->head_ldata)
    util_extract_ldata(&(clo->head_ldata), clo->head_ldata);
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int util_extract_clo(t_clo **head, t_clo *clo)
{
  int result = 0;
  if (clo->head_ldata)
    KERR("ln:%d %d", clo->closed_state_count_line, clo->tcpid.history_llid);
  while (clo->head_hdata)
    {
    util_extract_hdata(&(clo->head_hdata), clo->head_hdata);
    result = -1;
    }
  while (clo->head_ldata)
    {
    util_extract_ldata(&(clo->head_ldata), clo->head_ldata);
    result = -1;
    }
  if (clo->tcpid.llid)
    util_detach_llid_clo(clo->tcpid.llid, clo);
  delete_fast(clo);
  if (clo->prev)
    clo->prev->next = clo->next;
  if (clo->next)
    clo->next->prev = clo->prev;
  if (clo == (*head))
    (*head) = clo->next;
  memset(clo, 0, sizeof(t_clo));
  free(clo);
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static t_low *util_make_low(t_tcp_id *tcpid, u32_t ackno, u32_t seqno,
                            u8_t  flags, u16_t wnd, int datalen, u8_t *data)
{
  t_low *low = (t_low *) malloc(sizeof(t_low));
  memset(low, 0, sizeof(t_low));
  util_tcpid2low(low, tcpid);
  if (datalen > 0)
    {
    low->data = data;
    low->datalen = (u16_t) datalen;
    }
  low->ackno = ackno;
  low->seqno = seqno;
  low->flags = flags;
  low->wnd   = wnd;
  return low;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void send_seg(t_tcp_id *tcpid, u32_t ackno, u32_t seqno, u16_t wnd,
                     u8_t flags, int hlen, u8_t *hdata)
{
  int mac_len;
  u8_t *mac_data;
  t_low *low;
  low = util_make_low(tcpid, ackno, seqno, flags, wnd, hlen, hdata);
  tcp_low2packet(&mac_len, &mac_data, low);
  free(low->data);
  free(low);
  clo_low_output(mac_len, mac_data);
  free(mac_data);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void util_send_reset(t_tcp_id *tcpid, u32_t ackno, u32_t seqno, u16_t wnd, 
                     const char *fct, int ln)
{
  send_seg(tcpid, ackno, seqno, wnd, TH_RST, 0, NULL);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void util_send_syn(t_tcp_id *tcpid, u32_t ackno, u32_t seqno, u16_t wnd)
{
  send_seg(tcpid, ackno, seqno, wnd, TH_SYN, 0, NULL);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void util_send_ack(t_tcp_id *tcpid, u32_t ackno, u32_t seqno, u16_t wnd)
{
  send_seg(tcpid, ackno, seqno, wnd, TH_ACK, 0, NULL);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void util_send_synack(t_tcp_id *tcpid, u32_t ackno, u32_t seqno, u16_t wnd)
{
  printf("%s\n", __FUNCTION__);
  send_seg(tcpid, ackno, seqno, wnd, TH_SYN | TH_ACK, 0, NULL);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void util_send_finack(t_tcp_id *tcpid, u32_t ackno, u32_t seqno, u16_t wnd)
{
  printf("%s\n", __FUNCTION__);
  send_seg(tcpid, ackno, seqno, wnd, TH_FIN | TH_ACK, 0, NULL);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void util_send_fin(t_tcp_id *tcpid, u32_t ackno, u32_t seqno, u16_t wnd)
{
  printf("%s\n", __FUNCTION__);
  send_seg(tcpid, ackno, seqno, wnd, TH_FIN, 0, NULL);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void util_send_data(t_tcp_id *tcpid, u32_t ackno, u32_t seqno, u16_t wnd, 
                    int hlen, u8_t *hdata)
{
  send_seg(tcpid, ackno, seqno, wnd, TH_PUSH | TH_ACK, hlen, hdata);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void util_init(void)
{
  memset(g_head_fast_clo, 0, sizeof(t_fast_clo *) * MAX_HASH_IDX);
  memset(llid_to_clo, 0, sizeof(t_clo *) * CLOWNIX_MAX_CHANNELS);
}
/*---------------------------------------------------------------------------*/
