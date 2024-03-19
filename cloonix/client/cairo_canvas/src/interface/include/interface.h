/*****************************************************************************/
/*    Copyright (C) 2006-2024 clownix@clownix.net License AGPL-3             */
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
typedef struct t_c2c_req_info
{
  char name[MAX_NAME_LEN];
  uint32_t loc_udp_ip;
  char dist_cloon[MAX_NAME_LEN];
  char dist_passwd[MSG_DIGEST_LEN];
  uint32_t dist_tcp_ip;
  uint16_t dist_tcp_port;
  uint32_t dist_udp_ip;
} t_c2c_req_info;

void to_cloonix_switch_create_node(int is_cnt, double x, double y,
                                   double *tx, double *ty);
/*--------------------------------------------------------------------------*/
void to_cloonix_switch_create_sat(char *name, int type,
                                  t_c2c_req_info *c2c, 
                                  double x, double y);
/*--------------------------------------------------------------------------*/
void to_cloonix_switch_create_lan(char *lan, double x, double y);
/*--------------------------------------------------------------------------*/
void to_cloonix_switch_delete_node(char *name);
/*--------------------------------------------------------------------------*/
void to_cloonix_switch_delete_sat(char *name);
/*--------------------------------------------------------------------------*/
void to_cloonix_switch_delete_lan(char *lan);
/*--------------------------------------------------------------------------*/
void to_cloonix_switch_create_edge(char *name, int num, char *lan);
/*--------------------------------------------------------------------------*/
void to_cloonix_switch_delete_edge(char *name, int num, char *lan);
/*--------------------------------------------------------------------------*/
void from_cloonix_switch_create_cnt(t_topo_cnt *cnt);
void from_cloonix_switch_create_node(t_topo_kvm *kvm);
void from_cloonix_switch_create_c2c(t_topo_c2c *c2c);
void from_cloonix_switch_create_a2b(t_topo_a2b *a2b);
void from_cloonix_switch_create_tap(t_topo_tap *tap);
void from_cloonix_switch_create_nat(t_topo_nat *nat);
void from_cloonix_switch_create_phy(t_topo_phy *phy);
/*--------------------------------------------------------------------------*/
void from_cloonix_switch_create_lan(char *lan);
/*--------------------------------------------------------------------------*/
void from_cloonix_switch_create_edge(char *name, int num, char *lan);
/*--------------------------------------------------------------------------*/
void from_cloonix_switch_delete_cnt(char *name);
void from_cloonix_switch_delete_node(char *name);
/*--------------------------------------------------------------------------*/
void from_cloonix_switch_delete_sat(char *name);
/*--------------------------------------------------------------------------*/
void from_cloonix_switch_delete_lan(char *lan);
/*--------------------------------------------------------------------------*/
void from_cloonix_switch_delete_edge(char *name, int num, char *lan);
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void interface_switch_init(char *path, char *password);
/*--------------------------------------------------------------------------*/
void launch_xterm_double_click(char *name, int vm_config_flags);
/*--------------------------------------------------------------------------*/
int get_vm_id_from_topo(char *name);
/*--------------------------------------------------------------------------*/
void timer_layout_subscribe(void *data);
/*--------------------------------------------------------------------------*/
void to_cloonix_switch_dyn_snf_req(int bank_type, char *name, int num, int on);
/*--------------------------------------------------------------------------*/












