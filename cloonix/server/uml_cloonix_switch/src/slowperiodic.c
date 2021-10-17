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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include "io_clownix.h"
#include "commun_daemon.h"
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "event_subscriber.h"
#include "utils_cmd_line_maker.h"


/*****************************************************************************/
typedef struct t_slowperiodic_subs
{
  int llid;
  int tid;
  struct t_slowperiodic_subs *prev;
  struct t_slowperiodic_subs *next;
} t_slowperiodic_subs;

static t_slowperiodic_subs *head_slowperiodic_subs;
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int get_bulk_files(t_slowperiodic **slowperiodic)
{
  DIR *dirptr;
  struct dirent *ent;
  int max = (MAX_BULK_FILES-1), nb = 0, len = max * sizeof(t_slowperiodic);
  *slowperiodic=(t_slowperiodic *)clownix_malloc(len, 5);
  memset(*slowperiodic, 0, len);
  dirptr = opendir(cfg_get_bulk());
  if (dirptr)
    {
    while ((ent = readdir(dirptr)) != NULL)
      {
      if (!strcmp(ent->d_name, "."))
        continue;
      if (!strcmp(ent->d_name, ".."))
        continue;
      if (ent->d_type == DT_REG)
        {
        memset((*slowperiodic)[nb].name, 0, MAX_NAME_LEN);
        strncpy((*slowperiodic)[nb].name, ent->d_name, MAX_NAME_LEN-1);
        nb += 1;
        }
      if (nb >= max)
        break;
      }
    if (closedir(dirptr))
      KOUT("%d", errno);
    }
  return nb;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void action_send_slowperiodic(void)
{
  int llid, tid, nb;
  t_slowperiodic *slowperiodic;
  t_slowperiodic_subs *cur = head_slowperiodic_subs;
  nb = get_bulk_files(&slowperiodic);
  while (cur)
    {
    llid = cur->llid;
    tid = cur->tid;
    if (msg_exist_channel(llid))
      {
      send_slowperiodic(llid, tid, nb, slowperiodic);
      }
    else
      event_print ("SLOWPERIODIC ERROR!!!!!!");
    cur = cur->next;
    }
  clownix_free(slowperiodic, __FUNCTION__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timeout_collect_slowperiodic(void *data)
{
  action_send_slowperiodic();
  clownix_timeout_add(500, timeout_collect_slowperiodic, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_slowperiodic_sub(int llid, int tid)
{
  t_slowperiodic_subs *sub;
  sub = (t_slowperiodic_subs *) clownix_malloc(sizeof(t_slowperiodic_subs), 13);
  memset(sub, 0, sizeof(t_slowperiodic_subs));
  sub->llid = llid;
  sub->tid = tid;
  if (head_slowperiodic_subs)
    head_slowperiodic_subs->prev = sub;
  sub->next = head_slowperiodic_subs;
  head_slowperiodic_subs = sub;
  action_send_slowperiodic();
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void slowperiodic_llid_delete(int llid)
{
  t_slowperiodic_subs *next, *cur = head_slowperiodic_subs;
  while(cur)
    {
    next = cur->next;
    if (cur->llid == llid)
      {
      if (cur->prev)
        cur->prev->next = cur->next;
      if (cur->next)
        cur->next->prev = cur->prev;
      if (cur == head_slowperiodic_subs)
        head_slowperiodic_subs = cur->next;
      clownix_free(cur, __FUNCTION__);
      }
    cur = next;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void slowperiodic_init(void)
{
  head_slowperiodic_subs = NULL;
  clownix_timeout_add(500, timeout_collect_slowperiodic, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/



