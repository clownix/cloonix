/*****************************************************************************/
/*    Copyright (C) 2006-2019 cloonix@cloonix.net License AGPL-3             */
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
#define MAX_ARG_LEN 500
#define MAX_ENV_LEN 100
#define NB_ENV 10
#define NB_ARG 50
void init_environ(char *ovs_bin, char *dpdk_dir);
int call_my_popen(char *dpdk_dir, int nb, char arg[NB_ARG][MAX_ARG_LEN]);
int ovs_execv_add_lan(t_all_ctx *all_ctx, char *ovs_bin,
                      char *dpdk_dir, char *lan_name);

int ovs_execv_del_lan(t_all_ctx *all_ctx, char *ovs_bin,
                      char *dpdk_dir, char *lan_name);

int ovs_execv_add_lan_eth(t_all_ctx *all_ctx, char *ovs_bin,
                          char *dpdk_dir, char *lan_name,
                          char *vm_name, int num);

int ovs_execv_del_lan_eth(t_all_ctx *all_ctx, char *ovs_bin,
                          char *dpdk_dir, char *lan_name,
                          char *vm_name, int num);

int ovs_execv_add_eth(t_all_ctx *all_ctx, char *ovs_bin, char *dpdk_dir,
                                          char *name, int num);

int ovs_execv_add_spy(t_all_ctx *all_ctx, char *ovs_bin, char *dpdk_dir,
                                          char *name, int num, int spy);

int ovs_execv_add_spy_eth(t_all_ctx *all_ctx, char *ovs_bin, char *dpdk_dir,
                          char *name, int num);

int ovs_execv_del_eth(t_all_ctx *all_ctx, char *ovs_bin, char *dpdk_dir,
                                          char *name, int num);

int ovs_execv_daemon(t_all_ctx *all_ctx, int is_switch,
                     char *ovs_bin, char *dpdk_dir);

