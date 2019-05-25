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
void eventfull_hook_spy(int idx, int len, char *buf);
void eventfull_collect_send(t_all_ctx *all_ctx, int cloonix_llid);
void eventfull_obj_add(char *name, int num, int vm_id,
                      t_eth_params eth_params[MAX_DPDK_VM]);
void eventfull_obj_del(char *name);
int  eventfull_lan_add_endp(char *lan, char *name, int num,
                            t_eth_params *eth_params);
int  eventfull_lan_del_endp(char *lan, char *name, int num);
void eventfull_lan_close(int idx);
void eventfull_lan_open(int idx, char *lan);
void eventfull_init(void);
