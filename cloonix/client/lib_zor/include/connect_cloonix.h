/*****************************************************************************/
/*    Copyright (C) 2006-2018 cloonix@cloonix.net License AGPL-3             */
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
typedef void (*t_evt_net_exists_cb)(char *net_name, int exists);
typedef void (*t_evt_vm_exists_cb)(char *net_name, char *name,
                                   int nb_eth, int vm_id, int exists);
typedef void (*t_evt_sat_exists_cb)(char *net_name, char *name, 
                                    int type, int exists);
typedef void (*t_evt_lan_exists_cb)(char *net_name, char *name, int exists);
typedef void (*t_evt_stats_endp_cb)(char *net_name, char *name, int num, 
                                    t_stats_counts *stats_counts);
typedef void (*t_evt_stats_sysinfo_cb)(char *net_name, char *name, 
                                       t_stats_sysinfo *stats,
                                       char *df);
/*--------------------------------------------------------------------------*/
char *get_cloonix_work_dir(char *net_name);
/*--------------------------------------------------------------------------*/
void connect_cloonix_init(char *conf_path,
                          t_evt_net_exists_cb  net_exists,
                          t_evt_vm_exists_cb   vm_exists,
                          t_evt_sat_exists_cb  sat_exists,
                          t_evt_lan_exists_cb  lan_exists,
                          t_evt_stats_endp_cb  stats_endp,
                          t_evt_stats_sysinfo_cb stats_sysinfo);
/****************************************************************************/







