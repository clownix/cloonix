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
void dpdk_phy_resp_add_lan(int is_ko, char *lan, char *name);
void dpdk_phy_resp_del_lan(int is_ko, char *lan, char *name);
int dpdk_phy_add_lan(int llid, int tid, char *name, char *lan);
int dpdk_phy_del_lan(int llid, int tid, char *name,  char *lan);
int dpdk_phy_add(int llid, int tid, char *name);
int dpdk_phy_del(int llid, int tid, char *name);
void dpdk_phy_resp_add(int is_ko, char *name);
void dpdk_phy_resp_del(int is_ko, char *name);
void dpdk_phy_init(void);
/*--------------------------------------------------------------------------*/

