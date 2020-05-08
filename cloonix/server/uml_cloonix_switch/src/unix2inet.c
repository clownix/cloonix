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
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_daemon.h"
#include "event_subscriber.h"
#include "hop_event.h"
#include "cfg_store.h"
#include "endp_mngt.h"


typedef struct t_unix2inet_record 
{
  int llid;
  int llid_con;
  char vmname[MAX_NAME_LEN];
  struct t_unix2inet_record *prev;
  struct t_unix2inet_record *next;
} t_unix2inet_record;
/*---------------------------------------------------------------------------*/

static t_unix2inet_record *g_head_unix2inet;
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_unix2inet_record *find_unix2inet_record(int llid, int llid_con)
{
  t_unix2inet_record *result = NULL;
  t_unix2inet_record *cur = g_head_unix2inet;
  while(cur)
    {
    if ((cur->llid == llid) && (cur->llid_con == llid_con))
      {
      result = cur;
      break;
      }
    cur = cur->next;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int alloc_unix2inet_record(int llid, int llid_con, char *vmname)
{
  int result = -1;
  t_unix2inet_record *rec = find_unix2inet_record(llid, llid_con);
  if ((llid == 0) || (llid_con == 0) || (strlen(vmname) < 2))
    KERR("%d %d %s", llid, llid_con, vmname);
  else if (rec != NULL)
    KERR("%d %d %s", llid, llid_con, vmname);
  else
    {
    result = 0;
    rec = (t_unix2inet_record *) clownix_malloc(sizeof(t_unix2inet_record), 5);
    memset(rec, 0, sizeof(t_unix2inet_record));
    rec->llid = llid;
    rec->llid_con = llid_con;
    strncpy(rec->vmname, vmname, MAX_NAME_LEN-1);
    if (g_head_unix2inet)
      g_head_unix2inet->prev = rec;
    rec->prev = NULL;
    rec->next = g_head_unix2inet;
    g_head_unix2inet = rec;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int free_unix2inet_record(int llid, int llid_con)
{
  int result = -1;
  t_unix2inet_record *rec = find_unix2inet_record(llid, llid_con);
  if (rec)
    {
    result = 0;
    if (rec->prev)
      rec->prev->next = rec->next;
    if (rec->next)
      rec->next->prev = rec->prev;
    if (rec == g_head_unix2inet)
      g_head_unix2inet = rec->next;
    clownix_free(rec, __FUNCTION__);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_unix2inet_record *get_unix2inet_record_with_llid(int llid)
{
  t_unix2inet_record *result = NULL;
  t_unix2inet_record *cur = g_head_unix2inet;
  while(cur)
    {
    if (cur->llid == llid)
      {
      result = cur;
      break;
      }
    cur = cur->next;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_unix2inet_record *get_unix2inet_record_with_vmname(char *vmname)
{
  t_unix2inet_record *result = NULL;
  t_unix2inet_record *cur = g_head_unix2inet;
  while(cur)
    {
    if (!strcmp(cur->vmname, vmname))
      {
      result = cur;
      break;
      }
    cur = cur->next;
    }
  return result;
} 
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void send_event_broken_unix2inet(int llid, int llid_con)
{
  char msg[MAX_PATH_LEN];
  sprintf(msg, "unix2inet_conpath_evt_break llid=%d", llid_con);
  rpct_send_app_msg(NULL, llid, 0, msg);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int get_common_lan(int nb_nat, int *nat, int nb_vm, int *vm)
{
  int i, j, result = -1;
  int done = 0;
  for (i=0; (done == 0) && (i<nb_nat); i++)
    {
    if (nat[i] == 0)
      {
      done = 1;
      KERR(" ");
      }
    for (j=0; (done == 0) && (j<nb_vm); j++)
      {
      if (vm[i] == 0)
        {
        done = 1;
        KERR(" ");
        }
      if (nat[i] == vm[i])
        {
        done = 1;
        result = nat[i];
        }
      }
    } 
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void unix2inet_monitor(int llid, int llid_con, char *satname, char *vmname)
{
  int i, nb, nb_lan_nat, nb_lan_vm, common_lan;
  int32_t lan_vm[MAX_SOCK_VM*MAX_TRAF_ENDPOINT];
  int32_t lan_nat[MAX_TRAF_ENDPOINT];
  t_vm *vm;
  for (i=0; i<MAX_SOCK_VM*MAX_TRAF_ENDPOINT; i++)
    lan_vm[i] = 0;
  for (i=0; i<MAX_TRAF_ENDPOINT; i++)
    lan_nat[i] = 0;
  if (endp_mngt_get_all_lan(satname, 1, &nb_lan_nat, lan_nat))
    KERR("%s", satname);
  else
    {
    vm = cfg_get_vm(vmname);
    if (!vm)
      KERR("%s", vmname);
    else
      {
      nb = vm->kvm.nb_tot_eth;
      if (endp_mngt_get_all_lan(vmname, nb, &nb_lan_vm, lan_vm))
        KERR("%s", satname);
      else
        {
        common_lan = get_common_lan(nb_lan_nat, lan_nat, nb_lan_vm, lan_vm);
        if (common_lan <= 0)
          KERR("%s %s", satname, vmname);
        else
          {
          if (endp_mngt_set_evt(satname, 1, common_lan, llid, llid_con))
            KERR("%s %s", satname, vmname);
          else if (endp_mngt_set_evt(vmname, nb, common_lan, llid, llid_con))
            KERR("%s %s", satname, vmname);
          else if (alloc_unix2inet_record(llid, llid_con, vmname))
            KERR("%s %s", satname, vmname);
          }
        }
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void unix2inet_lan_event(int llid, int llid_con)
{
  t_unix2inet_record *rec = find_unix2inet_record(llid, llid_con);
  if (!rec)
    KERR(" ");
  else
    {
    send_event_broken_unix2inet(llid, llid_con);
    if (free_unix2inet_record(llid, llid_con))
      KERR(" ");
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void unix2inet_llid_cutoff(int llid)
{
  t_unix2inet_record *rec = get_unix2inet_record_with_llid(llid);
  while(rec)
    {
    send_event_broken_unix2inet(llid, rec->llid_con);
    if (free_unix2inet_record(llid, rec->llid_con))
      KERR(" ");
    rec = get_unix2inet_record_with_llid(llid);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void unix2inet_vm_cutoff(char *vmname)
{
  t_unix2inet_record *rec = get_unix2inet_record_with_vmname(vmname);
  while(rec)
    {
    send_event_broken_unix2inet(rec->llid, rec->llid_con);
    if (free_unix2inet_record(rec->llid, rec->llid_con))
      KERR(" ");
    rec = get_unix2inet_record_with_vmname(vmname);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void unix2inet_init(void)
{
  g_head_unix2inet = NULL;
}
/*---------------------------------------------------------------------------*/



