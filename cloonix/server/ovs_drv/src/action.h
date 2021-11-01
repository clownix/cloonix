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
void action_add_ethd(char *respb, char *name, int num);
void action_del_ethd(char *respb, char *name, int num);
void action_add_eths1(char *respb, char *name, int num);
void action_add_eths2(char *respb, char *name, int num);
void action_del_eths(char *respb, char *name, int num);

void action_add_lan_ethd(char *respb, char *lan, char *name, int num);
void action_del_lan_ethd(char *respb, char *lan, char *name, int num);
void action_add_lan_ethv(char *respb, char *lan, char *name, int num, char *vhost);
void action_del_lan_ethv(char *respb, char *lan, char *name, int num, char *vhost);
void action_add_lan_eths(char *respb, char *lan, char *name, int num);
void action_del_lan_eths(char *respb, char *lan, char *name, int num);

void action_add_lan(char *respb, char *lan);
void action_del_lan(char *respb, char *lan);
void action_add_lan_nat(char *respb, char *lan, char *name);
void action_del_lan_nat(char *respb, char *lan, char *name);
void action_add_lan_d2d(char *respb, char *lan, char *name);
void action_del_lan_d2d(char *respb, char *lan, char *name);
void action_add_lan_a2b(char *respb, char *lan, char *name, int num);
void action_del_lan_a2b(char *respb, char *lan, char *name, int num);
void action_add_phy(char *respb, char *name);
void action_del_phy(char *respb, char *name);
void action_add_tap(char *respb, char *name);
void action_del_tap(char *respb, char *name);
void action_add_lan_tap(char *respb, char *lan, char *name);
void action_del_lan_tap(char *respb, char *lan, char *name);
void action_add_lan_phy(char *respb, char *lan, char *name);
void action_del_lan_phy(char *respb, char *lan, char *name);
void action_req_ovsdb(char *bin, char *db, char *respb, uint32_t lcore_mask,
                      uint32_t socket_mem, uint32_t cpu_mask);
void action_req_ovs_switch(char *bin, char *db, char *respb,
                           uint32_t lcore_mask, uint32_t socket_mem,
                           uint32_t cpu_mask);
void action_req_suidroot(char *respb);
void action_req_destroy(void);
/*---------------------------------------------------------------------------*/

