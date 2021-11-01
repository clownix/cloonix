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
int  fmt_tx_add_phy(int tid, char *name);
int  fmt_tx_del_phy(int tid, char *name);
int  fmt_tx_add_tap(int tid, char *name);
int  fmt_tx_del_tap(int tid, char *name);

int  fmt_tx_add_ethd(int tid, char *name, int num);
int  fmt_tx_del_ethd(int tid, char *name, int num);

int  fmt_tx_add_eths1(int tid, char *name, int num);
int  fmt_tx_add_eths2(int tid, char *name, int num);
int  fmt_tx_del_eths(int tid, char *name, int num);

int  fmt_tx_add_lan(int tid, char *lan);
int  fmt_tx_del_lan(int tid, char *lan);

int  fmt_tx_add_lan_tap(int tid, char *lan, char *name);
int  fmt_tx_del_lan_tap(int tid, char *lan, char *name);

int  fmt_tx_add_lan_phy(int tid, char *lan, char *name);
int  fmt_tx_del_lan_phy(int tid, char *lan, char *name);

int  fmt_tx_add_lan_nat(int tid, char *lan, char *name);
int  fmt_tx_del_lan_nat(int tid, char *lan, char *name);

int  fmt_tx_add_lan_ethd(int tid, char *lan, char *name, int num);
int  fmt_tx_del_lan_ethd(int tid, char *lan, char *name, int num);

int  fmt_tx_add_lan_ethv(int tid, char *lan, char *name, int num, char *vhost);
int  fmt_tx_del_lan_ethv(int tid, char *lan, char *name, int num, char *vhost);

int  fmt_tx_add_lan_eths(int tid, char *lan, char *name, int num);
int  fmt_tx_del_lan_eths(int tid, char *lan, char *name, int num);

int  fmt_tx_add_lan_a2b(int tid, char *lan, char *name, int num);
int  fmt_tx_del_lan_a2b(int tid, char *lan, char *name, int num);

int  fmt_tx_add_lan_d2d(int tid, char *lan, char *name);
int  fmt_tx_del_lan_d2d(int tid, char *lan, char *name);

void fmt_rx_rpct_recv_sigdiag_msg(int llid, int tid, char *line);
/*--------------------------------------------------------------------------*/

