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
#define MAX_CHAINED_STATS 10
#define MAX_NUM_LEN 16

enum {
  sysdf_time_ms=0,
  sysdf_uptime,
  sysdf_load1,
  sysdf_load5,
  sysdf_load15,
  sysdf_totalram,
  sysdf_freeram,
  sysdf_cachedram,
  sysdf_sharedram,
  sysdf_bufferram,
  sysdf_totalswap,
  sysdf_freeswap,
  sysdf_procs,
  sysdf_totalhigh,
  sysdf_freehigh,
  sysdf_mem_unit,
  sysdf_process_utime,
  sysdf_process_stime,
  sysdf_process_cutime,
  sysdf_process_cstime,
  sysdf_process_rss,
  sysdf_df,
  sysdf_max,
};


void stats_endp(char *net_name, char *name, int num,
                t_stats_counts *stats_counts);
void stats_sysinfo(char *net_name, char *name,
                   t_stats_sysinfo *si, char *df);
void stats_net_alloc(char *net_name);
void stats_net_free(char *net_name);
void stats_item_alloc(char *net_name, char *name);
void stats_item_free(char *net_name, char *name);
/*--------------------------------------------------------------------------*/
GtkWidget **stats_alloc_grid_var(char *net_name, char *name);
void stats_free_grid_var(char *net_name, char *name);
/*--------------------------------------------------------------------------*/
