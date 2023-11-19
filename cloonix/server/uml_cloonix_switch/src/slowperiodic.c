/*****************************************************************************/
/*    Copyright (C) 2006-2023 clownix@clownix.net License AGPL-3             */
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
#include "podman_images.h"


/*****************************************************************************/
typedef struct t_slowperiodic_subs
{
  int llid;
  int tid;
  struct t_slowperiodic_subs *prev;
  struct t_slowperiodic_subs *next;
} t_slowperiodic_subs;

static t_slowperiodic_subs *head_subs;
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int ends_with_suffix(char *str, char *suffix)
{
  int result = 0;
  size_t lenstr, lensuffix;
  if (!str || !suffix)
    KOUT(" ");
  lenstr = strlen(str);
  lensuffix = strlen(suffix);
  if ((lenstr==0) || (lensuffix==0))
    KOUT("ERROR %lu %lu", lenstr, lensuffix);
  if (lensuffix < lenstr)
    result = (strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0);
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int get_bulk_files(t_slowperiodic **slowperiodic, char *suffix)
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
        if (ends_with_suffix(ent->d_name, suffix))
          {
          strncpy((*slowperiodic)[nb].name, ent->d_name, MAX_NAME_LEN-1);
          nb += 1;
          }
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
  int llid, tid, nb_qcow2, nb_img, nb_pod;
  t_slowperiodic *slowperiodic_qcow2;
  t_slowperiodic *slowperiodic_img;
  t_slowperiodic *slowperiodic_pod;
  t_slowperiodic_subs *cur = head_subs;
  nb_qcow2 = get_bulk_files(&slowperiodic_qcow2, ".qcow2");
  nb_img = get_bulk_files(&slowperiodic_img, ".zip");
  nb_pod = podman_image_get_list("podman", &slowperiodic_pod);
  while (cur)
    {
    llid = cur->llid;
    tid = cur->tid;
    if (msg_exist_channel(llid))
      {
      if (nb_qcow2)
        send_slowperiodic_qcow2(llid, tid, nb_qcow2, slowperiodic_qcow2);
      if (nb_img)
        send_slowperiodic_img(llid, tid, nb_img, slowperiodic_img);
      if (nb_pod)
        send_slowperiodic_podman(llid, tid, nb_pod, slowperiodic_pod);
      }
    else
      event_print ("SLOWPERIODIC ERROR!!!!!!");
    cur = cur->next;
    }
  clownix_free(slowperiodic_qcow2, __FUNCTION__);
  clownix_free(slowperiodic_img, __FUNCTION__);
  if (nb_pod)
    clownix_free(slowperiodic_pod, __FUNCTION__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timeout_collect_slowperiodic(void *data)
{
  action_send_slowperiodic();
  clownix_timeout_add(1000, timeout_collect_slowperiodic, NULL, NULL, NULL);
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
  if (head_subs)
    head_subs->prev = sub;
  sub->next = head_subs;
  head_subs = sub;
  action_send_slowperiodic();
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void slowperiodic_llid_delete(int llid)
{
  t_slowperiodic_subs *next, *cur = head_subs;
  while(cur)
    {
    next = cur->next;
    if (cur->llid == llid)
      {
      if (cur->prev)
        cur->prev->next = cur->next;
      if (cur->next)
        cur->next->prev = cur->prev;
      if (cur == head_subs)
        head_subs = cur->next;
      clownix_free(cur, __FUNCTION__);
      }
    cur = next;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void slowperiodic_init(void)
{
  head_subs = NULL;
  clownix_timeout_add(1000, timeout_collect_slowperiodic, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/



