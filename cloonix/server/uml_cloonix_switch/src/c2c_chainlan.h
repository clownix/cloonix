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
int c2c_chainlan_store_cmd(char *name, int cmd_mac_mangle, uint8_t *mac);
void c2c_chainlan_update_endp_type(char *name, int endp_type);
void c2c_chainlan_snf_started(char *name, int num, char *vhost);
void c2c_chainlan_resp_add_lan(int is_ko, char *name, int num,
                               char *vhost, char *lan);
void c2c_chainlan_resp_del_lan(int is_ko, char *name, int num,
                               char *vhost, char *lan);
int c2c_chainlan_action_heartbeat(t_ovs_c2c *c2c, char *attlan);
int c2c_chainlan_exists_lan(char *name);
int c2c_chainlan_add_lan(t_ovs_c2c *cur, char *lan);
int c2c_chainlan_del_lan(t_ovs_c2c *cur, char *lan);
int c2c_chainlan_add(t_ovs_c2c *cur);
int c2c_chainlan_del(t_ovs_c2c *cur);
int c2c_chainlan_partial_del(t_ovs_c2c *cur);
void c2c_chainlan_init(void);
/*--------------------------------------------------------------------------*/

