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
#define PING_KO "ping_agent_ko"
#define PING_OK "ping_agent_ok"
#define BACKDOOR_CONNECTED "backdoor_connected"
#define BACKDOOR_DISCONNECTED "backdoor_disconnected"
#define HALT_REQUEST "halt_requested"
#define REBOOT_REQUEST "reboot_requested"
#define AGENT_SYSINFO "agent_sysinfo_sys"
#define AGENT_SYSINFO_DF "agent_sysinfo_df"
#define CLOONIX_UP_VPORT_AND_RUNNING "cloonix_up_vport_and_running"
#define CLOONIX_DOWN_AND_NOT_RUNNING "cloonix_down_and_not_running"
#define STOP_DOORS_LISTENING "stop_doors_listening"
#define XWY_CONNECT "xwy_connect %s"
/*---------------------------------------------------------------------------*/
void doors_send_info(int llid, int tid, char *type, char *info);
void doors_recv_info(int llid, int tid, char *type, char *info);
/*---------------------------------------------------------------------------*/
void doors_send_event(int llid, int tid, char *name, char *line);
void doors_recv_event(int llid, int tid, char *name, char *line);
/*---------------------------------------------------------------------------*/
void doors_send_command(int llid, int tid, char *name, char *cmd);
void doors_recv_command(int llid, int tid, char *name, char *cmd);
/*---------------------------------------------------------------------------*/
void doors_send_status(int llid, int tid, char *name, char *status);
void doors_recv_status(int llid, int tid, char *name, char *status);
/*---------------------------------------------------------------------------*/
void doors_send_add_vm(int llid, int tid, char *name, char *way2agent);
void doors_recv_add_vm(int llid, int tid, char *name, char *way2agent);
/*---------------------------------------------------------------------------*/
void doors_send_del_vm(int llid, int tid, char *name);
void doors_recv_del_vm(int llid, int tid, char *name);
/*---------------------------------------------------------------------------*/
void doors_xml_init(void);
int doors_xml_decoder (int llid, int len, char *str_rx);
/*---------------------------------------------------------------------------*/
