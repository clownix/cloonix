/*****************************************************************************/
/*    Copyright (C) 2006-2019 cloonix@cloonix.net License AGPL-3             */
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
enum{
  type_network=1,
  type_graph,
  type_kill,
  type_vm_spice,
  type_vm_ssh,
  type_vm_sys,
  type_vm_kil,
  type_vm_title,
  type_sat_title,
  type_lan_title,
  type_vm,
  type_sat,
  type_lan,
};
int find_names_with_str_iter(char *str_iter, char *net_name, char *name);
void store_net_alloc(char *net_name);
void store_vm_alloc(char *net_name, char *name, int nb_eth, int vm_id);
void store_sat_alloc(char *net_name, char *name, int type);
void store_lan_alloc(char *net_name, char *name);
void store_net_free(char *net_name);
void store_vm_free(char *net_name, char *name);
void store_sat_free(char *net_name, char *name);
void store_lan_free(char *net_name, char *name);
GtkTreeStore *store_get(void);
void store_init(void);
int store_get_vm_id(char *net_name, char *name);
/*---------------------------------------------------------------------------*/
                                                  
