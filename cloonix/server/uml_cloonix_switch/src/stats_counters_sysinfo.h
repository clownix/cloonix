/*****************************************************************************/
/*    Copyright (C) 2006-2023 clownix@clownix.net License AGPL-3             */
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
void stats_counters_sysinfo_vm_death(char *name);
void stats_counters_sysinfo_llid_close(int llid);
void stats_counters_sysinfo_update_df(char *name, char *payload);
void stats_counters_sysinfo_update(char *name,
                                   unsigned long uptime,
                                   unsigned long load1,
                                   unsigned long load5,
                                   unsigned long load15,
                                   unsigned long totalram,
                                   unsigned long freeram,
                                   unsigned long cachedram,
                                   unsigned long sharedram,
                                   unsigned long bufferram,
                                   unsigned long totalswap,
                                   unsigned long freeswap,
                                   unsigned long procs,
                                   unsigned long totalhigh,
                                   unsigned long freehigh,
                                   unsigned long mem_unit);
void stats_counters_sysinfo_init(void);
