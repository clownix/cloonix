/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "glob_common.h"
#include "machine.h"

static void *utils_malloc(int len) { return malloc(len); }

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

static void machine_free(t_machine *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_machine)
    g_head_machine = cur->next;
}

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

/* The ip a machine uses, seen in its frames on the way to passt. */
void machine_learn_ip(uint8_t *mac, uint32_t ip)
{
  t_arp_ip *cur = find_arp_ip(mac);
  if (cur == NULL)
    {
    cur = (t_arp_ip *) utils_malloc(sizeof(t_arp_ip));
    if (cur == NULL)
      KOUT("ERROR MALLOC");
    memset(cur, 0, sizeof(t_arp_ip));
    memcpy(cur->mac, mac, 6);
    if (g_head_arp_ip)
      g_head_arp_ip->prev = cur;
    cur->next = g_head_arp_ip;
    g_head_arp_ip = cur;
    }
  cur->ip = ip;
}

/* The mac a known ip belongs to: 0 with it copied into mac, -1 unknown. */
int machine_mac_of_ip(uint32_t ip, uint8_t *mac)
{
  t_arp_ip *cur = g_head_arp_ip;
  while((cur) && (cur->ip != ip))
    cur = cur->next;
  if (cur == NULL)
    return -1;
  memcpy(mac, cur->mac, 6);
  return 0;
}

/* The (mac, ip) pairs known, at most max of them. */
int machine_known(uint8_t *macs, uint32_t *ips, int max)
{
  int nb = 0;
  t_arp_ip *cur = g_head_arp_ip;
  while((cur) && (nb < max))
    {
    memcpy(macs + 6*nb, cur->mac, 6);
    ips[nb] = cur->ip;
    nb += 1;
    cur = cur->next;
    }
  return nb;
}

void machine_begin(void)
{
  t_machine *cur = g_head_machine;
  while(cur)
    {
    cur->to_be_deleted = 1;
    cur = cur->next;
    }
}

uint32_t machine_ip_get(char *name)
{
  uint32_t result = 0;
  t_arp_ip *arp;
  t_machine *cur = g_head_machine;
  while(cur)
    {
    if (!strcmp(cur->name, name))
      {
      arp = find_arp_ip(cur->mac);
      if (arp)
        {
        result = arp->ip;
        break;
        }
      }
    cur = cur->next;
    }
  return result;
}

void machine_add(char *name, int num, uint8_t *mac)
{
  t_machine *cur = find_machine(mac);
  if (cur)
    cur->to_be_deleted = 0;
  else
    {
    cur = (t_machine *) utils_malloc(sizeof(t_machine));
    if (cur == NULL)
      KOUT("ERROR MALLOC");
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

