/*****************************************************************************/
/*    Copyright (C) 2006-2022 clownix@clownix.net License AGPL-3             */
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
#define MAX_ARG_LEN 1300
#define MAX_ENV_LEN 100
#define NB_ENV 10
#define NB_ARG 50
int ovs_vsctl(char *ovs_bin, char *dpdk_dir, char *multi_arg_cmd);
int dpdk_is_usable(void);
int create_ovsdb_server_conf(char *ovs_bin, char *dpdk_dir);

void init_environ(char *net, char *ovs_bin, char *dpdk_dir);
int call_my_popen(char *dpdk_dir, int nb, char arg[NB_ARG][MAX_ARG_LEN]);

int ovs_execv_add_lan_br(char *ovs_bin, char *dpdk_dir, char *lan_name);
int ovs_execv_del_lan_br(char *ovs_bin, char *dpdk_dir, char *lan_name);

int ovs_execv_add_lan_nat(char *ovs, char *dpdk, char *lan, char *name);
int ovs_execv_del_lan_nat(char *ovs, char *dpdk, char *lan, char *name);
int ovs_execv_add_lan_d2d(char *ovs, char *dpdk, char *lan, char *name);
int ovs_execv_del_lan_d2d(char *ovs, char *dpdk, char *lan, char *name);

int ovs_execv_add_lan_a2b(char *ovs, char *dpdk, char *lan, char *name);
int ovs_execv_del_lan_a2b(char *ovs, char *dpdk, char *lan, char *name);
int ovs_execv_add_lan_b2a(char *ovs, char *dpdk, char *lan, char *name);
int ovs_execv_del_lan_b2a(char *ovs, char *dpdk, char *lan, char *name);

int ovs_execv_add_lan_eth(char *ovs_bin, char *dpdk_dir, char *lan_name,
                          char *vm_name, int num);
int ovs_execv_del_lan_eth(char *ovs_bin, char *dpdk_dir, char *lan_name,
                          char *vm_name, int num);
int ovs_execv_add_eth(char *ovs_bin, char *dpdk_dir, char *name, int num);
int ovs_execv_del_eth(char *ovs_bin, char *dpdk_dir, char *name, int num);


int ovs_execv_add_tap(char *ovs_bin, char *dpdk_dir, char *name);
int ovs_execv_del_tap(char *ovs_bin, char *dpdk_dir, char *name);


int ovs_execv_add_lan_tap(char *ovs_bin, char *dpdk_dir,
                          char *lan, char *name);
int ovs_execv_del_lan_tap(char *ovs_bin, char *dpdk_dir,
                          char *lan, char *name);


int ovs_execv_add_phy(char *ovs_bin, char *dpdk_dir, char *name);
int ovs_execv_del_phy(char *ovs_bin, char *dpdk_dir, char *name);


int ovs_execv_add_lan_phy(char *ovs_bin, char *dpdk_dir,
                          char *lan, char *name);
int ovs_execv_del_lan_phy(char *ovs_bin, char *dpdk_dir,
                          char *lan, char *name);



int ovs_execv_ovs_vswitchd(char *net, char *ovs_bin, char *dpdk_dir,
                           uint32_t lcore_mask, uint32_t socket_mem,
                           uint32_t cpu_mask);
int ovs_execv_ovsdb_server(char *net, char *ovs_bin, char *dpdk_dir,
                           uint32_t lcore_mask, uint32_t socket_mem,
                           uint32_t cpu_mask);

char *get_pidfile_ovsdb(char *dpdk_dir);
char *get_pidfile_ovs(char *dpdk_dir);

void ovs_execv_init(void);
