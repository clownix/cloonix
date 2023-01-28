/*****************************************************************************/
/*    Copyright (C) 2006-2022 clownix@clownix.net License AGPL-3             */
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
int fmt_tx_add_lan(int tid, char *lan);
int fmt_tx_del_lan(int tid, char *lan);
int fmt_tx_add_tap(int tid, char *name, char *vhost, char *mac);
int fmt_tx_del_tap(int tid, char *name, char *vhost);
int fmt_tx_add_phy(int tid, char *name, char *vhost, char *mac);
int fmt_tx_del_phy(int tid, char *name, char *vhost);
int fmt_tx_vhost_up(int tid, char *name, int num, char *vhost);
int fmt_tx_add_snf_lan(int tid, char *name, int num, char *vhost, char *lan);
int fmt_tx_del_snf_lan(int tid, char *name, int num, char *vhost, char *lan);
int fmt_tx_add_lan_endp(int tid, char *name, int num, char *vhost, char *lan);
int fmt_tx_del_lan_endp(int tid, char *name, int num, char *vhost, char *lan);

void fmt_rx_rpct_recv_sigdiag_msg(int llid, int tid, char *line);
/*--------------------------------------------------------------------------*/

