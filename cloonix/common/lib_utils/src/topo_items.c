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
  if (topo->nb_d2d  != ref->nb_d2d)
    return 5;
  if (topo->nb_sat  != ref->nb_sat)
    return 3;
  if (topo->nb_phy  != ref->nb_phy)
    return 41;
  if (topo->nb_pci  != ref->nb_pci)
    return 42;
  if (topo->nb_bridges != ref->nb_bridges)
    return 5;
  if (topo->nb_mirrors != ref->nb_mirrors)
    return 6;
  if (topo->nb_endp != ref->nb_endp)
    return 7;
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
  if (topo->nb_d2d)
    {
    if (!topo->d2d)
      return 118;
    if (!ref->d2d)
      return 119;
    for (i=0; i<topo->nb_d2d; i++)
      {
      if (memcmp(&(topo->d2d[i]), &(ref->d2d[i]), sizeof(t_topo_d2d)))
        return (251+i);
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
  if (topo->nb_phy)
    {
    if (!topo->phy)
      return 12;
    if (!ref->phy)
      return 13;
    for (i=0; i<topo->nb_phy; i++)
      {
      if (memcmp(&(topo->phy[i]), &(ref->phy[i]), sizeof(t_topo_phy)))
        return (4500+i);
      }
    }

  if (topo->nb_pci)
    {
    if (!topo->pci)
      return 12;
    if (!ref->pci)
      return 13;
    for (i=0; i<topo->nb_pci; i++)
      {
      if (memcmp(&(topo->pci[i]), &(ref->pci[i]), sizeof(t_topo_pci)))
        return (4600+i);
      }
    }

  if (topo->nb_bridges)
    {
    if (!topo->bridges)
      return 12;
    if (!ref->bridges)
      return 13;
    for (i=0; i<topo->nb_bridges; i++)
      {
      if (memcmp(&(topo->bridges[i]), &(ref->bridges[i]), sizeof(t_topo_bridges)))
        return (4700+i);
      }
    }

  if (topo->nb_mirrors)
    {
    if (!topo->mirrors)
      return 12;
    if (!ref->mirrors)
      return 13;
    for (i=0; i<topo->nb_mirrors; i++)
      {
      if (memcmp(&(topo->mirrors[i]), &(ref->mirrors[i]), sizeof(t_topo_mirrors)))
        return (4800+i);
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
  topo->nb_d2d = ref->nb_d2d;
  topo->nb_sat = ref->nb_sat;
  topo->nb_phy = ref->nb_phy;
  topo->nb_pci = ref->nb_pci;
  topo->nb_bridges = ref->nb_bridges;
  topo->nb_mirrors = ref->nb_mirrors;
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

  if (topo->nb_d2d)
    {
    topo->d2d =
    (t_topo_d2d *)clownix_malloc(ref->nb_d2d*sizeof(t_topo_d2d),28);
    memcpy(topo->d2d, ref->d2d, ref->nb_d2d*sizeof(t_topo_d2d));
    }

  if (topo->nb_sat)
    {
    topo->sat =
    (t_topo_sat *)clownix_malloc(ref->nb_sat*sizeof(t_topo_sat),28);
    memcpy(topo->sat, ref->sat, ref->nb_sat*sizeof(t_topo_sat));
    } 
  if (topo->nb_phy)
    {
    topo->phy =
    (t_topo_phy *)clownix_malloc(ref->nb_phy*sizeof(t_topo_phy),28);
    memcpy(topo->phy, ref->phy, ref->nb_phy*sizeof(t_topo_phy));
    }

  if (topo->nb_pci)
    {
    topo->pci =
    (t_topo_pci *)clownix_malloc(ref->nb_pci*sizeof(t_topo_pci),28);
    memcpy(topo->pci, ref->pci, ref->nb_pci*sizeof(t_topo_pci));
    }

  if (topo->nb_bridges)
    {
    topo->bridges =
    (t_topo_bridges *)clownix_malloc(ref->nb_bridges*sizeof(t_topo_bridges),28);
    memcpy(topo->bridges, ref->bridges, ref->nb_bridges*sizeof(t_topo_bridges));
    }

  if (topo->nb_mirrors)
    {
    topo->mirrors =
    (t_topo_mirrors *)clownix_malloc(ref->nb_mirrors*sizeof(t_topo_mirrors),28);
    memcpy(topo->mirrors, ref->mirrors, ref->nb_mirrors*sizeof(t_topo_mirrors));
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
    clownix_free(topo->d2d, __FUNCTION__);
    clownix_free(topo->sat, __FUNCTION__);
    clownix_free(topo->phy, __FUNCTION__);
    clownix_free(topo->pci, __FUNCTION__);
    clownix_free(topo->bridges, __FUNCTION__);
    clownix_free(topo->mirrors, __FUNCTION__);
    clownix_free(topo->endp, __FUNCTION__);
    clownix_free(topo, __FUNCTION__);
    }
}
/*---------------------------------------------------------------------------*/

