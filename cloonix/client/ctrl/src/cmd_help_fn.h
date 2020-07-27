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
void init_connection_to_uml_cloonix_switch(void);
int cmd_topo_set(int argc, char **argv);
int cmd_topo_get(int argc, char **argv);
int cmd_ftopo_get(int argc, char **argv);
int cmd_event_print(int argc, char **argv);
int cmd_event_sys(int argc, char **argv);
int cmd_event_blkd(int argc, char **argv);
int cmd_snf_on(int argc, char **argv);
int cmd_snf_off(int argc, char **argv);
int cmd_snf_get_file(int argc, char **argv);
int cmd_snf_set_file(int argc, char **argv);
/*---------------------------------------------------------------------------*/
int cmd_dpdk_ovs_cnf(int argc, char **argv);
int cmd_kill(int argc, char **argv);
int cmd_delall(int argc, char **argv);
int cmd_name_dump(int argc, char **argv);
int cmd_config_dump(int argc, char **argv);
int cmd_topo_dump(int argc, char **argv);
int cmd_pid_dump(int argc, char **argv);
int cmd_add_vm_kvm(int argc, char **argv);
int cmd_qreboot_vm(int argc, char **argv);
int cmd_qhalt_vm(int argc, char **argv);
int cmd_reboot_vm(int argc, char **argv);
int cmd_halt_vm(int argc, char **argv);
int cmd_del_vm(int argc, char **argv);
int cmd_add_tap(int argc, char **argv);
int cmd_add_phy(int argc, char **argv);
int cmd_add_pci(int argc, char **argv);
int cmd_add_wif(int argc, char **argv);
int cmd_add_snf(int argc, char **argv);
int cmd_add_a2b(int argc, char **argv);
int cmd_add_nat(int argc, char **argv);
int cmd_del_sat(int argc, char **argv);
int cmd_add_c2c(int argc, char **argv);
int cmd_add_d2d(int argc, char **argv);
int cmd_add_vl2sat(int argc, char **argv);
int cmd_del_vl2sat(int argc, char **argv);
/*---------------------------------------------------------------------------*/
int cmd_sav_derived(int argc, char **argv);
int cmd_sav_full(int argc, char **argv);
/*---------------------------------------------------------------------------*/
int cmd_sub_qmp(int argc, char **argv);
int cmd_snd_qmp(int argc, char **argv);
/*---------------------------------------------------------------------------*/
void help_dpdk_ovs_cnf(char *line);
void help_sub_qmp(char *line);
void help_snd_qmp(char *line);
/*---------------------------------------------------------------------------*/
void help_sav_derived(char *line);
void help_sav_full(char *line);
/*---------------------------------------------------------------------------*/
void help_topo_set(char *line);
void help_topo_get(char *line);
void help_ftopo_get(char *line);
void help_add_vm_kvm(char *line);
void help_qreboot_vm(char *line);
void help_halt_vm(char *line);
void help_reboot_vm(char *line);
void help_qhalt_vm(char *line);
void help_del_vm(char *line);
void help_add_sat(char *line);
void help_add_wif(char *line);
void help_add_c2c(char *line);
void help_add_d2d(char *line);
void help_del_sat(char *line);
void help_add_vl2sat(char *line);
void help_del_vl2sat(char *line);
void help_snf_on(char *line);
void help_snf_off(char *line);
void help_snf_get_file(char *line);
void help_snf_set_file(char *line);
/*---------------------------------------------------------------------------*/
int cmd_event_hop(int argc, char **argv);
void help_event_hop(char *line);
void help_sav(char *line);
/*---------------------------------------------------------------------------*/
int cmd_graph_width_height(int argc, char **argv);
void help_graph_width_height(char *line);
int cmd_graph_zoom(int argc, char **argv);
void help_graph_zoom(char *line);
int cmd_graph_save_params_req(int argc, char **argv);
void help_graph_center_scale(char *line);
int cmd_graph_center_scale(int argc, char **argv);
/*---------------------------------------------------------------------------*/
int cmd_list_commands(int argc, char **argv);
int cmd_lay_commands(int argc, char **argv);
int cmd_sav_list_commands(int argc, char **argv);


int cmd_mud_lan(int argc, char **argv);
void help_mud_lan(char *line);

int cmd_mud_sat(int argc, char **argv);
void help_mud_sat(char *line);

int cmd_mud_eth(int argc, char **argv);
void help_mud_eth(char *line);
/*---------------------------------------------------------------------------*/
int cmd_sub_endp(int argc, char **argv);
void help_sub_endp(char *line);
int cmd_sub_sysinfo(int argc, char **argv);
void help_sub_sysinfo(char *line);
/*---------------------------------------------------------------------------*/
int cmd_cnf_a2b(int argc, char **argv);
void help_cnf_a2b(char *line);
/*---------------------------------------------------------------------------*/
int cmd_hwsim_config(int argc, char **argv);
void help_hwsim_config(char *line);
int cmd_hwsim_dump(int argc, char **argv);
void help_hwsim_dump(char *line);
/*---------------------------------------------------------------------------*/





