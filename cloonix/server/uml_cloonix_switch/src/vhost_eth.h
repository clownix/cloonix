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
void vhost_eth_tap_rename(char *name, int num, char *new_eth_name);
int vhost_lan_info(char *lan, char *vhost_ifname, char *name, int *num);
int vhost_lan_exists(char *lan);
int vhost_eth_exists(char *name, int num);
int vhost_eth_add_lan(int llid, int tid, char *lan,
                      int vm_id, char *name, int num, char *info);
int vhost_eth_del_lan(int llid, int tid, char *lan,
                      char *name, int num, char *info);
void vhost_eth_add_ack_lan(int tid, int is_ok, char *lan, char *name, int num);
void vhost_eth_add_ack_port(int tid, int is_ok, char *lan, char *name, int num);
void vhost_eth_del_ack_lan(int tid, int is_ok, char *lan, char *name, int num);
void vhost_eth_del_ack_port(int tid, int is_ok, char *lan, char *name, int num);
void vhost_eth_add_vm(char *name, int nb_tot_eth, t_eth_table *eth_tab);
void vhost_eth_del_vm(char *name, int nb_tot_eth, t_eth_table *eth_tab);
t_topo_endp *vhost_eth_translate_topo_endp(int *nb);
void vhost_eth_init(void);
/*---------------------------------------------------------------------------*/

