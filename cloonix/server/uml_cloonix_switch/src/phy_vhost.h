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
char *phy_vhost_get_ifname(char *name, int num);
void phy_vhost_ack_add_port(int tid, int is_ok, char *lan, char *name);
void phy_vhost_ack_del_port(int tid, int is_ok, char *lan, char *name);
int  phy_vhost_add_port(int llid, int tid, char *name, char *lan);
int  phy_vhost_del_port(char *name, char *lan);
void phy_vhost_init(void);
/*--------------------------------------------------------------------------*/
