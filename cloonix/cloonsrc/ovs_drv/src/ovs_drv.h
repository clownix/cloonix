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
void clean_all_upon_error(void);
char *get_ovs_bin(void);
char *get_ovs_dir(void);
int get_ovsdb_pid(void);
void set_ovsdb_pid(int pid);
int get_ovs_pid(void);
void set_ovs_pid(int pid);
char *get_net_name(void);
int get_ovs_launched(void);
int get_ovsdb_launched(void);
void set_ovs_launched(int val);
void set_ovsdb_launched(int val);
/*---------------------------------------------------------------------------*/

