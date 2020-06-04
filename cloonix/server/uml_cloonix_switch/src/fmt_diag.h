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
int  fmt_tx_add_lan_pci_dpdk(int tid, char *lan, char *pci);
int  fmt_tx_del_lan_pci_dpdk(int tid, char *lan, char *pci);
int  fmt_tx_add_phy_vhost_port(int tid, char *name, char *lan);
int  fmt_tx_del_phy_vhost_port(int tid, char *name, char *lan);
int  fmt_tx_add_vhost_lan(int tid, char *name, int num, char *lan);
int  fmt_tx_add_vhost_port(int tid,char *name,int num,char *vhost,char *lan);
int  fmt_tx_del_vhost_lan(int tid, char *name, int num, char *lan);
int  fmt_tx_del_vhost_port(int tid,char *name,int num,char *vhost,char *lan);
int  fmt_tx_add_tap(int tid, char *name);
int  fmt_tx_del_tap(int tid, char *name);
int  fmt_tx_add_lan_tap(int tid, char *lan, char *name);
int  fmt_tx_del_lan_tap(int tid, char *lan, char *name);
int  fmt_tx_add_lan(int tid, char *lan);
int  fmt_tx_del_lan(int tid, char *lan);

int  fmt_tx_add_lan_snf(int tid, char *lan, char *name);
int  fmt_tx_del_lan_snf(int tid, char *lan, char *name);
int  fmt_tx_add_lan_nat(int tid, char *lan, char *name);
int  fmt_tx_del_lan_nat(int tid, char *lan, char *name);

int  fmt_tx_add_eth(int tid, char *name, int num, char *strmac);
int  fmt_tx_del_eth(int tid, char *name, int num);
int  fmt_tx_add_lan_eth(int tid, char *lan, char *name, int num);
int  fmt_tx_del_lan_eth(int tid, char *lan, char *name, int num);

void fmt_rx_rpct_recv_diag_msg(int llid, int tid, char *line);
void fmt_rx_rpct_recv_evt_msg(int llid, int tid, char *line);
/*--------------------------------------------------------------------------*/

