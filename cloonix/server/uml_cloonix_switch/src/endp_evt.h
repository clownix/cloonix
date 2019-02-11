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
int  endp_evt_exists(char *name, int num);
int  endp_evt_lan_full(char *name, int num, int *tidx);
int  endp_evt_lan_find(char *name, int num, char *lan, int *tidx);
int  endp_evt_lan_is_in_use(char *lan);
void endp_evt_add_lan(int llid, int tid, char *name, int num,
                      char *lan, int tidx);
int  endp_evt_del_lan(char *name, int num, int tidx, char *lan);
void endp_evt_mulan_birth(char *lan);
void endp_evt_mulan_death(char *lan);
void endp_evt_connect_OK(char *name, int num, char *lan, int tidx, int rank);
void endp_evt_connect_KO(char *name, int num, char *lan, int tidx);
void endp_evt_birth(char *name, int num, int endp_type);
int  endp_evt_death(char *name, int num);
void endp_evt_quick_death(char *name, int num);
int endp_is_wlan(char *name, int num, int *nb_eth);
void endp_evt_init(void);
