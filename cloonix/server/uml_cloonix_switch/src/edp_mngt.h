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
typedef struct t_edp_mngt_vhost
{
  char peer_name[MAX_NAME_LEN];
  int peer_num;
} t_edp_mngt_vhost;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
typedef struct t_edp
{
  char name[MAX_NAME_LEN];
  char lan[MAX_NAME_LEN];
  int flag_lan_ok;
  int endp_type;
  int eth_type;
  int llid;
  int tid;
  int dpdk_attach_ok;
  int count_llid_on;
  int count_flag_ko;
  t_edp_mngt_vhost vhost;
  struct t_edp *prev;
  struct t_edp *next;
} t_edp;
/*--------------------------------------------------------------------------*/
void edp_mngt_cisco_nat_destroy(char *name);
void edp_mngt_cisco_nat_create(char *name);

void edp_mngt_kvm_lan_exists(char *lan, int *dpdk, int *vhost, int *sock);

t_edp *edp_mngt_name_lan_find(char *name, char *lan);
t_edp *edp_mngt_lan_find(char *lan);
t_edp *edp_mngt_edp_find(char *name);
void edp_mngt_edp_free(t_edp *cur);
t_edp *edp_mngt_get_head_edp(int *nb_edp);
t_topo_endp *edp_mngt_translate_topo_endp(int *nb);
int  edp_mngt_lan_exists(char *lan, int *eth_type, int *endp_type);
int  edp_mngt_add_lan(int llid, int tid, char *name, char *lan, char *err);
int  edp_mngt_del_lan(int llid, int tid, char *name, char *lan, char *err);
int  edp_mngt_add(int llid, int tid, char *name, int endp_type);
int  edp_mngt_del(int llid, int tid, char *name);
void edp_mngt_del_all(void);
int  edp_mngt_get_qty(void);
int  edp_mngt_exists(char *name, int *endp_type);
void edp_mngt_set_eth_type(t_edp *cur, int eth_type);
t_edp *edp_mngt_get_first(int *nb_enp);
void edp_mngt_init(void);
/*--------------------------------------------------------------------------*/
