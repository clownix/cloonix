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
char *utils_get_cnt_dir(void);
char *utils_get_mnt_dir(void);

char *utils_get_c2c_dir(void);
char *utils_get_nat_main_dir(void);
char *utils_get_nat_proxy_dir(void);
char *utils_get_a2b_dir(void);
char *utils_get_run_dir(void);
char *utils_get_run_config_dir(void);
char *utils_get_snf_pcap_dir(void);

void utils_send_status_ko(int *llid, int *tid, char *err);
void utils_send_status_ok(int *llid, int *tid);
int utils_get_next_tid(void);
int utils_get_eth_numbers(int nb_tot_eth, t_eth_table *eth_tab, int *nb_eth);
char *utils_get_suid_power_bin_path(void);
char *utils_dir_conf(int vm_id);
char *utils_dir_conf_tmp(int vm_id);
char *utils_get_root_fs(char *rootfs);
void utils_chk_my_dirs(t_vm *vm);
void utils_finish_vm_init(void *ul_vm_id);
char *utils_get_cow_path_name(int vm_id);
void utils_send_creation_info(char *name, char **argv);
int utils_execv(void *ptr);
char *utils_get_uname_r_mod_path(void);
void utils_init(void);
int utils_get_uid_user(void);
int utils_get_gid_user(void);
char *utils_get_cdrom_path_name(t_vm *vm);

char *utils_get_disks_path_name(int vm_id);
char *utils_get_qmp_path(int vm_id);
char *utils_get_qga_path(int vm_id);
char *utils_get_qbackdoor_path(int vm_id);

char *utils_get_dtach_bin_path(void);
char *utils_get_dtach_sock_dir(void);
char *utils_get_dtach_sock_path(char *name);
char *utils_get_spice_path(int vm_id);
/*--------------------------------------------------------------------------*/
char *utils_get_cloonix_switch_path(void);
/*--------------------------------------------------------------------------*/
void free_wake_up_eths(t_vm *vm);
void free_wake_up_eths_and_delete_vm(t_vm *vm, int error_death);
/*--------------------------------------------------------------------------*/
void utils_format_gene(char *start, char *err, char *name, char **argv);
/*--------------------------------------------------------------------------*/
char *utils_get_proxymous_bin(void);
char *utils_get_ovs_path(char *name);
char *utils_get_ovs_dir(void);
char *utils_get_ovs_bin_dir(void);
char *utils_get_ovs_drv_bin_dir(void);
char *utils_get_ovs_snf_bin_dir(void);
char *utils_get_ovs_nat_main_bin_dir(void);
char *utils_get_ovs_a2b_bin_dir(void);
char *utils_get_ovs_c2c_bin_dir(void);
int util_get_max_tempo_fail(void);
char *utils_get_crun_dir(void);
char *utils_get_nginx_dir(void);
char *utils_get_nginx_conf_dir(void);
char *utils_get_nginx_logs_dir(void);
char *utils_get_nginx_client_body_temp_dir(void);
int lio_system(char *argv[]);
char *utils_get_log_dir(void);
/****************************************************************************/
