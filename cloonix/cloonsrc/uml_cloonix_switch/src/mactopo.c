/*****************************************************************************/
/*    Copyright (C) 2006-2025 clownix@clownix.net License AGPL-3             */
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

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_daemon.h"
#include "cfg_store.h"
#include "hop_event.h"
#include "lan_to_name.h"
#include "mactopo.h"
#include "ovs_a2b.h"
#include "fmt_diag.h"

/*---------------------------------------------------------------------------*/
typedef struct t_item_mactopo
{
  int item_type;
  char name[MAX_NAME_LEN];
  int num;
  char lan[MAX_NAME_LEN];
  char vhost[MAX_NAME_LEN];
  char mac[MAX_NAME_LEN];
  int llid_snf;
  struct t_item_mactopo *prev;
  struct t_item_mactopo *next;
} t_item_mactopo;
/*---------------------------------------------------------------------------*/

static t_item_mactopo *g_head_topo;
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static t_item_mactopo *find_cur(char *name, int num)
{
  t_item_mactopo *cur = g_head_topo;
  while (cur)
    {
    if ((!strcmp(name, cur->name)) && (num == cur->num))
      break;
    cur = cur->next;
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_item_mactopo *find_lan_phym(char *lan)
{
  t_item_mactopo *ocur, *cur = g_head_topo;
  t_ovs_a2b *a2b;
  char *olan;
  int num;
  while (cur)
    {
    if ((cur->item_type == item_phym) && (!strcmp(lan, cur->lan)))
      break;
    cur = cur->next;
    }
  if (cur == NULL)
    {
    cur = g_head_topo;
    while (cur)
      {
      if (cur->item_type == item_a2b)
        {
        ocur = NULL;
        a2b = ovs_a2b_exists(cur->name);
        if (a2b)
          {
          num = !(cur->num);
          olan = a2b->side[num].lan_added;
          if (strlen(olan))
            {
            ocur = g_head_topo;
            while (ocur)
              {
              if ((ocur->item_type == item_phym) &&
                  (!strcmp(olan, ocur->lan)))
                break;
              ocur = ocur->next;
              }
            }
          }
        cur = ocur;
        break;
        }
      cur = cur->next;
      }
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void alloc_cur(int item_type, char *name, int num,
                      char *lan, char *vhost, char *mac)
{
  t_item_mactopo *cur;
  cur = (t_item_mactopo *) malloc(sizeof(t_item_mactopo));
  memset(cur, 0, sizeof(t_item_mactopo));
  cur->item_type = item_type;
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  cur->num = num;
  strncpy(cur->lan, lan, MAX_NAME_LEN-1);
  if (vhost)
    strncpy(cur->vhost, vhost, MAX_NAME_LEN-1);
  strncpy(cur->mac, mac, MAX_NAME_LEN-1);
  if (g_head_topo)
    g_head_topo->prev = cur;
  cur->next = g_head_topo;
  g_head_topo = cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void free_cur(t_item_mactopo *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_topo)
    g_head_topo = cur->next;
  free(cur);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int type_exists_with_lan(char *lan, int type)
{
  int result = 0;
  t_item_mactopo *cur = g_head_topo;
  while (cur)
    {
    if ((!strcmp(cur->lan, lan)) && (cur->item_type == type))
      {
      result = 1;
      break;
      }
    cur = cur->next;
    }
  return result;
}
/*---------------------------------------------------------------------------*/
  
/****************************************************************************/
static t_item_mactopo *get_lan_a2b_vm_cnt_mac(char *a2b_name, int a2b_num)
{
  t_ovs_a2b *a2b = ovs_a2b_exists(a2b_name);
  t_item_mactopo *cur = NULL;
  char *lan;
  int num;
  if (a2b)
    {
    num = !(a2b_num);
    lan = a2b->side[num].lan_added;
    if (strlen(lan))
      {
      cur = g_head_topo;
      while(cur)
        {
        if ((!strcmp(cur->lan, lan)) && (strlen(cur->mac)))
          {
          if ((cur->item_type == item_cnt) || (cur->item_type == item_kvm))
            break;
          }
        cur = cur->next;
        }
      }
    if (cur == NULL)
      {
      cur = g_head_topo;
      lan = a2b->side[a2b_num].lan_added;
      if (strlen(lan))
        {
        while(cur)
          { 
          if ((!strcmp(cur->lan, lan)) && (strlen(cur->mac)))
            {
            if ((cur->item_type == item_cnt) || (cur->item_type == item_kvm))
              break;
            }
          cur = cur->next;
          }
        }
      }
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static char *get_vm_cnt_mac(char *lan)
{
  t_item_mactopo *cur_a2b, *cur = g_head_topo;
  char *result = NULL;
  while(cur)
    {
    if (cur->item_type == item_a2b)
      {
      cur_a2b = get_lan_a2b_vm_cnt_mac(cur->name, cur->num);
      if (cur_a2b)
        {
        result = cur_a2b->mac;
        break;
        }
      }
    else if ((cur->item_type == item_cnt) || (cur->item_type == item_kvm))
      {
      if ((!strcmp(cur->lan, lan)) && (strlen(cur->mac)))
        {
        result = cur->mac;
        break;
        }
      }
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void snf_update_macs(t_item_mactopo *myself)
{
  char *lan, msg[MAX_PATH_LEN];
  t_item_mactopo *cur;
  char *mac;
  lan = myself->lan;
  snprintf(msg, MAX_PATH_LEN-1, "cloonsnf_mac_spyed_purge");
  rpct_send_sigdiag_msg(myself->llid_snf, type_hop_snf, msg);
  hop_event_hook(myself->llid_snf, FLAG_HOP_SIGDIAG, msg);
  if ((myself->item_type == item_c2c) ||
      (myself->item_type == item_a2b))
    {
    cur = g_head_topo;
    while (cur)
      {
      if ((cur != myself) && (!strcmp(cur->lan, lan)) && (strlen(cur->mac)))
        {
        snprintf(msg,MAX_PATH_LEN-1,"cloonsnf_mac_spyed_rx_on=%s",cur->mac);
        rpct_send_sigdiag_msg(myself->llid_snf, type_hop_snf, msg);
        hop_event_hook(myself->llid_snf, FLAG_HOP_SIGDIAG, msg);
        }
      cur = cur->next;
      }
    }
  else if (myself->item_type == item_nat)
    {
    snprintf(msg, MAX_PATH_LEN-1,"cloonsnf_mac_spyed_tx_on=%s",NAT_MAC_CISCO);
    rpct_send_sigdiag_msg(myself->llid_snf, type_hop_snf, msg);
    hop_event_hook(myself->llid_snf, FLAG_HOP_SIGDIAG, msg);
    memset(msg, 0, MAX_PATH_LEN);
    snprintf(msg, MAX_PATH_LEN-1,"cloonsnf_mac_spyed_tx_on=%s",NAT_MAC_GW);
    rpct_send_sigdiag_msg(myself->llid_snf, type_hop_snf, msg);
    hop_event_hook(myself->llid_snf, FLAG_HOP_SIGDIAG, msg);
    memset(msg, 0, MAX_PATH_LEN);
    snprintf(msg, MAX_PATH_LEN-1,"cloonsnf_mac_spyed_tx_on=%s",NAT_MAC_DNS);
    rpct_send_sigdiag_msg(myself->llid_snf, type_hop_snf, msg);
    hop_event_hook(myself->llid_snf, FLAG_HOP_SIGDIAG, msg);
    }
  else if ((myself->item_type == item_phya)  ||
           (myself->item_type == item_phym)  ||
           (myself->item_type == item_tap))
    {
    mac = get_vm_cnt_mac(lan);
    if (mac)
      {
      snprintf(msg, MAX_PATH_LEN-1, "cloonsnf_mac_spyed_rx_on=%s", mac);
      rpct_send_sigdiag_msg(myself->llid_snf, type_hop_snf, msg);
      hop_event_hook(myself->llid_snf, FLAG_HOP_SIGDIAG, msg);
      }
    }
  else if ((myself->item_type == item_cnt)   ||
           (myself->item_type == item_kvm))
    {
    snprintf(msg, MAX_PATH_LEN-1,"cloonsnf_mac_spyed_tx_on=%s", myself->mac);
    rpct_send_sigdiag_msg(myself->llid_snf, type_hop_snf, msg);
    hop_event_hook(myself->llid_snf, FLAG_HOP_SIGDIAG, msg);
    }
  else
    KOUT("ERROR %d", myself->item_type);
} 
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void all_snf_update_macs(char *lan)
{
  t_item_mactopo *cur = g_head_topo;
  while (cur)
    {
    if ((cur->llid_snf) && (!strcmp(cur->lan, lan)))
      {
      snf_update_macs(cur);
      }
    cur = cur->next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void macvlan_change(t_item_mactopo *cur, int item_type, char *lan)
{
  t_item_mactopo *cur_a2b, *cur_phym = find_lan_phym(lan);
  if (cur_phym)
    {
    if ((item_type == item_cnt) || (item_type == item_kvm))
      {
      if (fmt_tx_macvlan_mac(1, cur_phym->vhost, cur->mac))
        KERR("ERROR LAN: %s %s %s", lan, cur->vhost, cur->mac);
      else
        {
        memset(cur_phym->mac, 0, MAX_NAME_LEN);
        strncpy(cur_phym->mac, cur->mac, MAX_NAME_LEN-1);
        }
      }
    else if (item_type == item_a2b)
      {
      cur_a2b = get_lan_a2b_vm_cnt_mac(cur->name, cur->num);
      if (cur_a2b)
        {
        if (fmt_tx_macvlan_mac(1, cur_phym->vhost, cur_a2b->mac))
          KERR("ERROR LAN: %s %s %s", lan, cur_phym->vhost, cur_a2b->mac);
        else
          {
          memset(cur_phym->mac, 0, MAX_NAME_LEN);
          strncpy(cur_phym->mac, cur_a2b->mac, MAX_NAME_LEN-1);
          }
        }
      }
    else
      KOUT("ERROR %d", item_type);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int check_add_req(int item_type, char *name, int num, char *lan,
                         char *vhost, unsigned char *mac_addr, char *err,
                         char *mac)
{
  int result = -1;
  memset(mac, 0, MAX_PATH_LEN);
  memset(err, 0, MAX_PATH_LEN);
  if ((!name) || (strlen(name) == 0) || (strlen(name) >= MAX_NAME_LEN))
    KOUT(" ");
  if ((!lan) || (strlen(lan) == 0) || (strlen(lan) >= MAX_NAME_LEN))
    KOUT(" ");
  if ((item_type == item_cnt)  ||
      (item_type == item_kvm)  ||
      (item_type == item_phym) ||
      (item_type == item_phya) ||
      (item_type == item_tap))
    { 
    if ((!mac_addr) || (mac_addr[0] == 0))
      KOUT(" ");
    if ((!vhost) || (strlen(vhost) == 0))
      KOUT(" ");
    sprintf(mac,"%02X:%02X:%02X:%02X:%02X:%02X",
            mac_addr[0] & 0xFF, mac_addr[1] & 0xFF, mac_addr[2] & 0xFF,
            mac_addr[3] & 0xFF, mac_addr[4] & 0xFF, mac_addr[5] & 0xFF);
    } 
  if (find_cur(name, num))
    {
    snprintf(err, MAX_PATH_LEN-1, "Record %s %d exists", name, num);
    KERR("ERROR %s %d lan exists, %s cannot be added",name, num, lan);
    }
  else
    {
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void mactopo_snf_add(char *name, int num, char *lan, int llid)
{
  t_item_mactopo *cur = find_cur(name, num);
  if (!cur)
    KERR("ERROR %s %d %s", name, num, lan);
  else if (strcmp(lan, cur->lan))
    KERR("ERROR %s %d %s %s", name, num, lan, cur->lan);
  else if (cur->llid_snf)
    KERR("ERROR %s %d %s %d %d", name, num, lan, cur->llid_snf, llid);
  else if (llid == 0)
    KERR("ERROR %s %d %s", name, num, lan);
  else
    {
    cur->llid_snf = llid;
    snf_update_macs(cur);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void mactopo_del_req(int item_type, char *name, int num, char *lan)
{
  t_item_mactopo *cur = find_cur(name, num);
  if (!cur)
    KERR("ERROR %s %d %s", name, num, lan);
  else
    {
    if (strcmp(lan, cur->lan))
      KERR("ERROR %s %d %s %s", name, num, lan, cur->lan);
    if (cur->item_type != item_type)
      KERR("ERROR %s %d %d %d",name, num, cur->item_type, item_type);
    free_cur(cur);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void mactopo_snf_del(char *name, int num, char *lan, int llid)
{
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void mactopo_del_resp(int item_type, char *name, int num, char *lan)
{
  t_item_mactopo *cur = find_cur(name, num);
  if (cur)
    KERR("ERROR %s %d %s", name, num, lan);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int mactopo_test_exists(char *name, int num)
{
  int result = 0;
  if (find_cur(name, num))
    result = 1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int mactopo_add_req(int item_type, char *name, int num, char *lan,
                    char *vhost, unsigned char *mac_addr, char *err)
{
  int result;
  char mac[MAX_PATH_LEN];
  if (check_add_req(item_type, name, num, lan, vhost, mac_addr, err, mac))
    {
    result = -1;
    }
  else if (item_type == item_phym)
    {
    if ((type_exists_with_lan(lan, item_tap))  ||
        (type_exists_with_lan(lan, item_phya)) ||
        (type_exists_with_lan(lan, item_phym)) ||
        (type_exists_with_lan(lan, item_nat))  ||
        (type_exists_with_lan(lan, item_c2c)))
      {
      snprintf(err, MAX_PATH_LEN-1, "Incompatible item on lan %s", lan);
      KERR("ERROR %s %d incompatible with item on lan %s", name, num, lan);
      }
    else
      {
      alloc_cur(item_type, name, num, lan, vhost, mac);
      all_snf_update_macs(lan);
      result = 0;
      }
    }
  else if ((item_type == item_tap)  ||
           (item_type == item_phya) ||
           (item_type == item_nat)  ||
           (item_type == item_c2c))
    {
    if (type_exists_with_lan(lan, item_phym))
      {
      snprintf(err, MAX_PATH_LEN-1, "Incompatible item on lan %s", lan);
      KERR("ERROR %s %d incompatible with item on lan %s", name, num, lan);
      }
    else
      {
      alloc_cur(item_type, name, num, lan, vhost, mac);
      all_snf_update_macs(lan);
      result = 0;
      }
    }
  else if ((item_type == item_cnt)  ||
           (item_type == item_kvm)  ||
           (item_type == item_a2b))
    {
    if ((type_exists_with_lan(lan, item_phym)) &&
        ((type_exists_with_lan(lan, item_cnt)) || 
         (type_exists_with_lan(lan, item_kvm)) || 
         (type_exists_with_lan(lan, item_a2b))))
      {
      snprintf(err, MAX_PATH_LEN-1, "Max item reached on lan %s", lan);
      KERR("ERROR %s %d Max item reached on lan %s", name, num, lan);
      }
    else
      {
      alloc_cur(item_type, name, num, lan, vhost, mac);
      all_snf_update_macs(lan);
      result = 0;
      }
    }
  else
    {
    KOUT("ERROR %d", item_type);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void mactopo_add_resp(int is_ok, int item_type, char *name, int num, char *lan)
{
  t_item_mactopo *cur = find_cur(name, num);
  char *mac;
  if (!cur)
    KERR("ERROR %s %d %s", name, num, lan);
  else
    {
    if (strcmp(lan, cur->lan))
      KERR("ERROR %s %d %s %s", name, num, lan, cur->lan);
    if (cur->item_type != item_type)
      KERR("ERROR %s %d %d %d",name, num, cur->item_type, item_type);
    if (is_ok == 0)
      KERR("ERROR %s %d %s", name, num, lan);
    else if ((item_type == item_tap)  ||
             (item_type == item_c2c)  ||
             (item_type == item_nat)  ||
             (item_type == item_phya))
      {
      }
    else if (item_type == item_phym)
      {
      mac = get_vm_cnt_mac(lan);
      if (mac)
        {
        if (fmt_tx_macvlan_mac(1, cur->vhost, mac))
          KERR("WARNING LAN: %s %s %s", lan, cur->vhost, mac);
        else
          {
          memset(cur->mac, 0, MAX_NAME_LEN);
          strncpy(cur->mac, mac, MAX_NAME_LEN-1);
          }
        }
      }
    else if ((item_type == item_cnt)  ||
             (item_type == item_kvm)  ||
             (item_type == item_a2b))
      {
      macvlan_change(cur, item_type, lan);
      }
    else
      KERR("ERROR NOT POSSIBLE %d", item_type);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void mactopo_init(void)
{
  g_head_topo = NULL;
}
/*---------------------------------------------------------------------------*/

