/*****************************************************************************/
/*    Copyright (C) 2006-2023 clownix@clownix.net License AGPL-3             */
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
int ovs_vsctl(char *ovs_bin, char *ovs_dir, char *multi_arg_cmd);
int ovs_vsctl_quiet(char *ovs_bin, char *ovs_dir, char *multi_arg_cmd);
int create_ovsdb_server_conf(char *ovs_bin, char *ovs_dir);

void init_environ(char *net, char *ovs_bin, char *ovs_dir);
int call_ovs_popen(char *ovs_dir, char **argv, int quiet,
                   const char *fct, int line);

int ovs_execv_add_lan_br(char *ovs_bin, char *ovs_dir, char *lan_name);
int ovs_execv_del_lan_br(char *ovs_bin, char *ovs_dir, char *lan_name);

int ovs_execv_add_lan_c2c(char *ovs_bin, char *ovs_dir, char *lan, char *name);
int ovs_execv_del_lan_c2c(char *ovs_bin, char *ovs_dir, char *lan, char *name);

int ovs_execv_add_lan_eth(char *ovs_bin, char *ovs_dir, char *lan_name,
                          char *vm_name, int num);
int ovs_execv_del_lan_eth(char *ovs_bin, char *ovs_dir, char *lan_name,
                          char *vm_name, int num);
int ovs_execv_add_eth(char *ovs_bin, char *ovs_dir, char *name, int num);
int ovs_execv_del_eth(char *ovs_bin, char *ovs_dir, char *name, int num);


int ovs_execv_add_tap(char *ovs_bin, char *ovs_dir, char *name);
int ovs_execv_del_tap(char *ovs_bin, char *ovs_dir, char *name);


int ovs_execv_add_lan_tap(char *ovs_bin, char *ovs_dir,
                          char *lan, char *name);
int ovs_execv_del_lan_tap(char *ovs_bin, char *ovs_dir,
                          char *lan, char *name);


int ovs_execv_add_phy(char *ovs_bin, char *ovs_dir, char *name);
int ovs_execv_del_phy(char *ovs_bin, char *ovs_dir, char *name);


int ovs_execv_add_lan_phy(char *ovs_bin, char *ovs_dir,
                          char *lan, char *name);
int ovs_execv_del_lan_phy(char *ovs_bin, char *ovs_dir,
                          char *lan, char *name);



int ovs_execv_ovs_vswitchd(char *net, char *ovs_bin, char *ovs_dir);
int ovs_execv_ovsdb_server(char *net, char *ovs_bin, char *ovs_dir);

char *get_pidfile_ovsdb(char *ovs_dir);
char *get_pidfile_ovs(char *ovs_dir);

void ovs_execv_init(void);
