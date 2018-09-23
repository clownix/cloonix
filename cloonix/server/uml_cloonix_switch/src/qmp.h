/*****************************************************************************/
/*    Copyright (C) 2006-2018 cloonix@cloonix.net License AGPL-3             */
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
void qmp_begin_qemu_unix(char *name);
void qmp_agent_sysinfo(char *name, int used_mem_agent);
void qmp_msg_recv(char *name, char *msg);
void qmp_msg_send(char *name, char *msg);
void qmp_event_free(int llid);
void qmp_init(void);
void qmp_clean_all_data(void);
void qmp_request_qemu_reboot(char *name, int llid, int tid);
void qmp_request_qemu_halt(char *name, int llid, int tid);
void qmp_request_save_rootfs(char *name, char *path, int llid,
                             int tid, int stype);
void qmp_request_save_rootfs_all(int nb, t_vm *vm, char *path, int llid,
                                 int tid, int stype);
void qmp_request_sub(char *name, int llid, int tid);
void qmp_request_snd(char *name, int llid, int tid, char *msg);

/*--------------------------------------------------------------------------*/
