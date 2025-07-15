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
typedef struct t_ovs_tap
{
  char name[MAX_NAME_LEN];
  char vhost[MAX_NAME_LEN];
  unsigned char mac_addr[8];
  char lan[MAX_NAME_LEN];
  char lan_added[MAX_NAME_LEN];
  int  del_tap_req;
  int  del_snf_ethv_sent;
  int  llid;
  int  tid;
  int  tap_id;
  int  endp_type;
  struct t_ovs_tap *prev;
  struct t_ovs_tap *next;
} t_ovs_tap;

t_ovs_tap *ovs_tap_get_first(int *nb_tap);
void ovs_tap_resp_msg_tap(int is_ko, int is_add, char *name);
void ovs_tap_resp_add_lan(int is_ko, char *name, int num,
                          char *vhost, char *lan);
void ovs_tap_resp_del_lan(int is_ko, char *name, int num,
                          char *vhost, char *lan);
t_ovs_tap *ovs_tap_exists(char *name);
int  ovs_tap_dyn_snf(char *name, int val);
void ovs_tap_add(int llid, int tid, char *name);
void ovs_tap_del(int llid, int tid, char *name);
void ovs_tap_add_lan(int llid, int tid, char *name, char *lan);
void ovs_tap_del_lan(int llid, int tid, char *name, char *lan);
t_topo_endp *ovs_tap_translate_topo_endp(int *nb);
void ovs_tap_init(void);
/*--------------------------------------------------------------------------*/
