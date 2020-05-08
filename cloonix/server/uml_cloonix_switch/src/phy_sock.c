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
#include <errno.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "endp_evt.h"


/*****************************************************************************/
typedef struct t_phy_timer
{
  int  llid;
  int  tid;
  char name[MAX_NAME_LEN];
  char lan[MAX_NAME_LEN];
  int  count;
  struct t_phy_timer *prev;
  struct t_phy_timer *next;
} t_phy_timer;
/*---------------------------------------------------------------------------*/

static t_phy_timer *g_head_timer;


/*****************************************************************************/
static t_phy_timer *timer_find(char *name)
{
  t_phy_timer *cur = NULL;
  if (name[0])
    {
    cur = g_head_timer;
    while(cur)
      {
      if (!strcmp(cur->name, name))
        break;
      cur = cur->next;
      }
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_phy_timer *timer_alloc(char *name)
{
  t_phy_timer *timer = timer_find(name);
  if (timer)
    KOUT("%s", name);
  timer = (t_phy_timer *)clownix_malloc(sizeof(t_phy_timer), 4);
  memset(timer, 0, sizeof(t_phy_timer));
  strncpy(timer->name, name, MAX_NAME_LEN-1);
  if (g_head_timer)
    g_head_timer->prev = timer;
  timer->next = g_head_timer;
  g_head_timer = timer;
  return timer;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_free(t_phy_timer *timer)
{
  if (timer->prev)
    timer->prev->next = timer->next;
  if (timer->next)
    timer->next->prev = timer->prev;
  if (timer == g_head_timer)
    g_head_timer = timer->next;
  clownix_free(timer, __FUNCTION__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void sock_add_lan(int llid, int tid, char *name, char *lan)
{
  int tidx;
  char err[MAX_PATH_LEN];

  if (endp_evt_lan_find(name, 0, lan, &tidx))
    {
    sprintf(err, "%s already has %s", name, lan);
    KERR("%s", err);
    if (msg_exist_channel(llid))
      send_status_ko(llid, tid, err);
    else 
      KERR("%s", err);
    }
  else if (endp_evt_lan_full(name, 0, &tidx))
    {
    sprintf(err, "Too many lans  for %s MAX_TRAF_ENDPOINT", name);
    KERR("%s", err);
    if (msg_exist_channel(llid))
      send_status_ko(llid, tid, err);
    else 
      KERR("%s", err);
    }
  else
    {
    endp_evt_add_lan(llid, tid, name, 0, lan, tidx);
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_endp(void *data)
{
  t_phy_timer *te = (t_phy_timer *) data;
  char err[MAX_PATH_LEN];
  if (endp_evt_exists(te->name, 0))
    {
    sock_add_lan(te->llid, te->tid, te->name, te->lan);
    timer_free(te);
    }
  else
    {
    te->count++;
    if (te->count >= 40)
      {
      sprintf(err, "bad phy sock start: %s %s", te->name, te->lan);
      if (msg_exist_channel(te->llid))
        send_status_ko(te->llid, te->tid, err);
      else 
        KERR("%s", err);
      timer_free(te);
      }
    else
      {
      clownix_timeout_add(20, timer_endp, (void *) te, NULL, NULL);
      }
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void phy_sock_timer_add(int llid, int tid, char *name, char *lan)
{
  t_phy_timer *te = timer_find(name);
  if (te)
    {
    if (msg_exist_channel(llid))
      send_status_ko(llid, tid, "endpoint already waiting");
    else 
      KERR("endpoint already waiting");
    }
  else
    {
    te = timer_alloc(name);
    te->llid = llid;
    te->tid = tid;
    strncpy(te->lan, lan, MAX_NAME_LEN-1);
    clownix_timeout_add(10, timer_endp, (void *) te, NULL, NULL);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void phy_sock_init(void)
{
  g_head_timer = NULL;
}
/*--------------------------------------------------------------------------*/
