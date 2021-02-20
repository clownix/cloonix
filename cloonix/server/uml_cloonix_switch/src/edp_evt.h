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
void edp_evt_end_eth_type_dpdk(char *lan, int status);
int edp_evt_action_add_lan(t_edp *cur, int llid, int tid);
void edp_evt_action_del_lan(t_edp *cur, int llid, int tid);
void edp_evt_change_phy_topo(void);
void edp_evt_change_pci_topo(void);
void edp_evt_update_fix_type_add(int eth_type, char *name, char *lan);
void edp_evt_update_fix_type_del(int eth_type, char *name, char *lan);
void edp_evt_update_non_fix_add(int eth_type, char *name, char *lan);
void edp_evt_update_non_fix_del(int eth_type, char *name, char *lan);
void edp_evt_init(void);
/*--------------------------------------------------------------------------*/
