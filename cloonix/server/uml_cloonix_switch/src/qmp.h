/*****************************************************************************/
/*    Copyright (C) 2006-2024 clownix@clownix.net License AGPL-3             */
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
void qmp_request_qemu_halt(char *name);
void qmp_begin_qemu_unix(char *name, int first);
void qmp_conn_end(char *name);
void qmp_msg_recv(char *name, char *msg);
void qmp_event_free(int llid);
void qmp_init(void);
void qmp_clean_all_data(void);
void qmp_request_qemu_reboot(char *name);
void qmp_request_save_rootfs(char *name, char *path, int llid, int tid);

/*--------------------------------------------------------------------------*/
