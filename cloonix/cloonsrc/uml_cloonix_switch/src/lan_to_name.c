/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_daemon.h"
#include "msg.h"
#include "lan_to_name.h"

typedef struct t_item_el
{
  int item_type;
  char item_name[MAX_NAME_LEN];
  int item_num;
  struct t_item_el *prev;
  struct t_item_el *next;
} t_item_el;

typedef struct t_bij_elem
{
  char name[MAX_NAME_LEN];
  int idx;
  int nb_users;
  /* rstp on, for dump commands; gone with the lan */
  int rstp;
  t_item_el *head_item;
} t_bij_elem;
static int glob_total_elems;
static t_bij_elem *tab_elems[MAX_LAN];

static int get_hash_of_id(int len, void *ident)
{
  int i, result;
  short *short_ident = (short *) ident;
  char *char_ident;
  short last;
  short hash = 0;
  if (len < 1)
    KOUT(" ");
  else if (len == 1)
    {
    char_ident = (char *) ident;
    result = (*char_ident & 0x3FFF) + 1;
    }
  else
    {
    for (i=0; i<(len/2); i++)
      hash ^= short_ident[i];
    if (len%2)
      {
      char_ident = (char *) ident;
      last = (short) (char_ident[len - 1] & 0xFF);
      hash ^= last;
      }
    result = (hash & 0x3FFF) + 1;
    }
  return result;
}

static int find_elem_with_name(char *name)
{
  int result = 0;
  int i, index, idx;
  idx = get_hash_of_id(strlen(name), (void *)name);
  for (i = idx; i < idx + MAX_LAN; i++)
    {
    index = i;
    if (index >= MAX_LAN)
      {
      index -= MAX_LAN;
      if (index == 0)
        index = 1;
      }
    if ((index<=0) || (index>=MAX_LAN))
      KOUT(" ");
    if ((tab_elems[index]) &&
        (!strcmp(tab_elems[index]->name, name)))
      {
      result = index;  
      break;
      }
    }
  return result;
}

static int find_free_idx_for_name(char *name)
{
  int idx, count = 0;
  if ((!name) || (!name[0]) || (strlen(name) >= MAX_NAME_LEN))
    KOUT(" ");
  idx = get_hash_of_id(strlen(name), (void *)name);
  while(tab_elems[idx])
    {
    count++;
    if (count == MAX_LAN)
      KOUT(" ");
    idx++;
    if (idx == MAX_LAN)
      idx = 1;
    }
  return idx;
}

static void add_item_el(t_bij_elem *target, int item_type,
                        char *item_name, int item_num)
{
  t_item_el *cur = (t_item_el *) malloc(sizeof(t_item_el));
  memset(cur, 0, sizeof(t_item_el));
  cur->item_type = item_type;
  strncpy(cur->item_name, item_name, MAX_NAME_LEN-1); 
  cur->item_num = item_num;
  if (target->head_item)
    target->head_item->prev = cur;
  cur->next = target->head_item;
  target->head_item = cur;
}

#define NB_LAN_HISTORY 64

typedef struct t_lan_event
{
  char name[MAX_NAME_LEN];
  char what[24];
  int users;
  unsigned long at_ms;
} t_lan_event;

static t_lan_event g_lan_history[NB_LAN_HISTORY];
static int g_lan_history_idx;

static void lan_history_add(char *name, char *what, int nb_users)
{
  t_lan_event *ev = &(g_lan_history[g_lan_history_idx]);
  memset(ev, 0, sizeof(t_lan_event));
  strncpy(ev->name, name, MAX_NAME_LEN-1);
  strncpy(ev->what, what, sizeof(ev->what)-1);
  ev->users = nb_users;
  ev->at_ms = cloonix_now_ms();
  g_lan_history_idx = (g_lan_history_idx + 1) % NB_LAN_HISTORY;
}

int lan_history_state(char *name)
{
  int i, idx, result = LAN_HISTORY_NOTHING;
  for (i=0; i<NB_LAN_HISTORY; i++)
    {
    idx = (g_lan_history_idx + i) % NB_LAN_HISTORY;
    if (!g_lan_history[idx].name[0])
      continue;
    if (strcmp(g_lan_history[idx].name, name))
      continue;
    if (!strcmp(g_lan_history[idx].what, "DIED"))
      result = LAN_HISTORY_DIED;
    else if ((result == LAN_HISTORY_DIED) &&
             (!strcmp(g_lan_history[idx].what, "BORN")))
      result = LAN_HISTORY_REUSED;
    }
  return result;
}

void lan_history_dump(char *name)
{
  unsigned long now = cloonix_now_ms();
  int i, idx, passed = 0, nb = 0;
  for (i=0; i<NB_LAN_HISTORY; i++)
    {
    idx = (g_lan_history_idx + i) % NB_LAN_HISTORY;
    if (!g_lan_history[idx].name[0])
      continue;
    if (strcmp(g_lan_history[idx].name, name))
      continue;
    /* one block: the first line is gated, the others follow it */
    if (nb == 0)
      KERR_HEAD(passed, "WARNING lan %s history: %s (users=%d) %lu ms ago",
                name, g_lan_history[idx].what, g_lan_history[idx].users,
                now - g_lan_history[idx].at_ms);
    else
      KERR_MORE(passed, "WARNING lan %s history: %s (users=%d) %lu ms ago",
                name, g_lan_history[idx].what, g_lan_history[idx].users,
                now - g_lan_history[idx].at_ms);
    nb += 1;
    }
  if (!nb)
    KERR("WARNING lan %s history: nothing in the last %d lan events",
         name, NB_LAN_HISTORY);
}

int lan_add_name(char *name, int item_type, char *item_name, int item_num)
{
  int idx;
  t_bij_elem *target;
  if ((!name) || (!name[0]) || (strlen(name) >= MAX_NAME_LEN))
    KOUT(" ");
  idx = find_elem_with_name(name);
  if (!idx)
    {
    idx = find_free_idx_for_name(name);
    target = (t_bij_elem *) clownix_malloc(sizeof(t_bij_elem), 20);
    memset(target, 0, sizeof(t_bij_elem));
    target->idx = idx;
    strcpy(target->name, name);
    tab_elems[idx] = target;
    glob_total_elems++;
    if (glob_total_elems >= MAX_LAN)
      KOUT(" ");
    msg_lan_add_name(name);
    lan_history_add(name, "BORN", 0);
    add_item_el(target, item_type, item_name, item_num);
    }
  else
    {
    target = tab_elems[idx];
    add_item_el(target, item_type, item_name, item_num);
    }
  target->nb_users += 1;
  lan_history_add(name, "gained a user", target->nb_users);
  return idx;
}

int lan_del_name(char *name, int item_type, char *item_name, int item_num)
{
  int target_idx;
  if ((!name) || (!name[0]) || (strlen(name) >= MAX_NAME_LEN))
    KOUT(" ");
  target_idx = find_elem_with_name(name);
  if (target_idx)
    {
    if (tab_elems[target_idx]->nb_users <= 0)
      KOUT(" ");
    tab_elems[target_idx]->nb_users -= 1;
    lan_history_add(name, "lost a user", tab_elems[target_idx]->nb_users);
    if (tab_elems[target_idx]->nb_users == 0)
      {
      lan_history_add(name, "DIED", 0);
      clownix_free(tab_elems[target_idx], __FUNCTION__);
      tab_elems[target_idx] = NULL;
      glob_total_elems--;
      if (glob_total_elems < 0)
        KOUT(" ");
      msg_lan_del_name(name);
      target_idx = 2*MAX_LAN;
      }
    }
  return target_idx;
}

int lan_get_with_name(char *name)
{
  int target_idx;
  if ((!name) || (!name[0]) || (strlen(name) >= MAX_NAME_LEN))
    KOUT("%p", name);
  target_idx = find_elem_with_name(name);
  return target_idx;
}

void lan_rstp_set(char *name, int val)
{
  int idx = find_elem_with_name(name);
  if (idx)
    tab_elems[idx]->rstp = val;
}

/* the lans running rstp */
int lan_rstp_names(char names[][MAX_NAME_LEN], int max)
{
  int i, nb = 0;
  for (i = 0; (i < MAX_LAN) && (nb < max); i++)
    if ((tab_elems[i]) && (tab_elems[i]->rstp))
      {
      memset(names[nb], 0, MAX_NAME_LEN);
      strncpy(names[nb], tab_elems[i]->name, MAX_NAME_LEN-1);
      nb += 1;
      }
  return nb;
}

void lan_init(void)
{
  glob_total_elems = 0;
  memset(tab_elems, 0, MAX_LAN * sizeof(t_bij_elem *));
}

