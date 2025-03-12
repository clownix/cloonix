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
#define MAX_A2D_LEN 50000

#define IDX_X11_DISPLAY_ADD 142
#define MAX_IDX_X11 20
#define MAX_RPC_MSG_LEN 50000

#define DBSSH_SERV_DOORS_REQ "DBSSH_SERV_DOORS_REQ in_idx_x11=%d cookie=%s"
#define DBSSH_SERV_DOORS_RESP "DBSSH_SERV_DOORS_RESP display_sock_x11=%d"
#define LABREAK "link_agent_break"
#define LABOOT  "link_agent_reboot %d"
#define LAHALT  "link_agent_halt %d"
#define LAPING  "link_agent_ping %d"
#define LAX11OPEN  "link_agent_x11_open sub_dido_idx=%d display_sock_x11=%d"
#define LAX11OPENOK  "link_agent_x11_open_ok sub_dido_idx=%d"
#define LAX11OPENKO  "link_agent_x11_open_ko sub_dido_idx=%d"
#define LASTATS "link_agent_stats_req"
#define LASTATSDF "link_agent_stats_df_req"


#define SYSINFOFORMAT "uptime:%lu load1:%lu load5:%lu load15:%lu "\
                      "totalram:%lu freeram:%lu cachedram:%lu "\
                      "sharedram:%lu bufferram:%lu "\
                      "totalswap:%lu freeswap:%lu procs:%lu "\
                      "totalhigh:%lu freehigh:%lu mem_unit:%lu"


enum{
  header_type_traffic = 999,
  header_type_x11_ctrl,
  header_type_x11,
  header_type_ctrl,
  header_type_ctrl_agent,
  header_type_stats,

};

enum{
  header_val_none = 777,
  header_val_add_dido_llid,
  header_val_del_dido_llid,
  header_val_ping,
  header_val_ack,
  header_val_reboot,
  header_val_halt,
  header_val_x11_open_serv,
  header_val_sysinfo,
  header_val_sysinfo_df,

};


/*---------------------------------------------------------------------------*/


