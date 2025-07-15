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
#define MAX_NETNS_BUF_LEN 500
#define HEADER_NETNS_MSG 6
#define TOT_NETNS_BUF_LEN (MAX_NETNS_BUF_LEN + HEADER_NETNS_MSG)

void action_add_lan_endp(char *bin, char *db, char *respb,
                         char *lan, char *name, int num, char *vhost);

void action_del_lan_endp(char *bin, char *db, char *respb,
                         char *lan, char *name, int num, char *vhost);

void action_system_promisc(char *bin, char *db, char *respb,  char *vhost);

void action_vhost_up(char *bin, char *db, char *respb,
                     char *name, int num, char *vhost);

void action_add_snf_lan(char *bin, char *db, char *respb,
                        char *name, int num, char *vhost, char *lan);

void action_del_snf_lan(char *bin, char *db, char *respb,
                        char *name, int num, char *vhost, char *lan);

void action_add_lan_rstp(char *bin, char *db, char *respb, char *lan);
void action_add_lan(char *bin, char *db, char *respb, char *lan);

void action_del_lan(char *bin, char *db, char *respb, char *lan);

void action_req_ovsdb(char *bin, char *db, char *net, char *respb);
void action_req_ovs_switch(char *bin, char *db, char *net, char *respb);
void action_req_suidroot(char *respb);
void action_req_destroy(void);
/*---------------------------------------------------------------------------*/

