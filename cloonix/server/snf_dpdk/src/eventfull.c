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
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "cirspy.h"

void send_eventfull_tx_rx(char *name, int num, int ms,
                          int ptx, int btx, int prx, int brx);

/*--------------------------------------------------------------------------*/
typedef struct t_evt
{
  char name[MAX_NAME_LEN];
  int num;
  uint8_t mac[6];
  int pkt_tx;
  int bytes_tx;
  int pkt_rx;
  int bytes_rx;
  int to_be_erased;
  struct t_evt *prev;
  struct t_evt *next;
} t_evt;  
/*--------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
static t_evt *g_head_evt;


/****************************************************************************/
static t_evt *find_evt(char *name, int num)
{
  t_evt *cur = g_head_evt;
  while(cur)
    {
    if ((!strcmp(cur->name, name)) && (cur->num == num))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_evt *get_evt_with_mac(uint8_t *mac)
{
  t_evt *cur = g_head_evt;
  while(cur)
    {
    if (!memcmp(cur->mac, mac, 6))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void eventfull_free_evt(t_evt *cur)
{
  while (cur)
    {
    if (cur->prev) 
      cur->prev->next = cur->next;
    if (cur->next) 
      cur->next->prev = cur->prev;
    if (cur == g_head_evt)
      g_head_evt = cur->next;
    free(cur);
    cur = cur->next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void eventfull_alloc_evt(char *name, int num, uint8_t *mac)
{
  t_evt *cur = malloc(sizeof(t_evt));
  if (cur == NULL)
    KERR(" ");
  else
    {
    memset(cur, 0, sizeof(t_evt));
    memcpy(cur->name, name, MAX_NAME_LEN-1);
    cur->num = num;
    memcpy(cur->mac, mac, 6);
    if (g_head_evt)
      g_head_evt->prev = cur;
    cur->next = g_head_evt;
    g_head_evt = cur;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void eventfull_can_be_sent(void *data)
{
  t_evt *cur = g_head_evt;
  unsigned int ms = (unsigned int) cloonix_get_msec();
  while(cur)
    {
    send_eventfull_tx_rx(cur->name, cur->num, ms,
                         cur->pkt_tx, cur->bytes_tx,
                         cur->pkt_rx, cur->bytes_rx);
    cur->pkt_tx   = 0;
    cur->bytes_tx = 0;
    cur->pkt_rx   = 0;
    cur->bytes_rx = 0;
    cur = cur->next;
    }
  clownix_timeout_add(5, eventfull_can_be_sent, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void eventfull_hook_spy(int len, uint8_t *buf)
{
  int flag_allcast = buf[0] & 0x01;
  t_evt *src, *dst = NULL;
  if (!flag_allcast)
    dst = get_evt_with_mac(&(buf[0]));
  src = get_evt_with_mac(&(buf[6]));
  if (src != NULL) 
    {
    src->pkt_tx += 1;
    src->bytes_tx += len;
    }
  if (dst != NULL)
    {
    dst->pkt_rx += 1;
    dst->bytes_rx += len;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void eventfull_obj_update_begin(void)
{
  t_evt *cur = g_head_evt;
  while(cur)
    {
    cur->to_be_erased = 1;
    cur = cur->next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void eventfull_obj_update_item(char *name, int num, char *strmac)
{
  int i, imac[6];
  uint8_t mac[6];
  t_evt *cur = find_evt(name, num);
  if (cur)
    cur->to_be_erased = 0;
  else
    {
    if (sscanf(strmac, "%02x:%02x:%02x:%02x:%02x:%02x", 
        &(imac[0]), &(imac[1]), &(imac[2]),
        &(imac[3]), &(imac[4]), &(imac[5])) != 6) 
      KERR("%s %d %s", name, num, strmac);
    else
      {
      for (i=0; i<6; i++)
        mac[i] = (uint8_t)(imac[i] & 0xFF); 
      eventfull_alloc_evt(name, num, mac);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void eventfull_obj_update_end(void)
{
  t_evt *next, *cur = g_head_evt;
  while(cur)
    {
    next = cur->next;
    if (cur->to_be_erased == 1)
      eventfull_free_evt(cur);
    cur = next;
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
  g_head_evt = NULL;
}
/*--------------------------------------------------------------------------*/
