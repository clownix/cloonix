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
#include <sys/types.h>
#include <sys/mman.h>
#include <stdint.h>

#include "ioc.h"
#include "rank_mngt.h"
#include "llid_rank.h"

static int g_current_max_ranks;
static t_llid_rank *g_rank_is_present[MAX_USOCK_RANKS];



/*****************************************************************************/
static void adjust_max_rank(void)
{
  int i, found = 0;
  for (i = MAX_USOCK_RANKS-1; i>0; i--)
    {
    if (g_rank_is_present[i])
      {
      found = i; 
      break;
      }
    }
  g_current_max_ranks = found+1;
}
/*--------------------------------------------------------------------------*/



/*****************************************************************************/
int rank_get_max(void)
{
  return g_current_max_ranks;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
t_llid_rank *rank_get_with_idx(uint32_t rank)
{
  return g_rank_is_present[rank];
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int rank_get_free(void)
{
  int i, result = 0;
  for (i=1; i<MAX_USOCK_RANKS; i++)
    {
    if (g_rank_is_present[i] == NULL)
      {
      if (llid_rank_get_with_prechoice_rank(i) == NULL)
        {
        result = i;
        break;
        }
      }
    }
  return result;
} 
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void rank_has_become_active(t_all_ctx *all_ctx, int llid, 
                                   char *lan, char *name, int num, 
                                   int tidx, uint32_t rank, char *resp)
{
  t_llid_rank *dr;
  dr = llid_rank_peer_rank_set(llid, name, num, tidx, rank);
  if (!dr)
    KERR("%s %s", lan, name);
  else
    {
    g_rank_is_present[rank] = dr;
    adjust_max_rank();
    sprintf(resp, 
           "mulan_req_start lan=%s name=%s num=%d tidx=%d rank=%d",
           lan, name, num, tidx, rank);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void rank_has_become_inactive(int llid, char *name, uint32_t rank)
{
  if (g_rank_is_present[rank])
    {
    if (llid_rank_peer_rank_unset(llid, name, rank))
      KERR("%s", name);
    g_rank_is_present[rank] = NULL;
    adjust_max_rank();
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void rank_dialog(t_all_ctx *all_ctx, int llid, char *line, char *resp)
{
  char lan[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  uint32_t rank;
  int num, tidx;
  if (sscanf(line, 
             "muend_req_rank lan=%s name=%s num=%d tidx=%d",
             lan, name, &num, &tidx) == 4)
    {
    if (strcmp(lan, all_ctx->g_name))
      KOUT("%s %s", lan, all_ctx->g_name);
    rank = rank_get_free();
    if (!rank)
      {
      KERR("%s", line);
      snprintf(resp, MAX_RESP_LEN-1, 
      "muend_resp_rank_alloc_ko lan=%s name=%s num=%d tidx=%d",
      lan, name, num, tidx);
      }
    else
      {
      llid_rank_llid_create(llid, name, num, tidx, rank);
      if (strlen(all_ctx->g_listen_traf_path))
        {
        snprintf(resp, MAX_RESP_LEN-1, 
        "muend_resp_rank_alloc_ok lan=%s name=%s num=%d tidx=%d rank=%d traf=%s",
        lan, name, num, tidx, rank, all_ctx->g_listen_traf_path);
        }
      else
        {
        KERR(" ");
        snprintf(resp, MAX_RESP_LEN-1, 
        "muend_resp_rank_alloc_ko lan=%s name=%s num=%d tidx=%d",
        lan, name, num, tidx);
        }
      }
    }
  else if (sscanf(line, 
                  "muend_ack_rank lan=%s name=%s num=%d tidx=%d rank=%d",
                  lan, name, &num, &tidx, &rank) == 5)
    {
    if (strcmp(lan, all_ctx->g_name))
      KOUT("%s %s", lan, all_ctx->g_name);
    rank_has_become_active(all_ctx, llid, lan, name, num, tidx, rank, resp);
    }
  else
    KERR("%s", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
t_llid_rank *rank_get_with_llid(int llid)
{
  return (llid_rank_get_with_llid(llid));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void init_rank_mngt(void)
{
  init_llid_rank();
  g_current_max_ranks = 0;
  memset(g_rank_is_present, 0, MAX_USOCK_RANKS*sizeof(t_llid_rank *));
}
/*---------------------------------------------------------------------------*/


