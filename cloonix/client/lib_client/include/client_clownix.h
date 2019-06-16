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

typedef void (*t_evt_stats_endp_cb)(int tid, char *name, int num,
                                   t_stats_counts *stats_counts, int status);
typedef void (*t_evt_stats_sysinfo_cb)(int tid, char *name,
                                       t_stats_sysinfo *stats, 
                                       char *df, int status);

typedef void (*t_evt_blkd_reports_cb)(int tid, t_blkd_reports *blkd);

typedef void (*t_qmp_cb)(int tid, char *name, char *line, int status);
typedef void (*t_mud_cli_dialog_cb)(int tid, char *name, int num, 
                                    char *line, int status);
typedef void (*t_list_commands_cb)(int tid, int qty, t_list_commands *list);
typedef void (*t_progress_txt_cb)(int status, int last, char *txt);
typedef void (*t_end_cb)(int tid, int status, char *err);
typedef void (*t_get_path_cb)(int tid, t_topo_clc *conf);
typedef void (*t_print_cb)(int tid, char *str);
typedef void (*t_topo_cb)(int tid, t_topo_info *topo);
typedef void (*t_sys_cb)(int tid, t_sys_info *sys);
typedef void (*t_topo_small_event_cb)(int tid, char *name, 
                                      char *p1, char *p2, int evt);
typedef void (*t_pid_cb)(int tid, int qty, t_pid_lst *pid);

typedef void (*t_eventfull_cb)(int nb_endp, t_eventfull_endp *endp);
typedef void (*t_hop_event_cb)(int tid, char *name, char *txt);
typedef void (*t_hop_name_list_cb)(int nb, t_hop_list *list);

typedef void (*t_slowperiodic_cb)(int nb, t_slowperiodic *spic);

/*---------------------------------------------------------------------------*/

void client_init(char *name, char *path, char *password);
int client_is_connected(void);
int topo_is_locked(void);

void client_get_path(int tid, t_get_path_cb cb);
void client_loop(void);

int get_clownix_main_llid(void);
void erase_clownix_main_llid(void);


void client_topo_tst_sub(t_topo_cb cb);

void client_promisc_set(int tid, t_end_cb cb, char *name, int eth, int promisc);


void client_add_vm(int tid, t_end_cb cb, char *nm, int nb_dpdk, int nb_eth,
                   int nb_wlan, int vm_config_flags, int cpu_qty, int mem_qty, 
                   char *kernel, char *root_fs, char *install_cdrom,
                   char *added_cdrom, char *added_disk, char *p9_host_share,
                   t_eth_params *eth_params);

void client_sav_vm(int tid, t_end_cb cb, char *nm, int type, char *new_dir_path);
void client_sav_vm_all(int tid, t_end_cb cb, int type, char *new_dir_path);
void client_reboot_vm(int tid, t_end_cb cb, char *nm);
void client_halt_vm(int tid, t_end_cb cb, char *nm);
void client_del_vm(int tid, t_end_cb cb, char *nm);
void client_add_sat(int tid, t_end_cb cb, char *name, 
                    int mutype, t_c2c_req_info *c2c_req_info);
void client_del_sat(int tid, t_end_cb cb, char *name);
void client_add_lan_endp(int tid, t_end_cb cb, char *name, int num, char *lan); 
void client_del_lan_endp(int tid, t_end_cb cb, char *name, int num, char *lan); 
void client_del_all(int tid, t_end_cb cb);
void client_kill_daemon(int tid, t_end_cb cb);

void client_topo_small_event_sub(int tid, t_topo_small_event_cb cb);
void client_topo_small_event_unsub(void);
void client_print_sub(int tid, t_print_cb cb);
void client_print_unsub(void);
void client_topo_sub(int tid, t_topo_cb cb);
void client_topo_unsub(void);
void client_sys_sub(int tid, t_sys_cb cb);
void client_sys_unsub(void);

void client_evt_stats_endp_sub(int tid, char *name, int num, int sub, 
                               t_evt_stats_endp_cb  cb);
void client_evt_stats_sysinfo_sub(int tid, char *name, int sub, 
                                  t_evt_stats_sysinfo_cb  cb);

void client_blkd_reports_sub(int tid, int sub, t_evt_blkd_reports_cb cb);

void client_save_vm(int tid, t_end_cb cb, char *nm, char *path);

int send_whole_topo(t_topo_info *topo, t_end_cb cb, t_print_cb cb_print);

char *to_ascii_sys(t_sys_info *sys);
char *to_ascii_topo_stats(t_topo_info *topo);



void client_list_commands(int tid,  t_list_commands_cb cb);
void client_sav_list_commands(int tid, t_list_commands_cb cb, char *dirpath);
void client_req_pids(int tid, t_pid_cb cb);

void client_req_eventfull(t_eventfull_cb cb);
void client_req_slowperiodic(t_slowperiodic_cb cb);

int cmd_ftopo_recv(char *topo_dir, t_progress_txt_cb pcb);

int cmd_topo_send(char *topo_dir, t_progress_txt_cb pcb);

int set_response_callback(t_end_cb cb, int tid);
void set_mud_cli_dialog_callback(t_mud_cli_dialog_cb cb);
void client_mud_cli_cmd(int tid, char *name, int num, char *line);


void set_qmp_callback(t_qmp_cb cb);
void client_qmp_sub(int tid, char *name);
void client_qmp_snd(int tid, char *name, char *msg);


int llid_client_doors_connect(char *path, char *passwd, 
                              t_doorways_end err_cb,
                              t_doorways_rx rx_cb);
#ifdef WITH_GLIB
void glib_client_init(void);
void glib_prepare_rx_tx(int llid);
void glib_clean_llid(int llid);
int glib_connect_llid(int llid, int fd, t_doorways_rx cb, char *passwd);
#endif


void client_set_hop_name_list(t_hop_name_list_cb cb);
void client_get_hop_name_list(int tid);
/*---------------------------------------------------------------------------*/
void client_set_hop_event(t_hop_event_cb cb);
void client_get_hop_event(int tid, int flags_hop, int nb, t_hop_list *list);
/*---------------------------------------------------------------------------*/


