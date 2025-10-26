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
void vwifi_trafic_fd_isset(fd_set *infd);
int vwifi_trafic_fd_set(fd_set *infd, int max_fd);
void vwifi_init(int offset);
void vwifi_heartbeat(int cur_sec);
int vwifi_listen_locked(int fd);

void vwifi_virtio_syn_ok_cli(int vwifi_cid);
void vwifi_virtio_syn_ko_cli(int vwifi_cid);

void vwifi_virtio_syn_ok_spy(int vwifi_cid);
void vwifi_virtio_syn_ko_spy(int vwifi_cid);

void vwifi_virtio_syn_ok_ctr(int vwifi_cid);
void vwifi_virtio_syn_ko_ctr(int vwifi_cid);

void vwifi_client_syn_cli (int vwifi_base, int fd);
void vwifi_client_syn_spy (int vwifi_base, int fd);
void vwifi_client_syn_ctr (int vwifi_base, int fd);

void vwifi_virtio_traf_cli(int vwifi_cid, char *rx, int len);
void vwifi_virtio_traf_spy(int vwifi_cid, char *rx, int len);
void vwifi_virtio_traf_ctr(int vwifi_cid, char *rx, int len);

/*---------------------------------------------------------------------------*/

