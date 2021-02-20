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
int net_phy_vfio_detach(char *pci, char *old_drv);
int net_phy_vfio_attach(char *pci, char **old_drv);
int net_phy_ifname_change(char *old_name, char *new_name);
int net_phy_flags_iff_up_down(char *intf, int up);
t_topo_phy *net_phy_get(int *nb);
t_topo_pci *net_pci_get(int *nb);
void net_phy_init(void);
/*--------------------------------------------------------------------------*/
