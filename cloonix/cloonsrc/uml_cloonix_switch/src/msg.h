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
enum {
     ovsreq_add_cnt_lan = 15,
     ovsreq_del_cnt_lan,
     ovsreq_add_kvm_lan,
     ovsreq_del_kvm_lan,
     ovsreq_add_tap_lan,
     ovsreq_del_tap_lan,
     ovsreq_add_phy_lan,
     ovsreq_del_phy_lan,
     ovsreq_add_nat_lan,
     ovsreq_del_nat_lan,
     ovsreq_add_c2c_lan,
     ovsreq_del_c2c_lan,
     ovsreq_add_a2b_lan,
     ovsreq_del_a2b_lan,
     ovsreq_vhost_up,
     ovsreq_add_tap,
     ovsreq_del_tap,
     ovsreq_add_phy,
     ovsreq_del_phy,
     ovsreq_add_snf_lan,
     ovsreq_del_snf_lan,
     };

void msg_vlan_exist_no_more(char *lan);
void msg_ack_tap(int tid, int ko, int add, char *name, char *vhost, char *mc);
void msg_ack_phy(int tid, int ko, int add, char *name, int num, char *vhost);
void msg_ack_lan(int tid, int ko, int add, char *lan);
void msg_ack_vhost_up(int tid, int ko, char *name, int num, char *vhost);
void msg_ack_snf_ethv(int tid, int ko, int add, char *name, int num,
                      char *vhost, char *lan);

void msg_ack_lan_endp(int tid, int ko, int add, char *name, int num,
                      char *vhost, char *lan);

/*--------------------------------------------------------------------------*/
int msg_send_add_tap(char *name, char *vhost, char *mac);
int msg_send_del_tap(char *name, char *vhost);
int msg_send_add_phy(char *name, char *vhost, char *mac, int num_macvlan);
int msg_send_del_phy(char *name, char *vhost, int type);
int msg_send_vhost_up(char *name, int num, char *vhost);

int msg_send_add_snf_lan(char *name, int num, char *vhost, char *lan);
int msg_send_del_snf_lan(char *name, int num, char *vhost, char *lan);

int msg_send_add_lan_endp(int ovsreq,char *name,int num,char *vhost,char *lan);

int msg_send_del_lan_endp(int ovsreq, char *name, int num,
                          char *vhost, char *lan);


void msg_lan_add_name(char *name);
void msg_lan_add_rstp(char *name);
void msg_lan_del_name(char *name);

int msg_ovsreq_get_qty(void);

int msg_ovsreq_exists_vhost_lan(char *vhost, char *lan);
int msg_ovsreq_exists_name(char *name);
/*--------------------------------------------------------------------------*/
void msg_init(void);
/*--------------------------------------------------------------------------*/

