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
  int i, j;
  if (topo->nb_cnt  != ref->nb_cnt)
    return 1;
  if (topo->nb_kvm  != ref->nb_kvm)
    return 1;
  if (topo->nb_d2d  != ref->nb_d2d)
    return 6;
  if (topo->nb_tap  != ref->nb_tap)
    return 7;
  if (topo->nb_a2b  != ref->nb_a2b)
    return 8;
  if (topo->nb_nat  != ref->nb_nat)
    return 3;
  if (topo->nb_phy  != ref->nb_phy)
    return 41;
  if (topo->nb_bridges != ref->nb_bridges)
    return 5;
  if (topo->nb_endp != ref->nb_endp)
    return 7;
  if (memcmp(&(topo->clc), &(ref->clc), sizeof(t_topo_clc)))
    return 5;
  if (topo->nb_cnt)
    {
    if (!topo->cnt)
      return 6;
    if (!ref->cnt)
      return 7;
    for (i=0; i<topo->nb_cnt; i++)
      {
      if (memcmp(&(topo->cnt[i]), &(ref->cnt[i]), sizeof(t_topo_cnt)))
        {
        KERR("%s %s", topo->cnt[i].name, ref->cnt[i].name);
        KERR("%s %s", topo->cnt[i].image, ref->cnt[i].image);
        KERR("%d %d", topo->cnt[i].nb_tot_eth, ref->cnt[i].nb_tot_eth);
for(j=0; j<topo->cnt[i].nb_tot_eth; j++)
{
        KERR("%d %d %s", topo->cnt[i].eth_table[j].endp_type,
                         topo->cnt[i].eth_table[j].randmac,
                         topo->cnt[i].eth_table[j].vhost_ifname);

KERR("%hhu %hhu %hhu %hhu %hhu %hhu", 
topo->cnt[i].eth_table[j].mac_addr[0],
topo->cnt[i].eth_table[j].mac_addr[1],
topo->cnt[i].eth_table[j].mac_addr[2],
topo->cnt[i].eth_table[j].mac_addr[3],
topo->cnt[i].eth_table[j].mac_addr[4],
topo->cnt[i].eth_table[j].mac_addr[5]);

KERR("%hhu %hhu %hhu %hhu %hhu %hhu", 
ref->cnt[i].eth_table[j].mac_addr[0],
ref->cnt[i].eth_table[j].mac_addr[1],
ref->cnt[i].eth_table[j].mac_addr[2],
ref->cnt[i].eth_table[j].mac_addr[3],
ref->cnt[i].eth_table[j].mac_addr[4],
ref->cnt[i].eth_table[j].mac_addr[5]);

        KERR("%d %d %s", ref->cnt[i].eth_table[j].endp_type,
                         ref->cnt[i].eth_table[j].randmac,
                         ref->cnt[i].eth_table[j].vhost_ifname);

}
        return (1000+i);
        }
      }
    }
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

  if (topo->nb_tap)
    {
    if (!topo->tap)
      return 218;
    if (!ref->tap)
      return 219;
    for (i=0; i<topo->nb_tap; i++)
      {
      if (memcmp(&(topo->tap[i]), &(ref->tap[i]), sizeof(t_topo_tap)))
        return (351+i);
      }
    }

  if (topo->nb_a2b)
    {
    if (!topo->a2b)
      return 218;
    if (!ref->a2b)
      return 219;
    for (i=0; i<topo->nb_a2b; i++)
      {
      if (memcmp(&(topo->a2b[i]), &(ref->a2b[i]), sizeof(t_topo_a2b)))
        return (351+i);
      }
    }


  if (topo->nb_nat)
    {
    if (!topo->nat)
      return 12;
    if (!ref->nat)
      return 13;
    for (i=0; i<topo->nb_nat; i++)
      {
      if (memcmp(&(topo->nat[i]), &(ref->nat[i]), sizeof(t_topo_nat)))
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
  
  topo->nb_cnt = ref->nb_cnt;
  topo->nb_kvm = ref->nb_kvm;
  topo->nb_d2d = ref->nb_d2d;
  topo->nb_tap = ref->nb_tap;
  topo->nb_a2b = ref->nb_a2b;
  topo->nb_nat = ref->nb_nat;
  topo->nb_phy = ref->nb_phy;
  topo->nb_bridges = ref->nb_bridges;
  topo->nb_endp = ref->nb_endp;

  memcpy(&(topo->clc), &(ref->clc), sizeof(t_topo_clc));

  if (topo->nb_cnt)
    {
    topo->cnt =
    (t_topo_cnt *)clownix_malloc(ref->nb_cnt*sizeof(t_topo_cnt),26);
    memcpy(topo->cnt, ref->cnt, ref->nb_cnt*sizeof(t_topo_cnt));
    }

  if (topo->nb_kvm)
    {
    topo->kvm = 
    (t_topo_kvm *)clownix_malloc(ref->nb_kvm*sizeof(t_topo_kvm),26);
    memcpy(topo->kvm, ref->kvm, ref->nb_kvm*sizeof(t_topo_kvm)); 
    }

  if (topo->nb_d2d)
    {
    topo->d2d =
    (t_topo_d2d *)clownix_malloc(ref->nb_d2d*sizeof(t_topo_d2d),28);
    memcpy(topo->d2d, ref->d2d, ref->nb_d2d*sizeof(t_topo_d2d));
    }

  if (topo->nb_tap)
    {
    topo->tap =
    (t_topo_tap *)clownix_malloc(ref->nb_tap*sizeof(t_topo_tap),28);
    memcpy(topo->tap, ref->tap, ref->nb_tap*sizeof(t_topo_tap));
    }

  if (topo->nb_a2b)
    {
    topo->a2b =
    (t_topo_a2b *)clownix_malloc(ref->nb_a2b*sizeof(t_topo_a2b),28);
    memcpy(topo->a2b, ref->a2b, ref->nb_a2b*sizeof(t_topo_a2b));
    }


  if (topo->nb_nat)
    {
    topo->nat =
    (t_topo_nat *)clownix_malloc(ref->nb_nat*sizeof(t_topo_nat),28);
    memcpy(topo->nat, ref->nat, ref->nb_nat*sizeof(t_topo_nat));
    }

  if (topo->nb_phy)
    {
    topo->phy =
    (t_topo_phy *)clownix_malloc(ref->nb_phy*sizeof(t_topo_phy),28);
    memcpy(topo->phy, ref->phy, ref->nb_phy*sizeof(t_topo_phy));
    }

  if (topo->nb_bridges)
    {
    topo->bridges =
    (t_topo_bridges *)clownix_malloc(ref->nb_bridges*sizeof(t_topo_bridges),28);
    memcpy(topo->bridges, ref->bridges, ref->nb_bridges*sizeof(t_topo_bridges));
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
    clownix_free(topo->cnt, __FUNCTION__);
    clownix_free(topo->kvm, __FUNCTION__);
    clownix_free(topo->d2d, __FUNCTION__);
    clownix_free(topo->tap, __FUNCTION__);
    clownix_free(topo->a2b, __FUNCTION__);
    clownix_free(topo->nat, __FUNCTION__);
    clownix_free(topo->phy, __FUNCTION__);
    clownix_free(topo->info_phy, __FUNCTION__);
    clownix_free(topo->bridges, __FUNCTION__);
    clownix_free(topo->endp, __FUNCTION__);
    clownix_free(topo, __FUNCTION__);
    }
}
/*---------------------------------------------------------------------------*/

