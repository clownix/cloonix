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
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "ioc.h"
#include "hwsim.h"
#include "iface.h"
#include "blkd_addr.h"
#include "main.h"

typedef struct t_dst_elem
{
  uint8_t addr[ETH_ALEN];
  uint8_t fillalign[2];
  struct t_dst_elem *next;
} t_dst_elem;

typedef struct t_blkd_addr
{
  int idx;
  int ready_to_tx;
  uint8_t our_hwaddr[ETH_ALEN];
  uint8_t our_addr[ETH_ALEN];
  uint8_t fillalign[4];
  int fd_virtio;
  int llid_virtio;
  t_dst_elem *head_dst_elem;
} t_blkd_addr;

static t_blkd_addr *g_blkd_addr[MAX_NB_IDX];
static t_blkd_addr *g_llid_virtio[CLOWNIX_MAX_CHANNELS];


/****************************************************************************/
void blkd_addr_print(char *str, uint8_t *addr)
{
  int i, iaddr[ETH_ALEN];
  str[MAX_LEN_PRINT-1] = 0;
  for (i=0; i<ETH_ALEN; i++)
    iaddr[i] = (int) (addr[i]) & 0xFF;
  snprintf(str, MAX_LEN_PRINT-2, "%02X:%02X:%02X:%02X:%02X:%02X", 
         iaddr[0], iaddr[1],iaddr[2],iaddr[3],iaddr[4],iaddr[5]);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void free_blkd_addr(t_all_ctx *all_ctx, int idx)
{
  t_blkd_addr *blkd_addr;
  t_dst_elem *cur, *next;
  int llid;
  if ((idx < 0) || (idx >= MAX_NB_IDX))
    KOUT("%d", idx);
  blkd_addr = g_blkd_addr[idx];
  if (blkd_addr)
    {
    cur = blkd_addr->head_dst_elem;
    while(cur)
      {
      next = cur->next;
      free(cur);
      cur = next;
      } 
    blkd_addr->head_dst_elem = NULL;
    llid = blkd_addr->llid_virtio;
    if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
      KOUT("%d", llid);
    if (msg_exist_channel(all_ctx, llid, __FUNCTION__))
      msg_delete_channel(all_ctx, llid);
    close(blkd_addr->fd_virtio);
    g_llid_virtio[llid] = NULL;
    blkd_addr->fd_virtio = -1;
    blkd_addr->llid_virtio = 0;
    }
}
/*-------------------------------------------------------------------------*/

/*****************************************************************************/
static int find_dst_in_chain(t_dst_elem *head_dst_elem, uint8_t *dst)
{
  int result = 0;
  t_dst_elem *cur = head_dst_elem;
  while (cur)
    {
    if (!memcmp(cur->addr, dst, ETH_ALEN))
      {
      result = 1;
      break;
      }
    cur = cur->next;
    }
  return result;
}
/*-------------------------------------------------------------------------*/

/*****************************************************************************/
static void add_dst_elem(t_blkd_addr *blkd_addr, uint8_t *dst)
{
  t_dst_elem *elem;
  if (!find_dst_in_chain(blkd_addr->head_dst_elem, dst))
    {
    elem = (t_dst_elem *) malloc(sizeof(t_dst_elem));
    memset(elem, 0, sizeof(t_dst_elem));
    memcpy(elem->addr, dst, ETH_ALEN);
    elem->next = blkd_addr->head_dst_elem;
    blkd_addr->head_dst_elem = elem;
    }
}
/*-------------------------------------------------------------------------*/

/****************************************************************************/
static void process_blkd_tx(t_all_ctx *all_ctx, t_blkd_addr *blkd_addr,
                            int len, char *payload) 
{
  t_blkd *bd;
  int llid_tab[1];
  if (msg_exist_channel(all_ctx, blkd_addr->llid_virtio, __FUNCTION__))
    {
    llid_tab[0] = blkd_addr->llid_virtio;
    bd = blkd_create_tx_empty(0,0,0);
    memcpy(bd->payload_blkd, payload, len);
    bd->payload_len = len;
    blkd_put_tx((void *) all_ctx, 1, llid_tab, bd);
    }
  else
    {
    KERR("%d %d", blkd_addr->idx, len);
    free_blkd_addr(all_ctx, blkd_addr->idx);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void process_host_msg(char *msg, char *resp)
{
  KERR("%s", msg);
  strcpy(resp, "OKOKOK");  
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void msg_from_host(t_all_ctx *all_ctx, t_blkd_addr *blkd_addr,
                          int len, char *msg)
{
  char name[MAX_NAME_LEN];
  char resp[MAX_LEN_PRINT];
  char headtxt[MAX_LEN_PRINT];
  char *ptr;
  int num, srv_llid, srv_tid, cli_llid, cli_tid, payload_len;
  t_header_wlan *payload;
  if (sscanf(msg, WLAN_HEADER_FORMAT, name, &num, 
                                      &srv_llid, &srv_tid,
                                      &cli_llid, &cli_tid) != 6)
    KERR("%s", msg);
  else if (!strstr(msg, "end_wlan_head"))
    KERR("%s", msg);
  else
    { 
    ptr = strstr(msg, "end_wlan_head")+strlen("end_wlan_head");
    process_host_msg(ptr, resp);
    memset(headtxt, 0, MAX_LEN_PRINT);
    snprintf(headtxt, MAX_LEN_PRINT-1, WLAN_HEADER_FORMAT,
             name, num, srv_llid, srv_tid, cli_llid, cli_tid);
    payload_len = sizeof(t_header_wlan) + strlen(headtxt) + strlen(resp) + 1;
    payload = (t_header_wlan *) malloc(payload_len);
    memset(payload, 0, payload_len);
    payload->wlan_header_type = wlan_header_type_guest_host;
    payload->data_len = payload_len;
    sprintf(payload->data, "%s%s", headtxt, resp);
    process_blkd_tx(all_ctx, blkd_addr, payload_len, (char *) payload);
    free(payload);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void process_blkd_rx(t_all_ctx *all_ctx, t_blkd_addr *blkd_addr, 
                            int len, t_header_wlan *header_wlan)
{
  struct ieee80211_hdr *hdr;
  uint8_t *dst;
  uint8_t *hwaddr = blkd_addr->our_hwaddr;
  if (header_wlan->wlan_header_type == wlan_header_type_peer_sig_addr)
    {
    if (len == sizeof(t_header_wlan))
      add_dst_elem(blkd_addr, header_wlan->src);
    else
      KERR("%d %d", len, (int)sizeof(t_header_wlan));
    }
  else if (header_wlan->wlan_header_type == wlan_header_type_peer_data)
    {
    if (!main_fd_hwsim_ok())
      KERR(" ");
    else
      {
      hdr = (void *) header_wlan->data;
      dst = hdr->addr1;
      if (((dst[0] & 1) ||
           (memcmp(blkd_addr->our_addr, dst, ETH_ALEN) == 0)) &&
           (blkd_addr->ready_to_tx == 1))
         {
         hwsim_nl_send_packet_to_air(hwaddr, header_wlan->data_len, 
                                     header_wlan->data);
         }
      }
    }
  else if (header_wlan->wlan_header_type == wlan_header_type_guest_host)
    {
    msg_from_host(all_ctx, blkd_addr,header_wlan->data_len,header_wlan->data);
    }
  else
    {
    KERR("%d", (int) (header_wlan->wlan_header_type & 0xff));
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_blkd_virtio(void *ptr, int llid)
{
  t_blkd *blkd;
  t_blkd_addr *blkd_addr;
  t_header_wlan *header_wlan;
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  int len;
  if (ptr && llid)
    {
    blkd_addr = g_llid_virtio[llid];
    if (!blkd_addr)
      KERR("%d", llid);
    else
      {
      blkd = blkd_get_rx(ptr, llid);
      while(blkd)
        {
        len = blkd->payload_len;
        header_wlan = (t_header_wlan *)blkd->payload_blkd;
        if (len < sizeof(t_header_wlan))
          KOUT("%d", len);
        process_blkd_rx(all_ctx, blkd_addr, len, header_wlan);
        blkd_free(ptr, blkd);
        blkd = blkd_get_rx(ptr, llid);
        }
      }
    }
  else
    KOUT(" ");
}
/*-------------------------------------------------------------------------*/

/*****************************************************************************/
static void err_blkd_virtio(void *ptr, int llid, int err, int from)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  t_blkd_addr *blkd_addr;
  if (ptr && llid)
    {
    blkd_addr = g_llid_virtio[llid];
    if (!blkd_addr)
      KERR("%d", llid);
    else
      {
      if (blkd_addr->llid_virtio != llid)
        KOUT(" ");
      free_blkd_addr(all_ctx, blkd_addr->idx);
      }
    }
  else
    KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void blkd_addr_tx_virtio(int idx, int len, char *payload)
{
  t_all_ctx *all_ctx = get_all_ctx();
  t_blkd_addr *blkd_addr;
  if ((len < sizeof(t_header_wlan)) || (len > PAYLOAD_BLKD_SIZE))
    KOUT("%d %d %d", len, (int)sizeof(t_header_wlan), PAYLOAD_BLKD_SIZE);
  else
    {
    if ((idx < 0) || (idx >= MAX_NB_IDX))
      KOUT("%d", idx);
    blkd_addr = g_blkd_addr[idx];
    if (blkd_addr)
      {
      process_blkd_tx(all_ctx, blkd_addr, len, payload); 
      }
    else
      {
      KERR("%d %d", idx, len);
      }
    }
}
/*-------------------------------------------------------------------------*/

/*****************************************************************************/
int blkd_addr_get_dst(uint8_t *hwaddr, uint8_t *addr, uint8_t *dst, int *idx)
{
  int i, result = -1;
  t_blkd_addr *blkd_addr;
  char str1[MAX_LEN_PRINT];
  char str2[MAX_LEN_PRINT];
  for (i=0;  i<MAX_NB_IDX; i++)
    {
    blkd_addr = g_blkd_addr[i];
    if (!blkd_addr)
      break;
    if (!memcmp(blkd_addr->our_hwaddr, hwaddr, ETH_ALEN))
      {
      if (memcmp(blkd_addr->our_addr, addr, ETH_ALEN))
        {
        blkd_addr_print(str1, addr);
        blkd_addr_print(str2, blkd_addr->our_addr);
        KERR("addr:%s our_addr:%s", str1, str2);
        }
      else
        {
        if ((blkd_addr->llid_virtio > 0) &&
            ((dst[0] & 1) || find_dst_in_chain(blkd_addr->head_dst_elem,dst)))
          {
          *idx = i;
          result = 0;
          break;
          }
        }
      }
    }
  return result;
}
/*-------------------------------------------------------------------------*/

/*****************************************************************************/
void blkd_addr_heartbeat(void)
{
  t_all_ctx *all_ctx = get_all_ctx();
  int i, fd, llid, len;
  char virtioport[MAX_LEN_PRINT];
  t_header_wlan header_wlan;
  t_blkd_addr *blkd_addr;
  for (i=0;  i<MAX_NB_IDX; i++)
    {
    blkd_addr = g_blkd_addr[i];
    if (!blkd_addr)
      break;
    if (blkd_addr->fd_virtio == -1)
      {
      memset(virtioport, 0, MAX_LEN_PRINT);
      snprintf(virtioport, MAX_LEN_PRINT-1, VIRTIOPORT, (i+1));
      fd = open(virtioport, O_RDWR);
      if (fd >= 0)
        {
        llid = blkd_watch_fd((void *) all_ctx, virtioport, fd,
                              rx_blkd_virtio, err_blkd_virtio);
        if (llid > 0)
          {
          if (llid >= CLOWNIX_MAX_CHANNELS)
            KOUT("%d", llid);
          blkd_addr->fd_virtio = fd;
          blkd_addr->llid_virtio = llid;
          g_llid_virtio[llid] = blkd_addr;
          }
        else
          KERR("Error Virtio port blkd %s", virtioport);
        }
      }
    else
      {
      len = (int) sizeof(t_header_wlan);
      memset(&header_wlan, 0, len);
      header_wlan.wlan_header_type = wlan_header_type_peer_sig_addr;
      memcpy(header_wlan.src, blkd_addr->our_addr, ETH_ALEN);
      process_blkd_tx(all_ctx, blkd_addr, len, (char *)&header_wlan);
      }
    }
}
/*-------------------------------------------------------------------------*/

/*****************************************************************************/
void blkd_addr_hwaddr_rx(uint8_t *our_hwaddr, uint8_t *our_addr)
{
  char str1[MAX_LEN_PRINT];
  char str2[MAX_LEN_PRINT];
  int i, found = 0;
  t_blkd_addr *blkd_addr;
  for (i=0;  i<MAX_NB_IDX; i++)
    {
    blkd_addr = g_blkd_addr[i];
    if (!blkd_addr)
      break;
    if ((!memcmp(blkd_addr->our_hwaddr, our_hwaddr, ETH_ALEN)) &&
        (!memcmp(blkd_addr->our_addr, our_addr, ETH_ALEN)))
      {
      found = 1;
      if (blkd_addr->ready_to_tx == 0)
        blkd_addr->ready_to_tx = 1;
      }
    }
  if (!found)
    {
    blkd_addr_print(str1, our_hwaddr);
    blkd_addr_print(str2, our_addr);
    KERR("hwaddr:%s addr:%s", str1, str2);
    }
}
/*-------------------------------------------------------------------------*/

/*****************************************************************************/
int blkd_addr_init(int nb_idx)
{
  int i, result = 0;
  uint8_t *addr;
  char intf[MAX_NAME_LEN];
  char str[MAX_LEN_PRINT];

  if ((nb_idx<1) || (nb_idx>MAX_NB_IDX))
    KOUT("%d", nb_idx); 
  memset(g_blkd_addr, 0, MAX_NB_IDX * sizeof(t_blkd_addr));
  for (i=0;  i<nb_idx; i++)
    {
    memset(intf, 0, MAX_NAME_LEN);
    snprintf(intf, MAX_NAME_LEN-1, "wlan%d", i);
    addr = iface_get_mac(intf);
    if (!addr) 
      {
      result = -1;
      KERR("%s", intf);
      break;
      }
    g_blkd_addr[i] = (t_blkd_addr *) malloc(sizeof(t_blkd_addr));
    memset(g_blkd_addr[i], 0, sizeof(t_blkd_addr));
    g_blkd_addr[i]->idx = i;
    g_blkd_addr[i]->fd_virtio = -1;
    g_blkd_addr[i]->llid_virtio = 0;
    g_blkd_addr[i]->ready_to_tx = 0;
    memcpy(g_blkd_addr[i]->our_addr, addr, ETH_ALEN);
    blkd_addr_print(str, g_blkd_addr[i]->our_addr);
    g_blkd_addr[i]->our_hwaddr[0] = 0x42;
    g_blkd_addr[i]->our_hwaddr[3] = i >> 8;
    g_blkd_addr[i]->our_hwaddr[4] = i;
    KERR("%s addr:%s", intf, str);
    }
  return result;
}
/*-------------------------------------------------------------------------*/
