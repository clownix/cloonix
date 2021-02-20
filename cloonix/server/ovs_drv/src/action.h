/*****************************************************************************/
/*    Copyright (C) 2006-2021 clownix@clownix.net License AGPL-3             */
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
void action_add_lan_pci_dpdk(char *respb, char *lan, char *pci);
void action_del_lan_pci_dpdk(char *respb, char *lan, char *pci);
void action_add_lan_phy_port(char *respb, char *lan, char *name);
void action_del_lan_phy_port(char *respb, char *lan, char *name);
void action_add_eth_br(char *respb, char *name, int num, char *mac);
void action_del_eth_br(char *respb, char *name, int num);
void action_add_lan_br(char *respb, char *lan);
void action_del_lan_br(char *respb, char *lan);
void action_add_lan_snf_br(char *respb, char *lan, char *name);
void action_del_lan_snf_br(char *respb, char *lan, char *name);
void action_add_lan_nat_br(char *respb, char *lan, char *name);
void action_del_lan_nat_br(char *respb, char *lan, char *name);
void action_add_lan_d2d_br(char *respb, char *lan, char *name);
void action_del_lan_d2d_br(char *respb, char *lan, char *name);
void action_add_lan_a2b_br(char *respb, char *lan, char *name);
void action_del_lan_a2b_br(char *respb, char *lan, char *name);
void action_add_lan_b2a_br(char *respb, char *lan, char *name);
void action_del_lan_b2a_br(char *respb, char *lan, char *name);
void action_add_lan_eth_br(char *respb, char *lan, char *name, int num);
void action_del_lan_eth_br(char *respb, char *lan, char *name, int num);
void action_add_tap_br(char *respb, char *name);
void action_del_tap_br(char *respb, char *name);
void action_add_lan_tap_br(char *respb, char *lan, char *name);
void action_del_lan_tap_br(char *respb, char *lan, char *name);
int action_add_eth_req(char *line, char *name, int *num, char *mac);
void action_req_ovsdb(char *bin, char *db, char *respb, uint32_t lcore_mask,
                      uint32_t socket_mem, uint32_t cpu_mask);
void action_req_ovs_switch(char *bin, char *db, char *respb,
                           uint32_t lcore_mask, uint32_t socket_mem,
                           uint32_t cpu_mask);
void action_req_suidroot(char *respb);
void action_req_destroy(void);
/*---------------------------------------------------------------------------*/

