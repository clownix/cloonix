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


/*****************************************************************************/
static int topo_vlg_diff(t_lan_group *vlg, t_lan_group *ref)
{
  int i;
  if (vlg->nb_lan != ref->nb_lan)
    return 1;
  for (i=0; i<vlg->nb_lan; i++)
    {
    if (strcmp(vlg->lan[i].lan, ref->lan[i].lan))
      return 1;
    }
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int topo_compare(t_topo_info *topo, t_topo_info *ref)
{
  int i;
  if (topo->nb_kvm  != ref->nb_kvm)
    return 1;
  if (topo->nb_c2c  != ref->nb_c2c)
    return 2;
  if (topo->nb_snf  != ref->nb_snf)
    return 3;
  if (topo->nb_sat  != ref->nb_sat)
    return 4;
  if (topo->nb_endp != ref->nb_endp)
    return 5;
  if (memcmp(&(topo->clc), &(ref->clc), sizeof(t_topo_clc)))
    return 5;
  if (topo->nb_kvm)
    {
    if (!topo->kvm)
      return 6;
    if (!ref->kvm)
      return 7;
    for (i=0; i<topo->nb_kvm; i++)
      {
      if (memcmp(&(topo->kvm[i]), &(ref->kvm[i]), sizeof(t_topo_kvm)))
        return (1000+i);
      }
    }
  if (topo->nb_c2c)
    {
    if (!topo->c2c)
      return 8;
    if (!ref->c2c)
      return 9;
    for (i=0; i<topo->nb_c2c; i++)
      {
      if (memcmp(&(topo->c2c[i]), &(ref->c2c[i]), sizeof(t_topo_c2c)))
        return (2000+i);
      }
    }
  if (topo->nb_snf)
    {
    if (!topo->snf)
      return 10;
    if (!ref->snf)
      return 11;
    for (i=0; i<topo->nb_snf; i++)
      {
      if (memcmp(&(topo->snf[i]), &(ref->snf[i]), sizeof(t_topo_snf)))
        return (3000+i);
      }
    }
  if (topo->nb_sat)
    {
    if (!topo->sat)
      return 12;
    if (!ref->sat)
      return 13;
    for (i=0; i<topo->nb_sat; i++)
      {
      if (memcmp(&(topo->sat[i]), &(ref->sat[i]), sizeof(t_topo_sat)))
        return (4000+i);
      }
    }
  if (topo->nb_endp)
    {
    if (!topo->endp)
      return 14;
    if (!ref->endp)
      return 15;
    for (i=0; i<topo->nb_endp; i++)
      {
      if (strcmp(topo->endp[i].name, ref->endp[i].name))
        return 16;
      if (topo->endp[i].num != ref->endp[i].num)
        return 17;
      if (topo->endp[i].type != ref->endp[i].type)
        return 18;
      if (topo_vlg_diff(&(topo->endp[i].lan), &(ref->endp[i].lan)))
        return 19;
      }
    }
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void topo_vlg_dup(t_lan_group *vlg, t_lan_group *ref)
{
  int i, len;
  vlg->nb_lan = ref->nb_lan;
  len = vlg->nb_lan * sizeof(t_lan_group_item);
  vlg->lan = (t_lan_group_item *) clownix_malloc(len, 24);
  memset(vlg->lan, 0, len);
  for (i=0; i<vlg->nb_lan; i++)
    {
    strncpy(vlg->lan[i].lan, ref->lan[i].lan, MAX_NAME_LEN-1);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
t_topo_info *topo_duplicate(t_topo_info *ref)
{
  int i;
  t_topo_info  *topo;
  topo = (t_topo_info *) clownix_malloc(sizeof(t_topo_info), 25);
  memset(topo, 0, sizeof(t_topo_info));
  
  topo->nb_kvm = ref->nb_kvm;
  topo->nb_c2c = ref->nb_c2c;
  topo->nb_snf = ref->nb_snf;
  topo->nb_sat = ref->nb_sat;
  topo->nb_endp = ref->nb_endp;

  memcpy(&(topo->clc), &(ref->clc), sizeof(t_topo_clc));

  if (topo->nb_kvm)
    {
    topo->kvm = 
    (t_topo_kvm *)clownix_malloc(ref->nb_kvm*sizeof(t_topo_kvm),26);
    memcpy(topo->kvm, ref->kvm, ref->nb_kvm*sizeof(t_topo_kvm)); 
    }

  if (topo->nb_c2c)
    {
    topo->c2c = 
    (t_topo_c2c *)clownix_malloc(ref->nb_c2c*sizeof(t_topo_c2c),28);
    memcpy(topo->c2c, ref->c2c, ref->nb_c2c*sizeof(t_topo_c2c));
    } 

  if (topo->nb_snf)
    {
    topo->snf = 
    (t_topo_snf *)clownix_malloc(ref->nb_snf*sizeof(t_topo_snf),28);
    memcpy(topo->snf, ref->snf, ref->nb_snf*sizeof(t_topo_snf));
    } 

  if (topo->nb_sat)
    {
    topo->sat =
    (t_topo_sat *)clownix_malloc(ref->nb_sat*sizeof(t_topo_sat),28);
    memcpy(topo->sat, ref->sat, ref->nb_sat*sizeof(t_topo_sat));
    } 

  if (topo->nb_endp)
    {
    topo->endp =
    (t_topo_endp *)clownix_malloc(ref->nb_endp*sizeof(t_topo_endp),28);
    memset(topo->endp, 0, ref->nb_endp*sizeof(t_topo_endp));
    for (i=0; i<topo->nb_endp; i++)
      {
      strncpy(topo->endp[i].name, ref->endp[i].name, MAX_NAME_LEN-1);
      topo->endp[i].num  = ref->endp[i].num;
      topo->endp[i].type = ref->endp[i].type;
      topo_vlg_dup(&(topo->endp[i].lan), &(ref->endp[i].lan));
      }

    } 

  return topo;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void topo_free_topo(t_topo_info *topo)
{
  int i;
  if (topo)
    {
    for (i=0; i<topo->nb_endp; i++)
      clownix_free(topo->endp[i].lan.lan, __FUNCTION__);
    clownix_free(topo->kvm, __FUNCTION__);
    clownix_free(topo->c2c, __FUNCTION__);
    clownix_free(topo->snf, __FUNCTION__);
    clownix_free(topo->sat, __FUNCTION__);
    clownix_free(topo->endp, __FUNCTION__);
    clownix_free(topo, __FUNCTION__);
    }
}
/*---------------------------------------------------------------------------*/

