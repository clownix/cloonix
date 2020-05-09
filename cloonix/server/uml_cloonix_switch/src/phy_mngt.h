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
typedef struct t_phy_mngt_vhost
{
  char peer_name[MAX_NAME_LEN];
  int peer_num;
} t_phy_mngt_vhost;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
typedef struct t_phy
{
  char name[MAX_NAME_LEN];
  int endp_type;
  int eth_type;
  int llid;
  int tid;
  int flag_lan_ok;
  int dpdk_attach_ok;
  t_lan_group lan;
  t_phy_mngt_vhost vhost;
  struct t_phy *prev;
  struct t_phy *next;
} t_phy;
/*--------------------------------------------------------------------------*/
t_phy *phy_mngt_lan_find(char *lan);
t_phy *phy_mngt_phy_find(char *name);
void phy_mngt_phy_free(t_phy *cur);
void phy_mngt_lowest_clean_lan(t_phy *cur);
t_phy *phy_mngt_get_head_phy(int *nb_phy);
t_topo_endp *phy_mngt_translate_topo_endp(int *nb);
int  phy_mngt_lan_exists(char *lan, int *eth_type, int *endp_type);
int  phy_mngt_add_lan(int llid, int tid, char *name, char *lan, char *err);
int  phy_mngt_del_lan(int llid, int tid, char *name, char *lan, char *err);
int  phy_mngt_add(int llid, int tid, char *name, int endp_type);
int  phy_mngt_del(int llid, int tid, char *name);
void phy_mngt_del_all(void);
int  phy_mngt_get_qty(void);
int  phy_mngt_exists(char *name, int *endp_type);
void phy_mngt_set_eth_type(t_phy *cur, int eth_type);
void phy_mngt_init(void);
/*--------------------------------------------------------------------------*/
