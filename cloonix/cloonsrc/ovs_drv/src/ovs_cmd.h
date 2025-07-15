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
int ovs_cmd_system_promisc(char *ovs_bin, char *ovs_dir, char *vhost);
int ovs_cmd_vhost_up(char *ovs_bin, char *ovs_dir,
                     char *name, int num, char *vhost);
int ovs_cmd_add_snf_lan(char *ovs_bin, char *ovs_dir, char *name, int num,
                        char *vhost, char *lan);
int ovs_cmd_del_snf_lan(char *ovs_bin, char *ovs_dir, char *name, int num,
                        char *vhost, char *lan);
int ovs_cmd_add_lan_endp(char *ovs_bin, char *ovs_dir, char *lan, char *vhost);
int ovs_cmd_del_lan_endp(char *ovs_bin, char *ovs_dir, char *lan, char *vhost);

int ovs_cmd_add_lan(char *ovs_bin, char *ovs_dir, char *lan);
int ovs_cmd_add_lan_rstp(char *ovs_bin, char *ovs_dir, char *lan);
int ovs_cmd_del_lan(char *ovs_bin, char *ovs_dir, char *lan);

/*---------------------------------------------------------------------------*/

