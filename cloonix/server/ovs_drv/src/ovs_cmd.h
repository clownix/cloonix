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
int ovs_cmd_add_lan_ethv(char *ovs, char *dpdk, char *lan, char *vhost);
int ovs_cmd_del_lan_ethv(char *ovs, char *dpdk, char *lan, char *vhost);

int ovs_cmd_add_lan_ethd(char *ovs, char *dpdk, char *lan, char *name, int num);
int ovs_cmd_del_lan_ethd(char *ovs, char *dpdk, char *lan, char *name, int num);

int ovs_cmd_add_lan_eths(char *ovs, char *dpdk, char *lan, char *name, int num);
int ovs_cmd_del_lan_eths(char *ovs, char *dpdk, char *lan, char *name, int num);

int ovs_cmd_add_ethd(char *ovs, char *dpdk, char *name, int num);
int ovs_cmd_del_ethd(char *ovs, char *dpdk, char *name, int num);

int ovs_cmd_add_eths1(char *ovs, char *dpdk, char *name, int num);
int ovs_cmd_add_eths2(char *ovs, char *dpdk, char *name, int num);
int ovs_cmd_del_eths(char *ovs, char *dpdk, char *name, int num);

int ovs_cmd_add_lan_nat(char *ovs_bin, char *dpdk_dir, char *lan, char *name);
int ovs_cmd_del_lan_nat(char *ovs_bin, char *dpdk_dir, char *lan, char *name);
int ovs_cmd_add_lan_d2d(char *ovs_bin, char *dpdk_dir, char *lan, char *name);
int ovs_cmd_del_lan_d2d(char *ovs_bin, char *dpdk_dir, char *lan, char *name);
int ovs_cmd_add_phy(char *ovs_bin, char *dpdk_dir, char *name);
int ovs_cmd_del_phy(char *ovs_bin, char *dpdk_dir, char *name);
int ovs_cmd_add_lan_phy(char *ovs_bin, char *dpdk_dir, char *lan, char *name);
int ovs_cmd_del_lan_phy(char *ovs_bin, char *dpdk_dir, char *lan, char *name);
int ovs_cmd_add_tap(char *ovs_bin, char *dpdk_dir, char *name);
int ovs_cmd_del_tap(char *ovs_bin, char *dpdk_dir, char *name);
int ovs_cmd_add_lan_tap(char *ovs_bin, char *dpdk_dir, char *lan, char *name);
int ovs_cmd_del_lan_tap(char *ovs_bin, char *dpdk_dir, char *lan, char *name);
int ovs_cmd_add_lan_a2b(char *ovs_bin, char *dpdk_dir, char *lan, char *name, int num);
int ovs_cmd_del_lan_a2b(char *ovs_bin, char *dpdk_dir, char *lan, char *name, int num);
int ovs_cmd_add_lan(char *ovs_bin, char *dpdk_dir, char *lan);
int ovs_cmd_del_lan(char *ovs_bin, char *dpdk_dir, char *lan);
/*---------------------------------------------------------------------------*/

