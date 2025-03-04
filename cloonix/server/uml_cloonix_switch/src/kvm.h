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
typedef struct t_ethv_cnx
{
  char name[MAX_NAME_LEN];
  int num;
  char vhost[MAX_NAME_LEN];
  int endp_type;
  int ready;
  char lan[MAX_NAME_LEN];
  char lan_added[MAX_NAME_LEN];
  int waiting_ack_add_lan;
  int waiting_ack_del_lan;
  int attached_lan_ok;
  int llid;
  int tid;
  int del_snf_ethv_sent;
  struct t_ethv_cnx *prev;
  struct t_ethv_cnx *next;
} t_ethv_cnx;
/*--------------------------------------------------------------------------*/
t_topo_endp *kvm_translate_topo_endp(int *nb);
int kvm_exists(char *name, int num);
void kvm_add_whole_vm(char *name, int nb_tot_eth, t_eth_table *eth_tab);
void kvm_resp_add_lan(int is_ko, char *name, int num, char *vhost, char *lan);
void kvm_resp_del_lan(int is_ko, char *name, int num, char *vhost, char *lan);
int kvm_add_lan(int llid, int tid, char *name, int num,
                     char *lan, int endp_type, t_eth_table *eth_tab);
int kvm_del_lan(int llid, int tid, char *name, int num,
                     char *lan, int endp_type);
void kvm_del(int llid, int tid, char *name, int num,
             int endp_type, int *can_delete);
int kvm_lan_added_exists(char *name, int num);
int kvm_dyn_snf(char *name, int num, int val);
t_ethv_cnx *get_first_head_ethv(void);
void kvm_init(void);
/*--------------------------------------------------------------------------*/

