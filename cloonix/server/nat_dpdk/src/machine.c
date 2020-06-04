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
#include <string.h>
#include <stdint.h>
#include <errno.h>

#define ALLOW_EXPERIMENTAL_API
#include <rte_compat.h>
#include <rte_bus_pci.h>
#include <rte_config.h>
#include <rte_cycles.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_flow.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_meter.h>
#include <rte_pci.h>
#include <rte_version.h>
#include <rte_vhost.h>
#include <rte_arp.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "utils.h"

typedef struct t_machine
{
  uint8_t mac[6];
  char name[MAX_NAME_LEN];
  int num;
  int to_be_deleted;
  struct t_machine *prev;
  struct t_machine *next;
} t_machine;

typedef struct t_arp_ip
{
  uint8_t mac[6];
  uint32_t ip;
  struct t_arp_ip *prev;
  struct t_arp_ip *next;
} t_arp_ip;


static t_machine *g_head_machine;
static t_arp_ip *g_head_arp_ip;
static int g_offset;
static uint32_t g_gw_ip;

/*****************************************************************************/
static int get_next_offset(void)
{
  int result = g_offset;
  g_offset += 1; 
  if (g_offset >= 100)
    g_offset = 1;
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static t_machine *find_machine(uint8_t *mac)
{
  t_machine *cur = g_head_machine;
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
static void machine_free(t_machine *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_machine)
    g_head_machine = cur->next;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_arp_ip *find_arp_ip(uint8_t *mac)
{
  t_arp_ip *cur = g_head_arp_ip;
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
void machine_begin(void)
{
  t_machine *cur = g_head_machine;
  while(cur)
    {
    cur->to_be_deleted = 1;
    cur = cur->next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void machine_add(char *name, int num, uint8_t *mac)
{
  t_machine *cur = find_machine(mac);
  if (cur)
    cur->to_be_deleted = 0;
  else
    {
    cur = (t_machine *) malloc(sizeof(t_machine));
    memset(cur, 0, sizeof(t_machine));
    memcpy(cur->mac, mac, 6);
    strncpy(cur->name, name, MAX_NAME_LEN-1);
    cur->num = num;
    if (g_head_machine)
      g_head_machine->prev = cur;
    cur->next = g_head_machine;
    g_head_machine = cur;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void machine_end(void)
{
  t_machine *next, *cur = g_head_machine;
  while(cur)
    {
    next = cur->next;
    if (cur->to_be_deleted == 1)
      machine_free(cur);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
uint32_t machine_dhcp(uint8_t *mac)
{
  t_arp_ip *cur = find_arp_ip(mac);
  t_machine *vm = find_machine(mac);
  uint32_t result;
  int offset;
  if (!vm)
    KERR("%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2],
                                          mac[3], mac[4], mac[5]);
  if (cur)
    result = cur->ip;
  else
    {
    offset = get_next_offset();
    result = g_gw_ip;
    result += offset + 8;
    cur = (t_arp_ip *) malloc(sizeof(t_arp_ip));
    memset(cur, 0, sizeof(t_arp_ip));
    memcpy(cur->mac, mac, 6);
    cur->ip = result;
    if (g_head_arp_ip)
      g_head_arp_ip->prev = cur;
    cur->next = g_head_arp_ip;
    g_head_arp_ip = cur;
    }
  return result;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void machine_init(void)
{
  g_head_machine = NULL;
  g_head_arp_ip  = NULL;
  g_offset = 0;
  g_gw_ip = utils_get_gw_ip();
}
/*--------------------------------------------------------------------------*/
