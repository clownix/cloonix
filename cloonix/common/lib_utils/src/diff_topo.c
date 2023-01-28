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
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include "io_clownix.h"
#include "lib_topo.h"

/****************************************************************************/
static int topo_find_cnt(char *name, t_topo_info *topo)
{
  int i, found = 0;
  for (i=0; i< topo->nb_cnt; i++)
    if (!strcmp(name, topo->cnt[i].name))
      {
      found = 1;
      break;
      }
  return found;
}
/*--------------------------------------------------------------------------*/

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
static int topo_find_tap(char *name, t_topo_info *topo)
{
  int i, found = 0;
  for (i=0; i< topo->nb_tap; i++)
    if (!strcmp(name, topo->tap[i].name))
      {
      found = 1;
      break;
      }
  return found;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int topo_find_a2b(char *name, t_topo_info *topo)
{
  int i, found = 0;
  for (i=0; i< topo->nb_a2b; i++)
    if (!strcmp(name, topo->a2b[i].name))
      {
      found = 1;
      break;
      }
  return found;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int topo_find_nat(char *name, t_topo_info *topo)
{
  int i, found = 0;
  for (i=0; i< topo->nb_nat; i++)
    if (!strcmp(name, topo->nat[i].name))
      {
      found = 1;
      break;
      }
  return found;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int topo_find_phy(char *name, t_topo_info *topo)
{
  int i, found = 0;
  for (i=0; i< topo->nb_phy; i++)
    if (!strcmp(name, topo->phy[i].name))
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
static t_topo_cnt_chain *topo_get_cnt_chain(t_topo_info *topo)
{
  int i;
  t_topo_cnt_chain *cur, *res = NULL;
  for (i=0; i< topo->nb_cnt; i++)
    {
    cur = (t_topo_cnt_chain *) clownix_malloc(sizeof(t_topo_cnt_chain), 3);
    memset(cur, 0, sizeof(t_topo_cnt_chain));
    memcpy(&(cur->cnt), &(topo->cnt[i]), sizeof(t_topo_cnt));
    cur->next = res;
    if (res)
      res->prev = cur;
    res = cur;
    }
  return res;
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
static t_topo_tap_chain *topo_get_tap_chain(t_topo_info *topo)
{
  int i;
  t_topo_tap_chain *cur, *res = NULL;
  for (i=0; i< topo->nb_tap; i++)
    {
    cur = (t_topo_tap_chain *) clownix_malloc(sizeof(t_topo_tap_chain), 3);
    memset(cur, 0, sizeof(t_topo_tap_chain));
    memcpy(&(cur->tap), &(topo->tap[i]), sizeof(t_topo_tap));
    cur->next = res;
    if (res)
      res->prev = cur;
    res = cur;
    }
  return res;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_topo_a2b_chain *topo_get_a2b_chain(t_topo_info *topo)
{
  int i;
  t_topo_a2b_chain *cur, *res = NULL;
  for (i=0; i< topo->nb_a2b; i++)
    {
    cur = (t_topo_a2b_chain *) clownix_malloc(sizeof(t_topo_a2b_chain), 3);
    memset(cur, 0, sizeof(t_topo_a2b_chain));
    memcpy(&(cur->a2b), &(topo->a2b[i]), sizeof(t_topo_a2b));
    cur->next = res;
    if (res)
      res->prev = cur;
    res = cur;
    }
  return res;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_topo_nat_chain *topo_get_nat_chain(t_topo_info *topo)
{
  int i;
  t_topo_nat_chain *cur, *res = NULL;
  for (i=0; i< topo->nb_nat; i++)
    {
    cur = (t_topo_nat_chain *) clownix_malloc(sizeof(t_topo_nat_chain), 3);
    memset(cur, 0, sizeof(t_topo_nat_chain));
    memcpy(&(cur->nat), &(topo->nat[i]), sizeof(t_topo_nat));
    cur->next = res;
    if (res)
      res->prev = cur;
    res = cur;
    }
  return res;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_topo_phy_chain *topo_get_phy_chain(t_topo_info *topo)
{
  int i;
  t_topo_phy_chain *cur, *res = NULL;
  for (i=0; i< topo->nb_phy; i++)
    {
    cur = (t_topo_phy_chain *) clownix_malloc(sizeof(t_topo_phy_chain), 3);
    memset(cur, 0, sizeof(t_topo_phy_chain));
    memcpy(&(cur->phy), &(topo->phy[i]), sizeof(t_topo_phy));
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
static void topo_free_cnt_chain(t_topo_cnt_chain *ch)
{
  t_topo_cnt_chain *next, *cur = ch;
  while(cur)
    {
    next = cur->next;
    clownix_free(cur, __FUNCTION__);
    cur = next;
    }
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
static void topo_free_tap_chain(t_topo_tap_chain *ch)
{
  t_topo_tap_chain *next, *cur = ch;
  while(cur)
    {
    next = cur->next;
    clownix_free(cur, __FUNCTION__);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void topo_free_a2b_chain(t_topo_a2b_chain *ch)
{
  t_topo_a2b_chain *next, *cur = ch;
  while(cur)
    {
    next = cur->next;
    clownix_free(cur, __FUNCTION__);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void topo_free_nat_chain(t_topo_nat_chain *ch)
{
  t_topo_nat_chain *next, *cur = ch;
  while(cur)
    {
    next = cur->next;
    clownix_free(cur, __FUNCTION__);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void topo_free_phy_chain(t_topo_phy_chain *ch)
{
  t_topo_phy_chain *next, *cur = ch;
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
void take_out_from_cnt_chain(t_topo_cnt_chain **ch, t_topo_info *topo)
{
  t_topo_cnt_chain *next, *cur = *ch;
  while (cur)
    {
    next = cur->next;
    if (topo_find_cnt(cur->cnt.name, topo))
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
void take_out_from_tap_chain(t_topo_tap_chain **ch, t_topo_info *topo)
{
  t_topo_tap_chain *next, *cur = *ch;
  while (cur)
    {
    next = cur->next;
    if (topo_find_tap(cur->tap.name, topo))
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
void take_out_from_a2b_chain(t_topo_a2b_chain **ch, t_topo_info *topo)
{
  t_topo_a2b_chain *next, *cur = *ch;
  while (cur)
    {
    next = cur->next;
    if (topo_find_a2b(cur->a2b.name, topo))
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
void take_out_from_nat_chain(t_topo_nat_chain **ch, t_topo_info *topo)
{
  t_topo_nat_chain *next, *cur = *ch;
  while (cur)
    {
    next = cur->next;
    if (topo_find_nat(cur->nat.name, topo))
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
void take_out_from_phy_chain(t_topo_phy_chain **ch, t_topo_info *topo)
{
  t_topo_phy_chain *next, *cur = *ch;
  while (cur)
    {
    next = cur->next;
    if (topo_find_phy(cur->phy.name, topo))
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

  diffs.add_cnt     = topo_get_cnt_chain(newt);
  if (oldt)
    diffs.del_cnt   = topo_get_cnt_chain(oldt);

  diffs.add_kvm     = topo_get_kvm_chain(newt);
  if (oldt)
    diffs.del_kvm   = topo_get_kvm_chain(oldt);

  diffs.add_c2c     = topo_get_c2c_chain(newt);
  if (oldt)
    diffs.del_c2c   = topo_get_c2c_chain(oldt);

  diffs.add_tap     = topo_get_tap_chain(newt);
  if (oldt)
    diffs.del_tap   = topo_get_tap_chain(oldt);

  diffs.add_a2b     = topo_get_a2b_chain(newt);
  if (oldt)
    diffs.del_a2b   = topo_get_a2b_chain(oldt);

  diffs.add_nat     = topo_get_nat_chain(newt);
  if (oldt)
    diffs.del_nat   = topo_get_nat_chain(oldt);

  diffs.add_phy     = topo_get_phy_chain(newt);
  if (oldt)
    diffs.del_phy   = topo_get_phy_chain(oldt);

  diffs.add_lan     = topo_get_lan_chain(newt);
  if (oldt)
    diffs.del_lan   = topo_get_lan_chain(oldt);

  diffs.add_edge    = topo_get_edge_chain(newt);
  if (oldt)
    diffs.del_edge  = topo_get_edge_chain(oldt);

  if (diffs.add_cnt && oldt)
    take_out_from_cnt_chain(&(diffs.add_cnt), oldt);

  if (diffs.del_cnt && newt)
    take_out_from_cnt_chain(&(diffs.del_cnt), newt);

  if (diffs.add_kvm && oldt)
    take_out_from_kvm_chain(&(diffs.add_kvm), oldt);

  if (diffs.del_kvm && newt)
    take_out_from_kvm_chain(&(diffs.del_kvm), newt);

  if (diffs.add_c2c && oldt)
    take_out_from_c2c_chain(&(diffs.add_c2c), oldt);

  if (diffs.del_c2c && newt)
    take_out_from_c2c_chain(&(diffs.del_c2c), newt);

  if (diffs.add_tap && oldt)
    take_out_from_tap_chain(&(diffs.add_tap), oldt);

  if (diffs.del_tap && newt)
    take_out_from_tap_chain(&(diffs.del_tap), newt);

  if (diffs.add_a2b && oldt)
    take_out_from_a2b_chain(&(diffs.add_a2b), oldt);

  if (diffs.del_a2b && newt)
    take_out_from_a2b_chain(&(diffs.del_a2b), newt);

  if (diffs.add_nat && oldt)
    take_out_from_nat_chain(&(diffs.add_nat), oldt);

  if (diffs.del_nat && newt)
    take_out_from_nat_chain(&(diffs.del_nat), newt);

  if (diffs.add_phy && oldt)
    take_out_from_phy_chain(&(diffs.add_phy), oldt);

  if (diffs.del_phy && newt)
    take_out_from_phy_chain(&(diffs.del_phy), newt);

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
  topo_free_cnt_chain(diffs->add_cnt);
  topo_free_kvm_chain(diffs->add_kvm);
  topo_free_c2c_chain(diffs->add_c2c);
  topo_free_tap_chain(diffs->add_tap);
  topo_free_a2b_chain(diffs->add_a2b);
  topo_free_nat_chain(diffs->add_nat);
  topo_free_phy_chain(diffs->add_phy);
  topo_free_lan_chain(diffs->add_lan);
  topo_free_edge_chain(diffs->add_edge);
  topo_free_cnt_chain(diffs->del_cnt);
  topo_free_kvm_chain(diffs->del_kvm);
  topo_free_c2c_chain(diffs->del_c2c);
  topo_free_tap_chain(diffs->del_tap);
  topo_free_a2b_chain(diffs->del_a2b);
  topo_free_nat_chain(diffs->del_nat);
  topo_free_phy_chain(diffs->del_phy);
  topo_free_lan_chain(diffs->del_lan);
  topo_free_edge_chain(diffs->del_edge);
}
/*--------------------------------------------------------------------------*/

