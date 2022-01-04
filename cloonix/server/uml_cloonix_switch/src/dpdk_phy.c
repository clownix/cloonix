/*****************************************************************************/
/*    Copyright (C) 2006-2022 clownix@clownix.net License AGPL-3             */
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
#include "commun_daemon.h"
#include "cfg_store.h"
#include "dpdk_xyx.h"
#include "utils_cmd_line_maker.h"


typedef struct t_add_lan
{
  char name[MAX_NAME_LEN];
  int num;
  char lan[MAX_NAME_LEN];
  int llid;
  int tid;
  int count;
  struct t_add_lan *prev;
  struct t_add_lan *next;
} t_add_lan;

static t_add_lan *g_head_lan;


/*****************************************************************************/
static t_add_lan *lan_find(char *name)
{
  t_add_lan *cur = g_head_lan;
  while(cur && strcmp(cur->name, name))
    cur = cur->next;
  return cur;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void lan_alloc(int llid, int tid, char *name, int num, char *lan)
{
  t_add_lan *cur  = (t_add_lan *) clownix_malloc(sizeof(t_add_lan), 6);
  memset(cur, 0, sizeof(t_add_lan));
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  strncpy(cur->lan, lan, MAX_NAME_LEN-1);
  cur->num = num;
  cur->llid = llid;
  cur->tid = tid;
  if (g_head_lan)
    g_head_lan->prev = cur;
  cur->next = g_head_lan;
  g_head_lan = cur;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void lan_free(t_add_lan *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_lan)
    g_head_lan = cur->next;
  clownix_free(cur, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void timer_beat(void *data)
{
  t_add_lan *next_cl, *cl = g_head_lan;
  while(cl)
    {
    next_cl = cl->next;
    if ((dpdk_xyx_exists(cl->name, cl->num)) &&
        (dpdk_xyx_ready(cl->name, cl->num)))
      {
      if (dpdk_xyx_lan_empty(cl->name, cl->num))
        dpdk_xyx_add_lan(cl->llid, cl->tid, cl->name, cl->num, cl->lan);
      else
        {
        KERR("ERROR PHY ADD %s %d %s", cl->name, cl->num, cl->lan);
        utils_send_status_ko(&(cl->llid), &(cl->tid), "lan existing");
        }
      lan_free(cl);
      }
    else
      {
      cl->count += 1; 
      if (cl->count > 100)
        {
        KERR("ERROR PHY ADD %s %d %s", cl->name, cl->num, cl->lan);
        lan_free(cl);
        }
      }
    cl = next_cl;
    }
  clownix_timeout_add(10, timer_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
int dpdk_phy_add(int llid, int tid, char *name)
{
  return (dpdk_xyx_add(llid, tid, name, 0, endp_type_phy));
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int dpdk_phy_del(int llid, int tid, char *name)
{
  return (dpdk_xyx_del(llid, tid, name, 0));
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void dpdk_phy_resp_add_lan(int is_ko, char *lan, char *name)
{
  dpdk_xyx_resp_add_lan(is_ko, lan, name, 0);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void dpdk_phy_resp_del_lan(int is_ko, char *lan, char *name)
{
  dpdk_xyx_resp_del_lan(is_ko, lan, name, 0);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int dpdk_phy_add_lan(int llid, int tid, char *name, char *lan)
{
  int result = -1;
  if (dpdk_xyx_exists(name, 0))
    {
    if (lan_find(name) == NULL)
      {
      lan_alloc(llid, tid, name, 0, lan);
      result = 0;
      }
    else
      KERR("ERROR PHY ADD %s %s", name, lan);
    }
  else
    KERR("ERROR PHY ADD %s %s", name, lan);
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int dpdk_phy_del_lan(int llid, int tid, char *name,  char *lan)
{
  return (dpdk_xyx_del_lan(llid, tid, name, 0, lan));
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void dpdk_phy_resp_add(int is_ko, char *name)
{
  dpdk_xyx_resp_add(is_ko, name, 0);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void dpdk_phy_resp_del(int is_ko, char *name)
{
  dpdk_xyx_resp_del(is_ko, name, 0);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void dpdk_phy_init(void)
{
  g_head_lan = NULL;
  clownix_timeout_add(5, timer_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/


