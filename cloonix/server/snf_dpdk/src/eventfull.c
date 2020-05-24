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
#include <stdint.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "cirspy.h"

void send_eventfull_tx_rx(char *name, int num, int ms,
                          int ptx, int btx, int prx, int brx);

/****************************************************************************/
typedef struct t_evt_eth
{
  int eth;
  char mac[ETH_ALEN];
  int pkt_tx;
  int bytes_tx;
  int pkt_rx;
  int bytes_rx;
  struct t_evt_eth *prev;
  struct t_evt_eth *next;
} t_evt_eth;
/*--------------------------------------------------------------------------*/
typedef struct t_evt_obj
{
  char name[MAX_NAME_LEN];
  t_evt_eth *head_evt_eth;
  int to_be_erased;
  struct t_evt_obj *prev;
  struct t_evt_obj *next;
} t_evt_obj;  
/*--------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
static t_evt_obj *g_obj_head;


/****************************************************************************/
static t_evt_obj *find_evt_obj(char *name)
{
  t_evt_obj *cur = g_obj_head;
  while(cur)
    {
    if (!strcmp(cur->name, name))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_evt_eth *get_eth_with_mac(char *mac)
{
  t_evt_obj *obj = g_obj_head;
  t_evt_eth *cur = NULL;
  char *mc;
  int result = 0;
  while((obj) && (result == 0))
    {
    cur = obj->head_evt_eth;
    while(cur)
      {
      mc = cur->mac;
      if (!memcmp(mc, mac, 6))
        {
        result = 1;
        break;
        }
      cur = cur->next;
      }
    obj = obj->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void update_stats(int flag_allcast, t_evt_eth *src,
                         t_evt_eth *dst, int len)
{
  t_evt_eth *cur = NULL;
  src->pkt_tx += 1;
  src->bytes_tx += len;
  if (flag_allcast)
    {
    while (cur)
      {
      if (cur != src)
        {
        cur->pkt_rx += 1;
        cur->bytes_rx += len;
        } 
      cur = cur->next;
      }
    }
  else
    {
    dst->pkt_rx += 1;
    dst->bytes_rx += len;
    } 
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void add_chain_eth(t_evt_obj *vme, int num, char *mac)
{
  t_evt_eth *cur;
  cur = (t_evt_eth *) malloc(sizeof(t_evt_eth));
  memset(cur, 0, sizeof(t_evt_eth));
  cur->eth = num;
  memcpy(cur->mac, mac, ETH_ALEN);
  if (vme->head_evt_eth)
    vme->head_evt_eth->prev = cur;
  cur->next = vme->head_evt_eth;
  vme->head_evt_eth = cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void delete_chain_eth(t_evt_obj *vme)
{
  t_evt_eth *cur;
  cur = vme->head_evt_eth;
  while (cur)
    {
    if (cur->prev) 
      cur->prev->next = cur->next;
    if (cur->next) 
      cur->next->prev = cur->prev;
    if (cur==vme->head_evt_eth)
      vme->head_evt_eth = cur->next;
    cur = vme->head_evt_eth;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void eventfull_can_be_sent(void *data)
{
  t_evt_obj *obj = g_obj_head;
  t_evt_eth *cur;
  unsigned int ms = (unsigned int) cloonix_get_msec();
  while(obj)
    {
    cur = obj->head_evt_eth;
    while(cur)
      {
      send_eventfull_tx_rx(obj->name, cur->eth, ms,
                           cur->pkt_tx, cur->bytes_tx,
                           cur->pkt_rx, cur->bytes_rx);
      cur->pkt_tx   = 0;
      cur->bytes_tx = 0;
      cur->pkt_rx   = 0;
      cur->bytes_rx = 0;
      cur = cur->next;
      }
    obj = obj->next;
    }
  clownix_timeout_add(5, eventfull_can_be_sent, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void eventfull_obj_add(char *name, int num, char *mac)
{
  t_evt_obj *cur = find_evt_obj(name);
  if (cur)
    {
    KERR("ADD OBJ %s %d More than a single eth?", name, num);
    add_chain_eth(cur, num, mac);
    }
  else
    {
    KERR("EVENT ADD OBJ %s %d %02x:%02x:%02x:%02x:%02x:%02x", name, num,
                       (mac[0]&0xff), (mac[1]&0xff),(mac[2]&0xff),
                       (mac[3]&0xff),(mac[4]&0xff),(mac[5]&0xff));
    cur = malloc(sizeof(t_evt_obj));
    memset(cur, 0, sizeof(t_evt_obj));
    memcpy(cur->name, name, MAX_NAME_LEN-1);
    add_chain_eth(cur, num, mac);
    if (g_obj_head) 
      g_obj_head->prev = cur;
    cur->next = g_obj_head;
    g_obj_head = cur;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void eventfull_obj_del(char *name)
{
  t_evt_obj *cur = find_evt_obj(name);
  if (!cur)
    KERR("%s", name);
  else
    {
    KERR("EVENT DEL OBJ %s", name);
    delete_chain_eth(cur);
    if (cur->prev) 
      cur->prev->next = cur->next;
    if (cur->next) 
      cur->next->prev = cur->prev;
    if (cur==g_obj_head) 
      g_obj_head = cur->next;
    free(cur);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void eventfull_hook_spy(int len, char *buf)
{
  int flag_allcast = buf[0] & 0x01;
  t_evt_eth *src, *dst = NULL;
  if (!flag_allcast)
    {
    dst = get_eth_with_mac(&(buf[0]));
    }
  src = get_eth_with_mac(&(buf[6]));
  if ((src != NULL) && ( (dst != NULL) || flag_allcast != 0)) 
    update_stats(flag_allcast, src, dst, len);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void eventfull_obj_update_begin(void)
{
  t_evt_obj *obj = g_obj_head;
  while(obj)
    {
    obj->to_be_erased = 1;
    obj = obj->next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void eventfull_obj_update_item(char *name, int num, char *strmac)
{
  int i, imac[6];
  char mac[6];
  t_evt_obj *obj = find_evt_obj(name);
  if (obj)
    obj->to_be_erased = 0;
  else
    {
    if (sscanf(strmac, "%02x:%02x:%02x:%02x:%02x:%02x", 
        &(imac[0]), &(imac[1]),&(imac[2]),
        &(imac[3]),&(imac[4]),&(imac[5])) != 6) 
      KERR("%s %d %s", name, num, strmac);
    else
      {
      for (i=0; i<6; i++)
        mac[i] = (char )(imac[i] & 0xFF); 
      eventfull_obj_add(name, num, mac);
      }
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void eventfull_obj_update_end(void)
{
  t_evt_obj *nobj, *obj = g_obj_head;
  while(obj)
    {
    nobj = obj->next;
    if (obj->to_be_erased == 1)
      eventfull_obj_del(obj->name);
    obj = nobj;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void eventfull_init_start(void)
{
  clownix_timeout_add(200, eventfull_can_be_sent, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void eventfull_init(void)
{
  g_obj_head = NULL;
}
/*--------------------------------------------------------------------------*/
