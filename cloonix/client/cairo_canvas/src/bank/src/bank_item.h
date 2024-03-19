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
typedef struct t_head_bank
{
  t_bank_item *head;
  int count_nb_items;
} t_head_bank;
/*--------------------------------------------------------------------------*/
void bank_add_item(int bank_type, char *name, char *lan, int num,
                   t_bank_item *intf, t_bank_item *blan, int eorig);
void bank_del_item(t_bank_item *bank_item, int eorig);
void delete_bitem(t_bank_item *bitem);
/*--------------------------------------------------------------------------*/
void selectioned_item_init(void);
void selectioned_item_delete(t_bank_item *bitem);
/*--------------------------------------------------------------------------*/
int add_new_cnt(char *type, char *name, char *image, int vm_id, 
                double x, double y, int hidden_on_graph,
                int ping_ok, int nb_tot_eth, t_eth_table *eth_tab);
/*--------------------------------------------------------------------------*/
int add_new_node(char *name, char *kernel, char *rootfs_used,
                 char *rootfs_backing,  char *install_cdrom, 
                 char *added_cdrom, char *added_disk,
                 double x, double y, int hidden_on_graph, 
                 int color_choice, int vm_id, int vm_config_flags,
                 int nb_tot_eth, t_eth_table *eth_tab);

int add_new_eth(char *name, int num,
                 double x, double y, int hidden_on_graph);
int add_new_lan(char *name, double x, double y, int hidden_on_graph);
int add_new_sat(char *name, int endp_type,
                t_topo_c2c *c2c, t_topo_a2b *a2b, 
                double x, double y, int hidden_on_graph);
void add_new_edge(t_bank_item *bi_eth, t_bank_item *bi_lan, int eorig);

/*--------------------------------------------------------------------------*/





