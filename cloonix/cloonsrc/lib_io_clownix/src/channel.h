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
#define IOCMLC 1
enum {
     kind_client = 0,
     kind_simple_watch_connect,
     kind_simple_watch,
     kind_server,
     kind_server_doors,
     kind_server_proxy_traf_inet,
     kind_server_proxy_traf_unix,
     kind_server_proxy_sig,
     kind_glib_managed
     };

/*---------------------------------------------------------------------------*/

int channel_get_kind(int cidx);
/*---------------------------------------------------------------------------*/
int channel_create(int fd, int kind, char *little_name,
                   t_fd_event rx_cb,  t_fd_event tx_cb, t_fd_error err_cb);
char *channel_get_little_name(int llid);
/*---------------------------------------------------------------------------*/
void channel_delete(int llid);
/*---------------------------------------------------------------------------*/
void channel_beat_set (t_heartbeat_cb beat);
/*---------------------------------------------------------------------------*/
void channel_loop(int once);
/*---------------------------------------------------------------------------*/
int  get_current_max_channels(void);
/*---------------------------------------------------------------------------*/
void channel_init(void);
/*---------------------------------------------------------------------------*/
int channel_check_llid(int llid, const char *fct);
/*---------------------------------------------------------------------------*/
int get_fd_with_cidx(int cidx);
/*---------------------------------------------------------------------------*/
int channel_get_epfd(void);
void channel_heartbeat_ms_set (int heartbeat_ms);
/*---------------------------------------------------------------------------*/
void called_from_channel_free_llid(int llid);
/*---------------------------------------------------------------------------*/










