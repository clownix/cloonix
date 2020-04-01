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
#include "io_clownix.h"
#include "commun_daemon.h"
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "event_subscriber.h"
#include "llid_trace.h"
#include "hop_event.h"
#include "doorways_mngt.h"
#include "mulan_mngt.h"
#include "endp_mngt.h"
#include "dpdk_ovs.h"


/*---------------------------------------------------------------------------*/
typedef struct t_flags_hop_clients
{
  int llid;
  int tid;
  int flags_hop;
  struct t_flags_hop_clients *prev;
  struct t_flags_hop_clients *next;
}t_flags_hop_clients;
/*---------------------------------------------------------------------------*/
struct t_hop_record;
typedef struct t_clients
{
  int llid;
  int tid;
  int flags_hop;
  struct t_hop_record *hop;
  struct t_clients *prev;
  struct t_clients *next;
}t_clients;
/*---------------------------------------------------------------------------*/
typedef struct t_hop_record
{
  int type_hop;
  char name[MAX_NAME_LEN];
  int  num;
  int llid;
  int nb_client_allocated;
  t_clients *clients;
  struct t_hop_record *prev;
  struct t_hop_record *next;
} t_hop_record;
/*---------------------------------------------------------------------------*/
static t_flags_hop_clients *g_head_flags_hop_clients;
static t_hop_record *g_head_hop;
static int g_nb_hop_allocated;
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static char *hop_name(t_hop_record *hop)
{
  static char name[2*MAX_NAME_LEN];
  memset(name, 0, 2*MAX_NAME_LEN);
  snprintf(name, 2*MAX_NAME_LEN-1, "%s,%d", hop->name, hop->num);
  return name;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_flags_hop_clients *find_flags_hop_clients(int llid)
{
  t_flags_hop_clients *cur = g_head_flags_hop_clients;
  while (cur && (cur->llid != llid))
    cur = cur->next;
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void alloc_flags_hop_clients(int llid, int tid, int flags_hop)
{
  t_flags_hop_clients *cur = find_flags_hop_clients(llid);
  if (!cur)
    {
    cur = clownix_malloc(sizeof(t_flags_hop_clients), 7);
    memset(cur, 0, sizeof(t_flags_hop_clients));
    cur->llid = llid;
    cur->tid = tid;
    cur->flags_hop = flags_hop;
    if (g_head_flags_hop_clients)
      g_head_flags_hop_clients->prev = cur;
    cur->next = g_head_flags_hop_clients;
    g_head_flags_hop_clients = cur;
    event_print("HOPS %s %d", __FUNCTION__, llid);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void free_flags_hop_clients(int llid)
{
  t_flags_hop_clients *cur = find_flags_hop_clients(llid);
  if (cur)
    {
    if (cur->prev)
      cur->prev->next = cur->next;
    if (cur->next)
      cur->next->prev = cur->prev;
    if (cur == g_head_flags_hop_clients)
      g_head_flags_hop_clients = cur->next;
    clownix_free(cur, __FUNCTION__);
    event_print("HOPS %s %d", __FUNCTION__, llid);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_hop_record *get_hop_with_llid(int llid)
{
  t_hop_record *cur = g_head_hop;
  while (cur)
    {
    if (cur->llid == llid)
      break;
    cur = cur->next;
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_hop_record *get_hop_with_name(char *name, int num)
{
  t_hop_record *cur = g_head_hop;
  while (cur)
    {
    if ((!strcmp(cur->name, name)) &&
        (num == cur->num))
      break;
    cur = cur->next;
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int get_hop_name_count(void)
{
  int result = 0;
  t_hop_record *cur = g_head_hop;
  while (cur)
    {
    result++;
    cur = cur->next;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static t_clients *get_client_with_llid(t_hop_record *hop, int llid)
{
  t_clients *cur = hop->clients;
  while (cur)
    {
    if (cur->llid == llid)
      break;
    cur = cur->next;
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void free_client(t_clients *cli)
{
  char debug_line[MAX_PATH_LEN];
  t_hop_record *hop = cli->hop;
  if (cli->prev)
    cli->prev->next = cli->next;
  if (cli->next)
    cli->next->prev = cli->prev;
  if (cli == hop->clients)
    hop->clients = cli->next;
  clownix_free(cli, __FUNCTION__);
  if (hop->clients == NULL)
    {
    memset(debug_line, 0, MAX_PATH_LEN);
    snprintf(debug_line, MAX_PATH_LEN-1, "rpct_send_hop_unsub %s", hop->name);
    hop_event_hook(hop->llid, FLAG_HOP_DIAG, debug_line);
    rpct_send_hop_unsub(NULL, hop->llid, 0);
    event_print("HOPS unsubscribe from hop %s", hop->name);
    }
  hop->nb_client_allocated -= 1;
  event_print("HOPS %s %d", __FUNCTION__, hop->nb_client_allocated);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void free_all_clients(t_clients *cli)
{
  int is_last_cli;
  t_clients *cur=cli, *next, *other;
  t_hop_record *hop;
  while(cur)
    {
    hop = g_head_hop;
    is_last_cli = 1;
    next = cur->next;
    while (hop)
      {
      other = get_client_with_llid(hop, cur->llid);
      if (other)
        is_last_cli = 0;
      hop = hop->next;
      }
    if (is_last_cli)
      llid_trace_free(cur->llid, 0, __FUNCTION__);
    cur = next;
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void alloc_client(t_hop_record *hop, int llid, int tid, int flags_hop)
{
  t_clients *cli;
  char debug_line[MAX_PATH_LEN];
  memset(debug_line, 0, MAX_PATH_LEN);
  snprintf(debug_line, MAX_PATH_LEN-1, "rpct_send_hop_sub %s", hop->name);
  hop_event_hook(hop->llid, FLAG_HOP_DIAG, debug_line);
  rpct_send_hop_sub(NULL, hop->llid, 0, flags_hop);
  event_print("HOPS %s subscribing to hop %s %04X", 
              __FUNCTION__, hop->name, flags_hop);
  cli = (t_clients *)clownix_malloc(sizeof(t_clients), 7);
  memset(cli, 0, sizeof(t_clients));
  cli->llid = llid;
  cli->tid = tid;
  cli->flags_hop = flags_hop;
  cli->hop = hop;
  cli->next = hop->clients;
  if (hop->clients)
    hop->clients->prev = cli;
  hop->clients = cli;
  hop->nb_client_allocated += 1;
  event_print("HOPS %s %d", __FUNCTION__, hop->nb_client_allocated);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void alloc_hop(int llid, int type_hop, char *name, int num)
{
  t_hop_record *hop = (t_hop_record *) clownix_malloc(sizeof(t_hop_record), 7);
  memset(hop, 0, sizeof(t_hop_record));
  strncpy(hop->name, name, MAX_NAME_LEN-1);
  hop->llid = llid;
  hop->type_hop = type_hop;
  hop->num = num;
  if (g_head_hop)
    g_head_hop->prev = hop;
  hop->next = g_head_hop;
  g_head_hop = hop;
  g_nb_hop_allocated += 1;
  event_print("HOPS %s %d", __FUNCTION__, g_nb_hop_allocated);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void free_hop(t_hop_record *hop)
{
  free_all_clients(hop->clients);
  if (hop->prev)
    hop->prev->next = hop->next;
  if (hop->next)
    hop->next->prev = hop->prev;
  if (hop == g_head_hop)
    g_head_hop = hop->next;
  clownix_free(hop, __FUNCTION__);
  g_nb_hop_allocated -= 1;
  event_print("HOPS %s %d", __FUNCTION__, g_nb_hop_allocated);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void hop_evt_client_free(int llid)
{
  t_clients *cli;
  t_hop_record *cur = g_head_hop;
  while (cur)
    {
    cli = get_client_with_llid(cur, llid);
    if (cli)
      free_client(cli);
    cur = cur->next;
    }
  free_flags_hop_clients(llid);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void alloc_client_if_not_done_already(t_hop_record *hop, int llid, 
                                             int tid, int flags_hop)
{
  t_clients *cli;
  if (hop)
    {
    cli = get_client_with_llid(hop, llid);
    if (!cli)
      {
      alloc_client(hop, llid, tid, flags_hop);
      }
    }
}
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
static void update_flags_hop_clients(void)
{
  t_hop_record *hop = g_head_hop;
  t_flags_hop_clients *cur = g_head_flags_hop_clients;
  while (cur)
    {
    while (hop)
      {
      alloc_client_if_not_done_already(hop,cur->llid,cur->tid,cur->flags_hop);
      hop = hop->next;
      }
    cur = cur->next;
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_hop_evt_doors_sub(int llid, int tid, int flags_hop, 
                            int nb, t_hop_list *list)
{
  int i;
  for (i=0; i<nb; i++)
    alloc_flags_hop_clients(llid, tid, flags_hop);
  update_flags_hop_clients();
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_hop_evt_doors_unsub(int llid, int tid)
{
  hop_evt_client_free(llid);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int hop_event_alloc(int llid, int type_hop, char *name, int num)
{
  int result = -1;
  t_hop_record *hop;
  if (!llid) 
    KOUT(" ");
  hop = get_hop_with_llid(llid);
  if (hop)
    KERR("%d %s %d", llid, name, num);
  else
    {
    if (strlen(name) == 0)
      KOUT(" ");
    hop = get_hop_with_name(name, num);
    if (hop)
      KERR("%d %s %d", llid, name, num);
    else
      {
      alloc_hop(llid, type_hop, name, num);
      update_flags_hop_clients();
      result = 0;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void hop_event_free(int llid)
{
  t_hop_record *hop = get_hop_with_llid(llid);
  if (hop)
    {
    free_hop(hop);
    }
  hop_evt_client_free(llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
t_hop_list *hop_get_name_list(int *onb)
{
  t_hop_record *cur = g_head_hop;
  int nb = get_hop_name_count();
  int i = 0;
  t_hop_list *list = (t_hop_list *) clownix_malloc(nb * sizeof(t_hop_list), 7);
  memset(list, 0, nb * sizeof(t_hop_list)); 
  while (cur)
    {
    list[i].type_hop = cur->type_hop;
    list[i].num = cur->num;
    strcpy(list[i].name, cur->name);
    i++;
    cur = cur->next;
    }
  *onb = nb;
  return list;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void hop_free_name_list(t_hop_list *list)
{
  clownix_free(list, __FUNCTION__);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_hop_msg(void *ptr, int llid, int tid, int flags_hop, char *txt)
{
  t_hop_record *hop = get_hop_with_llid(llid);
  t_clients *cur_client;
  if (!hop)
    KERR("%d %s", tid, txt);
  else
    {
    cur_client = hop->clients;
    if (!cur_client)
      KERR("%d %s", tid, txt);
    else
      {
      while (cur_client)
        {
        send_hop_evt_doors(cur_client->llid, cur_client->tid, 
                           flags_hop, hop_name(hop), txt);
        cur_client = cur_client->next;
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_pid_resp(void *ptr, int llid, int tid, char *name, int num,
                        int toppid, int pid)
{
  if (tid == type_hop_doors)
    doors_pid_resp(llid, name, pid);
  else if (tid == type_hop_mulan)
    mulan_pid_resp(llid, name, pid);
  else if (tid == type_hop_endp)
    endp_mngt_pid_resp(llid, name, toppid, pid);
  else if ((tid == type_hop_ovs) || (tid == type_hop_ovsdb)) 
    dpdk_ovs_pid_resp(llid, name, toppid, pid);
  else
    KERR("%d %s %d", tid, name, pid);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void hop_event_hook(int llid, int flag, char *iline)
{
  char line[MAX_HOP_PRINT_LEN];
  t_hop_record *hop = get_hop_with_llid(llid);
  t_clients *cur_client;
  if (!hop)
    KERR("%s", iline);
  cur_client = hop->clients;
  if (cur_client)
    {
    memset(line, 0, MAX_HOP_PRINT_LEN);
    snprintf(line, MAX_HOP_PRINT_LEN-1, "%07u: %s",
             (unsigned int) cloonix_get_msec(), iline);
    while ((cur_client) && (cur_client->flags_hop & flag))
      {
      send_hop_evt_doors(cur_client->llid, cur_client->tid,
                         flag, "cloonix", line);
      cur_client = cur_client->next;
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void hop_init(void)
{
  g_head_hop = NULL;
  g_nb_hop_allocated = 0;
  g_head_flags_hop_clients = NULL;
}
/*---------------------------------------------------------------------------*/

