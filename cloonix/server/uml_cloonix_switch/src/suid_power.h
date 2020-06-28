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
void suid_power_llid_closed(int llid);
int suid_power_rec_name(char *name, int on);
int suid_power_req_kill_all(void);
char *suid_power_get_drv(char *pci);
int suid_power_pid(void);
void suid_power_ifname_change(char *name, int num, char *old, char *nw);
void suid_power_ifup_phy(char *phy);
void suid_power_ifdown_phy(char *phy);
typedef void (*t_qemu_vm_end)(char *name);
void suid_power_launch_vm(t_vm *vm, t_qemu_vm_end end);
void suid_power_kill_vm(int vm_id);
void suid_power_diag_resp(int llid, int tid, char *line);
int  suid_power_diag_llid(int llid);
void suid_power_pid_resp(int llid, int tid, char *name, int pid);
void suid_power_first_start(void);
int suid_power_get_pid(int vm_id);

int suid_power_get_topo_pci(t_topo_pci **pci);
int suid_power_get_topo_phy(t_topo_phy **phy);
int suid_power_get_topo_bridges(t_topo_bridges **bridges);
int suid_power_get_topo_mirrors(t_topo_mirrors **mirrors);

int suid_power_req_vfio_attach(char *pci);
int suid_power_req_vfio_detach(char *pci);
t_topo_phy *suid_power_get_phy_info(char *name);
t_topo_pci *suid_power_get_pci_info(char *name);
void suid_power_init(void);
/*--------------------------------------------------------------------------*/

