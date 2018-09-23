/*****************************************************************************/
/*    Copyright (C) 2006-2018 cloonix@cloonix.net License AGPL-3             */
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
#include <sys/types.h>
#include <sys/mman.h>
#include <stdint.h>

#include "ioc.h"
#include "rank_mngt.h"

typedef struct t_unix_traf_chain
{
  int llid_unix_traf;
  struct t_unix_traf_chain *prev;
  struct t_unix_traf_chain *next;
} t_unix_traf_chain; 

static t_llid_rank *g_head;
static t_unix_traf_chain *g_unix_traf_chain_head;
int g_llid_unix_sock_traf_nb;


/*****************************************************************************/
static void traf_chain_insert(int llid)
{
  t_unix_traf_chain *cur;
  if (llid <= 0)
    KOUT(" ");

  cur = g_unix_traf_chain_head;
  while (cur)
    {
    if (cur->llid_unix_traf == llid)
      break;
    cur = cur->next;
    }

  if (cur)
    KOUT(" ");
  cur = (t_unix_traf_chain *) malloc(sizeof(t_unix_traf_chain));
  memset(cur, 0, sizeof(t_unix_traf_chain));
  cur->llid_unix_traf = llid;
  if (g_unix_traf_chain_head)
    g_unix_traf_chain_head->prev = cur;
  cur->next = g_unix_traf_chain_head;
  g_unix_traf_chain_head = cur;
  g_llid_unix_sock_traf_nb += 1;

}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void traf_chain_extract(int llid)
{
  t_unix_traf_chain *cur;
  if (llid <= 0)
    KOUT(" ");

  cur = g_unix_traf_chain_head;

  while (cur)
    {
    if (cur->llid_unix_traf == llid)
      break;
    cur = cur->next;
    }
  if (!cur)
    KERR(" ");
  else
    {
    if (cur->prev)
      cur->prev->next = cur->next;
    if (cur->next)
      cur->next->prev = cur->prev;
    if (cur == g_unix_traf_chain_head)
      g_unix_traf_chain_head = cur->next;
    free(cur);
    g_llid_unix_sock_traf_nb -= 1;
    }

}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_llid_traf_tab(t_all_ctx *all_ctx, int llid,  int32_t **llid_tab)
{
  t_unix_traf_chain *next, *cur;
  int is_blkd, idx = 0;
  *llid_tab = malloc(g_llid_unix_sock_traf_nb * sizeof(int64_t));
  memset(*llid_tab, 0, g_llid_unix_sock_traf_nb * sizeof(int64_t));
  cur = g_unix_traf_chain_head;
  while (cur)
    {
    next = cur->next;
    if (cur->llid_unix_traf != llid)
      {
      if (msg_exist_channel(all_ctx,cur->llid_unix_traf,&is_blkd,__FUNCTION__))
        {
        if (idx > g_llid_unix_sock_traf_nb)
          KOUT("%d %d", idx, g_llid_unix_sock_traf_nb);
        (*llid_tab)[idx] = cur->llid_unix_traf;
        idx += 1;
        }
      }
    cur = next;
    }
  return idx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_llid_rank *alloc_llid_rank(int llid, char *name, int num, 
                                    int tidx, uint32_t rank)
{
  t_llid_rank *cur;
  cur = (t_llid_rank *) malloc(sizeof(t_llid_rank));
  memset(cur, 0, sizeof(t_llid_rank));
  cur->llid = llid;
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  cur->num = num;
  cur->tidx = tidx;
  cur->prechoice_rank = rank;
  if (g_head)
    g_head->prev = cur;
  cur->next = g_head;
  g_head = cur;
  return cur;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void free_llid_rank(t_all_ctx *all_ctx, t_llid_rank *dr)
{
  int is_blkd;
  if (msg_exist_channel(all_ctx, dr->llid, &is_blkd, __FUNCTION__))
    msg_delete_channel(all_ctx, dr->llid);
  if (dr->llid_unix_sock_traf)
    {
    if (msg_exist_channel(all_ctx,dr->llid_unix_sock_traf,&is_blkd,__FUNCTION__))
      msg_delete_channel(all_ctx, dr->llid_unix_sock_traf);
    traf_chain_extract(dr->llid_unix_sock_traf);
    }
  if (dr->prev)
    dr->prev->next = dr->next;
  if (dr->next)
    dr->next->prev = dr->prev;
  if (dr == g_head)
    g_head = dr->next;
  free(dr);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
t_llid_rank *llid_rank_get_with_name(char *name, int num, int tidx)
{
  t_llid_rank *cur = g_head;
  while (cur)
    {
    if ((!strcmp(cur->name, name)) && (cur->num == num) && (cur->tidx == tidx))
      break;
    cur = cur->next;
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
t_llid_rank *llid_rank_get_with_llid(int llid)
{
  t_llid_rank *cur = g_head;
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
t_llid_rank *llid_rank_get_with_llid_unix_sock_traf(int llid)
{
  t_llid_rank *cur = g_head;
  if (llid > 0) 
    {
    while (cur)
      {
      if (cur->llid_unix_sock_traf == llid)
        break;
      cur = cur->next;
      }
    }
  else
    {
    KERR(" ");
    cur = NULL;
    }
  return cur;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
t_llid_rank *llid_rank_get_with_rank(uint32_t rank)
{
  t_llid_rank *cur = g_head;
  while (cur)
    {
    if (cur->rank == rank)
      break;
    cur = cur->next;
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
t_llid_rank *llid_rank_get_with_prechoice_rank(uint32_t rank)
{
  t_llid_rank *cur = g_head;
  while (cur)
    {
    if (cur->prechoice_rank == rank)
      break;
    cur = cur->next;
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
t_llid_rank *llid_rank_peer_rank_set(int llid, char *name, int num,
                                     int tidx, uint32_t rank)
{
  t_llid_rank *dr, *result = NULL;
  if ((rank != 0) && (rank < MAX_USOCK_RANKS))
    {
    dr = llid_rank_get_with_rank(rank);
    if (!dr)
      {
      dr = llid_rank_get_with_llid(llid);
      if (dr)
        {
        if ((!strcmp(name, dr->name)) && (dr->num == num))
          {
          if (dr->prechoice_rank == rank)
            {
            dr->tidx = tidx;
            dr->rank = rank;
            result = dr;
            }
          }
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int llid_rank_peer_rank_unset(int llid, char *name, uint32_t rank)
{
  int result = -1;
  t_llid_rank *dr;
  if ((rank != 0) && (rank < MAX_USOCK_RANKS))
    {
    dr = llid_rank_get_with_rank(rank);
    if (dr)
      {
      dr = llid_rank_get_with_llid(llid);
      if (dr)
        {
        if (!strcmp(name, dr->name))
          {
          if (dr->rank == rank)
            {
            dr->rank = 0;
            result = 0;
            }
          else
            KERR("%d %s", rank, name);
          }
        else
          KERR("%d %s", rank, name);
        }
      else
        KERR("%d %s", rank, name);
      }
    else
      KERR("%d %s", rank, name);
    }
  else
    KERR("%d %s", rank, name);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void llid_rank_llid_create(int llid, char *name, int num, 
                           int tidx, uint32_t rank)
{ 
  t_llid_rank *dr;
  dr = llid_rank_get_with_name(name, num, tidx);
  if (!dr)
    dr = llid_rank_get_with_llid(llid);
  if (!dr)
    dr = llid_rank_get_with_prechoice_rank(rank);
  if (!dr)
    {
    alloc_llid_rank(llid, name, num, tidx, rank);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void llid_rank_sig_disconnect(t_all_ctx *all_ctx, int llid)
{
  t_llid_rank *dr = llid_rank_get_with_llid(llid);
  if (dr)
    {
    rank_has_become_inactive(dr->llid, dr->name, dr->rank);
    free_llid_rank(all_ctx, dr);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int  llid_rank_traf_connect(t_all_ctx *all_ctx, int llid, 
                            char *name, int num, int tidx)
{
  int result = -1;
  t_llid_rank *dr;
  dr = llid_rank_get_with_name(name, num, tidx);
  if (dr)
    {
    if (dr->llid_unix_sock_traf)
      KERR("%s %d %d %d", name, num, llid, dr->llid_unix_sock_traf);
    else
      {
      blkd_set_rank((void *) all_ctx, llid, (int) dr->rank, name, num, tidx);
      traf_chain_insert(llid);
      dr->llid_unix_sock_traf = llid;
      }
    result = 0;
    }
  else
    KERR("%s %d", name, num);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void llid_rank_traf_disconnect(t_all_ctx *all_ctx, int llid)
{
  t_llid_rank *dr = llid_rank_get_with_llid_unix_sock_traf(llid);
  if (dr)
    {
    rank_has_become_inactive(dr->llid, dr->name, dr->rank);
    free_llid_rank(all_ctx, dr);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void init_llid_rank(void)
{
  g_head = NULL;
  g_unix_traf_chain_head = NULL;
  g_llid_unix_sock_traf_nb = 0;
}
/*---------------------------------------------------------------------------*/


