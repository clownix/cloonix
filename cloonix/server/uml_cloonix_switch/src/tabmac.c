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
#include "commun_daemon.h"
#include "dpdk_ovs.h"
#include "dpdk_nat.h"
#include "dpdk_tap.h"
#include "dpdk_dyn.h"
#include "dpdk_d2d.h"
#include "tabmac.h"
#include "snf_dpdk_process.h"


/*****************************************************************************/
static int nat_mac_collect(t_peer_mac *tabmac, int max)
{
  char *name, *strmac;
  int i, k = 0;
  name = dpdk_nat_get_next(NULL);
  while(name)
    {
    for (i=0; i<3; i++)
      {
      strmac = dpdk_nat_get_mac(name, i);
      if (k < max)
        {
        strncpy(tabmac[k].mac, strmac, MAX_NAME_LEN-1);
        k += 1;
        }
      else
        KERR("INCREASE MAX_PEER_MAC %d %d", k, max);
      }
    name = dpdk_nat_get_next(name);
    }
  return k;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int tap_mac_collect(t_peer_mac *tabmac, int max)
{
  char *name, *strmac;
  int i = 0;
  name = dpdk_tap_get_next(NULL);
  while(name)
    {
    strmac = dpdk_tap_get_mac(name);
    if (strmac)
      {
      if (i < max)
        {
        strncpy(tabmac[i].mac, strmac, MAX_NAME_LEN-1);
        i += 1;
        }
      else
        KERR("INCREASE MAX_PEER_MAC %d %d", i, max);
      }
    name = dpdk_tap_get_next(name);
    }
  return i;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int vm_mac_collect(t_peer_mac *tabmac, int max)
{
  t_vm *vm;
  char *mc, strmac[MAX_NAME_LEN];
  int i, j, nb_vm, k = 0;
  vm = cfg_get_first_vm(&nb_vm);
  for (i=0; i < nb_vm; i++)
    {
    for (j=0; j < vm->kvm.nb_tot_eth; j++)
      {
      if (vm->kvm.eth_table[j].eth_type == eth_type_dpdk)
        {
        mc = vm->kvm.eth_table[j].mac_addr;
        memset(strmac, 0, MAX_NAME_LEN);
        snprintf(strmac, MAX_NAME_LEN-1, "%02x:%02x:%02x:%02x:%02x:%02x",
                                      mc[0]&0xff, mc[1]&0xff, mc[2]&0xff,
                                      mc[3]&0xff, mc[4]&0xff, mc[5]&0xff);
        if (k < max)
          {
          strncpy(tabmac[i].mac, strmac, MAX_NAME_LEN-1);
          k += 1;
          }
        else
          KERR("INCREASE MAX_PEER_MAC %d %d", k, max);
        }
      }
    vm = vm->next;
    }
  return k;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int d2d_snf_mac_collect(t_tabmac_snf *tabmac_snf, int max)
{
  char *name;
  int nb, num, k=0;
  t_peer_mac *tabmac;
  name = dpdk_d2d_get_next(NULL);
  while(name)
    {
    nb = dpdk_d2d_get_strmac(name, &tabmac);
    for (num=0; num<nb; num++)
      {
      if (k < max)
         {
         memset(&tabmac_snf[k], 0, sizeof(t_tabmac_snf));
         strncpy(tabmac_snf[k].name, name, MAX_NAME_LEN-1);
         tabmac_snf[k].num = num;
         strncpy(tabmac_snf[k].mac, tabmac[num].mac, MAX_NAME_LEN-1);
         k += 1;
         } 
      else
        KERR("Increase MAX_PEER_MAC");
      }
    name = dpdk_d2d_get_next(name);
    }
  return k;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int nat_snf_mac_collect(t_tabmac_snf *tabmac_snf, int max)
{
  char *name, *strmac;
  int i, k = 0;
  name = dpdk_nat_get_next(NULL);
  while(name)
    {
    for (i=0; i<3; i++)
      {
      strmac = dpdk_nat_get_mac(name, i);
      if ((strmac) && (k < max))
        {
        memset(&tabmac_snf[k], 0, sizeof(t_tabmac_snf));
        strncpy(tabmac_snf[k].name, name, MAX_NAME_LEN-1);
        tabmac_snf[k].num = i;
        strncpy(tabmac_snf[k].mac, strmac, MAX_NAME_LEN-1);
        k += 1;
        }
      else if (k >= max)
        KERR("Increase MAX_PEER_MAC");
      }
    name = dpdk_nat_get_next(name);
    }
  return k;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int tap_snf_mac_collect(t_tabmac_snf *tabmac_snf, int max)
{
  char *name, *strmac;
  int k = 0;
  name = dpdk_tap_get_next(NULL);
  while(name)
    {
    strmac = dpdk_tap_get_mac(name);
    if ((strmac) && (k < max))
      {
      memset(&tabmac_snf[k], 0, sizeof(t_tabmac_snf));
      strncpy(tabmac_snf[k].name, name, MAX_NAME_LEN-1);
      tabmac_snf[k].num = 0;
      strncpy(tabmac_snf[k].mac, strmac, MAX_NAME_LEN-1);
      k += 1;
      }
    else if (k >= max)
      KERR("Increase MAX_PEER_MAC");
    name = dpdk_tap_get_next(name);
    }
  return k;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int vm_snf_mac_collect(t_tabmac_snf *tabmac_snf, int max)
{
 t_vm *vm;
  char *name, *mc;
  char mac[MAX_NAME_LEN];
  int i, j, nb_vm, k = 0;

  vm = cfg_get_first_vm(&nb_vm);
  for (i=0; i < nb_vm; i++)
    {
    for (j=0; j < vm->kvm.nb_tot_eth; j++)
      {
      if (vm->kvm.eth_table[j].eth_type == eth_type_dpdk)
        {
        name = vm->kvm.name;
        mc = vm->kvm.eth_table[j].mac_addr;
        memset(mac, 0, MAX_NAME_LEN);
        snprintf(mac, MAX_NAME_LEN-1, "%02x:%02x:%02x:%02x:%02x:%02x",
                                    mc[0]&0xff, mc[1]&0xff, mc[2]&0xff,
                                    mc[3]&0xff, mc[4]&0xff, mc[5]&0xff);
        if (k < max)
          {
          memset(&tabmac_snf[k], 0, sizeof(t_tabmac_snf));
          strncpy(tabmac_snf[k].name, name, MAX_NAME_LEN-1);
          tabmac_snf[k].num = j;
          strncpy(tabmac_snf[k].mac, mac, MAX_NAME_LEN-1);
          k += 1;
          }
        else
          KERR("Increase MAX_PEER_MAC");
        }
      }
    vm = vm->next;
    }
  return k;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int tabmac_update_for_snf(t_tabmac_snf tab[MAX_PEER_MAC])
{
  int nb = 0;
  memset(tab, 0, MAX_PEER_MAC * sizeof(t_tabmac_snf));
  nb += d2d_snf_mac_collect(&(tab[nb]), MAX_PEER_MAC - nb);
  nb += nat_snf_mac_collect(&(tab[nb]), MAX_PEER_MAC - nb);
  nb += tap_snf_mac_collect(&(tab[nb]), MAX_PEER_MAC - nb);
  nb += vm_snf_mac_collect(&(tab[nb]),  MAX_PEER_MAC - nb);
  return nb;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int tabmac_update_for_d2d(t_peer_mac tab[MAX_PEER_MAC])
{
  int nb = 0;
  memset(tab, 0, MAX_PEER_MAC * sizeof(t_peer_mac));
  nb += nat_mac_collect(&(tab[nb]), MAX_PEER_MAC - nb);
  nb += tap_mac_collect(&(tab[nb]), MAX_PEER_MAC - nb);
  nb += vm_mac_collect(&(tab[nb]),  MAX_PEER_MAC - nb);
  return nb;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void tabmac_process_possible_change(void)
{
  static int lock = 0;
  if (lock == 0)
    {
    lock = 1;
    snf_dpdk_process_possible_change();
    dpdk_d2d_process_possible_change();
    lock = 0;
    }
}
/*--------------------------------------------------------------------------*/

