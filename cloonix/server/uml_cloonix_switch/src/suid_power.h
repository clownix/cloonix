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
void suid_power_llid_closed(int llid);
int suid_power_rec_name(char *name, int on);
int suid_power_req_kill_all(void);
int suid_power_pid(void);
void suid_power_ifup_phy(char *phy);
void suid_power_ifdown_phy(char *phy);
typedef void (*t_qemu_vm_end)(char *name);
void suid_power_launch_vm(t_vm *vm, t_qemu_vm_end end);
void suid_power_kill_vm(int vm_id);
void suid_power_sigdiag_resp(int llid, int tid, char *line);
void suid_power_poldiag_resp(int llid, int tid, char *line);
int  suid_power_diag_llid(int llid);
void suid_power_pid_resp(int llid, int tid, char *name, int pid);
void suid_power_first_start(void);
int suid_power_get_pid(int vm_id);

int suid_power_get_topo_info_phy(t_topo_info_phy **phy);

int suid_power_get_topo_bridges(t_topo_bridges **bridges);

void suid_power_init(void);
/*--------------------------------------------------------------------------*/

