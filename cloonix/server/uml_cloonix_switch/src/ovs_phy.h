/*****************************************************************************/
/*    Copyright (C) 2006-2024 clownix@clownix.net License AGPL-3             */
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
typedef struct t_ovs_phy
{
  char name[MAX_NAME_LEN];
  unsigned char mac_addr[8];
  char vhost[MAX_NAME_LEN];
  char lan[MAX_NAME_LEN];
  char lan_added[MAX_NAME_LEN];
  int  del_phy_req;
  int  del_snf_ethv_sent;
  int  llid;
  int  tid;
  int  phy_id;
  int  num_macvlan;
  int  endp_type;
  struct t_ovs_phy *prev;
  struct t_ovs_phy *next;
} t_ovs_phy;

t_ovs_phy *ovs_phy_get_first(int *nb_phy);
void ovs_phy_resp_msg_phy(int is_ko, int is_add, char *name, int num, char *vhost);
void ovs_phy_resp_add_lan(int is_ko, char *name, int num,
                          char *vhost, char *lan);
void ovs_phy_resp_del_lan(int is_ko, char *name, int num,
                          char *vhost, char *lan);
int  ovs_phy_dyn_snf(char *name, int val);
void ovs_phy_add(int llid, int tid, char *name, int type);
void ovs_phy_del(int llid, int tid, char *name);
void ovs_phy_add_lan(int llid, int tid, char *name, char *lan);
void ovs_phy_del_lan(int llid, int tid, char *name, char *lan);
t_topo_endp *ovs_phy_translate_topo_endp(int *nb);
t_ovs_phy *ovs_phy_exists_vhost(char *name);
void ovs_phy_init(void);
/*--------------------------------------------------------------------------*/
