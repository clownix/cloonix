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
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include "io_clownix.h"
#include "lib_topo.h"


/****************************************************************************/
static int topo_find_kvm(char *name, t_topo_info *topo)
{
  int i, found = 0;
  for (i=0; i< topo->nb_kvm; i++)
    if (!strcmp(name, topo->kvm[i].name))
      {
      found = 1;
      break;
      }
  return found;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int topo_find_c2c(char *name, t_topo_info *topo)
{
  int i, found = 0;
  for (i=0; i< topo->nb_c2c; i++)
    if (!strcmp(name, topo->c2c[i].name))
      {
      found = 1;
      break;
      }
  return found;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int topo_find_snf(char *name, t_topo_info *topo)
{
  int i, found = 0;
  for (i=0; i< topo->nb_snf; i++)
    if (!strcmp(name, topo->snf[i].name))
      {
      found = 1;
      break;
      }
  return found;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int topo_find_sat(char *name, t_topo_info *topo)
{
  int i, found = 0;
  for (i=0; i< topo->nb_sat; i++)
    if (!strcmp(name, topo->sat[i].name))
      {
      found = 1;
      break;
      }
  return found;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int topo_find_lan(char *lan, t_topo_info *topo)
{
  int i, j, found = 0;
  for (i=0; (found==0) && (i < topo->nb_endp); i++)
    {
    for (j=0; j< topo->endp[i].lan.nb_lan; j++)
      {
      if (!strcmp(lan, topo->endp[i].lan.lan[j].lan))
        {
        found = 1;
        break;
        }
      }
    }
  return found;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int topo_find_edge(char *name, int num, char *lan, t_topo_info *topo)
{
  int i, j, found = 0;
  for (i=0; i< topo->nb_endp; i++)
    {
    if ((!strcmp(name, topo->endp[i].name)) && (topo->endp[i].num == num))
      {
      for (j=0; j< topo->endp[i].lan.nb_lan; j++)
        {
        if (!strcmp(lan, topo->endp[i].lan.lan[j].lan))
          {
          found = 1;
          break;
          }
        }
      }
    }
  return found;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_topo_kvm_chain *topo_get_kvm_chain(t_topo_info *topo)
{
  int i;
  t_topo_kvm_chain *cur, *res = NULL;
  for (i=0; i< topo->nb_kvm; i++)
    {
    cur = (t_topo_kvm_chain *) clownix_malloc(sizeof(t_topo_kvm_chain), 3); 
    memset(cur, 0, sizeof(t_topo_kvm_chain));
    memcpy(&(cur->kvm), &(topo->kvm[i]), sizeof(t_topo_kvm)); 
    cur->next = res;
    if (res)
      res->prev = cur;
    res = cur;
    }
  return res;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_topo_c2c_chain *topo_get_c2c_chain(t_topo_info *topo)
{
  int i;
  t_topo_c2c_chain *cur, *res = NULL;
  for (i=0; i< topo->nb_c2c; i++)
    {
    cur = (t_topo_c2c_chain *) clownix_malloc(sizeof(t_topo_c2c_chain), 3);
    memset(cur, 0, sizeof(t_topo_c2c_chain));
    memcpy(&(cur->c2c), &(topo->c2c[i]), sizeof(t_topo_c2c));
    cur->next = res;
    if (res)
      res->prev = cur;
    res = cur;
    }
  return res;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_topo_snf_chain *topo_get_snf_chain(t_topo_info *topo)
{
  int i;
  t_topo_snf_chain *cur, *res = NULL;
  for (i=0; i< topo->nb_snf; i++)
    {
    cur = (t_topo_snf_chain *) clownix_malloc(sizeof(t_topo_snf_chain), 3);
    memset(cur, 0, sizeof(t_topo_snf_chain));
    memcpy(&(cur->snf), &(topo->snf[i]), sizeof(t_topo_snf));
    cur->next = res;
    if (res)
      res->prev = cur;
    res = cur;
    }
  return res;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_topo_sat_chain *topo_get_sat_chain(t_topo_info *topo)
{
  int i;
  t_topo_sat_chain *cur, *res = NULL;
  for (i=0; i< topo->nb_sat; i++)
    {
    cur = (t_topo_sat_chain *) clownix_malloc(sizeof(t_topo_sat_chain), 3);
    memset(cur, 0, sizeof(t_topo_sat_chain));
    memcpy(&(cur->sat), &(topo->sat[i]), sizeof(t_topo_sat));
    cur->next = res;
    if (res)
      res->prev = cur;
    res = cur;
    }
  return res;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int lan_does_not_exist_in_chain(t_topo_lan_chain *ch, char *lan)
{
  t_topo_lan_chain *cur = ch;
  int result = 1;
  while(cur)
    {
    if (!strcmp(cur->lan, lan))
      {
      result = 0;
      break;
      }
    cur = cur->next;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_topo_lan_chain *topo_get_lan_chain(t_topo_info *topo)
{
  int i, j;
  char *lan;
  t_topo_lan_chain *cur, *res = NULL;
  for (i=0; i< topo->nb_endp; i++)
    {
    for (j=0; j< topo->endp[i].lan.nb_lan; j++)
      {
      lan = topo->endp[i].lan.lan[j].lan;
      if (lan_does_not_exist_in_chain(res, lan))
        {
        cur=(t_topo_lan_chain *)clownix_malloc(sizeof(t_topo_lan_chain),3);
        memset(cur, 0, sizeof(t_topo_lan_chain));
        strncpy(cur->lan, lan, MAX_NAME_LEN-1);
        cur->next = res;
        if (res)
          res->prev = cur;
        res = cur;
        }
      }
    }
  return res;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_topo_edge_chain *topo_get_edge_chain(t_topo_info *topo)
{
  int i, j;
  t_topo_edge_chain *cur, *res = NULL;
  for (i=0; i< topo->nb_endp; i++)
    {
    for (j=0; j< topo->endp[i].lan.nb_lan; j++)
      {
      cur=(t_topo_edge_chain *)clownix_malloc(sizeof(t_topo_edge_chain),3);
      memset(cur, 0, sizeof(t_topo_edge_chain));
      strncpy(cur->name, topo->endp[i].name, MAX_NAME_LEN-1);
      cur->num = topo->endp[i].num;
      strncpy(cur->lan, topo->endp[i].lan.lan[j].lan, MAX_NAME_LEN-1);
      cur->next = res;
      if (res)
        res->prev = cur;
      res = cur;
      }
    }
  return res;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void topo_free_kvm_chain(t_topo_kvm_chain *ch)
{
  t_topo_kvm_chain *next, *cur = ch;
  while(cur)
    {
    next = cur->next;
    clownix_free(cur, __FUNCTION__);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void topo_free_c2c_chain(t_topo_c2c_chain *ch)
{
  t_topo_c2c_chain *next, *cur = ch;
  while(cur)
    {
    next = cur->next;
    clownix_free(cur, __FUNCTION__);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void topo_free_snf_chain(t_topo_snf_chain *ch)
{
  t_topo_snf_chain *next, *cur = ch;
  while(cur)
    {
    next = cur->next;
    clownix_free(cur, __FUNCTION__);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void topo_free_sat_chain(t_topo_sat_chain *ch)
{
  t_topo_sat_chain *next, *cur = ch;
  while(cur)
    {
    next = cur->next;
    clownix_free(cur, __FUNCTION__);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void topo_free_lan_chain(t_topo_lan_chain *ch)
{
  t_topo_lan_chain *next, *cur = ch;
  while(cur)
    {
    next = cur->next;
    clownix_free(cur, __FUNCTION__);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void topo_free_edge_chain(t_topo_edge_chain *ch)
{
  t_topo_edge_chain *next, *cur = ch;
  while(cur)
    {
    next = cur->next;
    clownix_free(cur, __FUNCTION__);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void take_out_from_kvm_chain(t_topo_kvm_chain **ch, t_topo_info *topo)
{
  t_topo_kvm_chain *next, *cur = *ch;
  while (cur)
    {
    next = cur->next;
    if (topo_find_kvm(cur->kvm.name, topo))
      {
      if (cur->next)
        cur->next->prev = cur->prev;
      if (cur->prev)
        cur->prev->next = cur->next;
      if (cur == *ch)
        *ch = cur->next;
      clownix_free(cur, __FUNCTION__);
      }
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void take_out_from_c2c_chain(t_topo_c2c_chain **ch, t_topo_info *topo)
{
  t_topo_c2c_chain *next, *cur = *ch;
  while (cur)
    {
    next = cur->next;
    if (topo_find_c2c(cur->c2c.name, topo))
      {
      if (cur->next)
        cur->next->prev = cur->prev;
      if (cur->prev)
        cur->prev->next = cur->next;
      if (cur == *ch)
        *ch = cur->next;
      clownix_free(cur, __FUNCTION__);
      } 
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void take_out_from_snf_chain(t_topo_snf_chain **ch, t_topo_info *topo)
{
  t_topo_snf_chain *next, *cur = *ch;
  while (cur)
    {
    next = cur->next;
    if (topo_find_snf(cur->snf.name, topo))
      {
      if (cur->next)
        cur->next->prev = cur->prev;
      if (cur->prev)
        cur->prev->next = cur->next;
      if (cur == *ch)
        *ch = cur->next;
      clownix_free(cur, __FUNCTION__);
      } 
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void take_out_from_sat_chain(t_topo_sat_chain **ch, t_topo_info *topo)
{
  t_topo_sat_chain *next, *cur = *ch;
  while (cur)
    {
    next = cur->next;
    if (topo_find_sat(cur->sat.name, topo))
      {
      if (cur->next)
        cur->next->prev = cur->prev;
      if (cur->prev)
        cur->prev->next = cur->next;
      if (cur == *ch)
        *ch = cur->next;
      clownix_free(cur, __FUNCTION__);
      } 
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void take_out_from_lan_chain(t_topo_lan_chain **ch, t_topo_info *topo)
{
  t_topo_lan_chain *next, *cur = *ch;
  while (cur)
    {
    next = cur->next;
    if (topo_find_lan(cur->lan, topo))
      {
      if (cur->next)
        cur->next->prev = cur->prev;
      if (cur->prev)
        cur->prev->next = cur->next;
      if (cur == *ch)
        *ch = cur->next;
      clownix_free(cur, __FUNCTION__);
      }
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void take_out_from_edge_chain(t_topo_edge_chain **ch,t_topo_info *topo)
{
  t_topo_edge_chain *next, *cur = *ch;
  while (cur)
    {
    next = cur->next;
    if (topo_find_edge(cur->name, cur->num, cur->lan, topo))
      {
      if (cur->next)
        cur->next->prev = cur->prev;
      if (cur->prev)
        cur->prev->next = cur->next;
      if (cur == *ch)
        *ch = cur->next;
      clownix_free(cur, __FUNCTION__);
      }
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_topo_differences *topo_get_diffs(t_topo_info *newt, t_topo_info *oldt)
{
  static t_topo_differences diffs;
  memset(&diffs, 0, sizeof(t_topo_differences));

  diffs.add_kvm     = topo_get_kvm_chain(newt);
  if (oldt)
    diffs.del_kvm   = topo_get_kvm_chain(oldt);

  diffs.add_c2c     = topo_get_c2c_chain(newt);
  if (oldt)
    diffs.del_c2c   = topo_get_c2c_chain(oldt);

  diffs.add_snf     = topo_get_snf_chain(newt);
  if (oldt)
    diffs.del_snf   = topo_get_snf_chain(oldt);

  diffs.add_sat     = topo_get_sat_chain(newt);
  if (oldt)
    diffs.del_sat   = topo_get_sat_chain(oldt);

  diffs.add_lan     = topo_get_lan_chain(newt);
  if (oldt)
    diffs.del_lan   = topo_get_lan_chain(oldt);

  diffs.add_edge    = topo_get_edge_chain(newt);
  if (oldt)
    diffs.del_edge  = topo_get_edge_chain(oldt);

  if (diffs.add_kvm && oldt)
    take_out_from_kvm_chain(&(diffs.add_kvm), oldt);

  if (diffs.del_kvm && newt)
    take_out_from_kvm_chain(&(diffs.del_kvm), newt);

  if (diffs.add_c2c && oldt)
    take_out_from_c2c_chain(&(diffs.add_c2c), oldt);

  if (diffs.del_c2c && newt)
    take_out_from_c2c_chain(&(diffs.del_c2c), newt);

  if (diffs.add_snf && oldt)
    take_out_from_snf_chain(&(diffs.add_snf), oldt);

  if (diffs.del_snf && newt)
    take_out_from_snf_chain(&(diffs.del_snf), newt);

  if (diffs.add_sat && oldt)
    take_out_from_sat_chain(&(diffs.add_sat), oldt);

  if (diffs.del_sat && newt)
    take_out_from_sat_chain(&(diffs.del_sat), newt);

  if (diffs.add_lan && oldt)
    take_out_from_lan_chain(&(diffs.add_lan), oldt);

  if (diffs.del_lan && newt)
    take_out_from_lan_chain(&(diffs.del_lan), newt);

  if (diffs.add_edge && oldt)
    take_out_from_edge_chain(&(diffs.add_edge), oldt);

  if (diffs.del_edge && newt)
    take_out_from_edge_chain(&(diffs.del_edge), newt);

  return &diffs;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void topo_free_diffs(t_topo_differences *diffs)
{
  topo_free_kvm_chain(diffs->add_kvm);
  topo_free_c2c_chain(diffs->add_c2c);
  topo_free_snf_chain(diffs->add_snf);
  topo_free_sat_chain(diffs->add_sat);
  topo_free_lan_chain(diffs->add_lan);
  topo_free_edge_chain(diffs->add_edge);
  topo_free_kvm_chain(diffs->del_kvm);
  topo_free_c2c_chain(diffs->del_c2c);
  topo_free_snf_chain(diffs->del_snf);
  topo_free_sat_chain(diffs->del_sat);
  topo_free_lan_chain(diffs->del_lan);
  topo_free_edge_chain(diffs->del_edge);
}
/*--------------------------------------------------------------------------*/

