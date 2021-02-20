/*****************************************************************************/
/*    Copyright (C) 2006-2021 clownix@clownix.net License AGPL-3             */
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
typedef struct t_topo_kvm_chain
{
  t_topo_kvm kvm;
  struct t_topo_kvm_chain *prev;
  struct t_topo_kvm_chain *next;
} t_topo_kvm_chain;
/*--------------------------------------------------------------------------*/
typedef struct t_topo_c2c_chain
{
  t_topo_c2c c2c;
  struct t_topo_c2c_chain *prev;
  struct t_topo_c2c_chain *next;
} t_topo_c2c_chain;
/*--------------------------------------------------------------------------*/
typedef struct t_topo_d2d_chain
{
  t_topo_d2d d2d;
  struct t_topo_d2d_chain *prev;
  struct t_topo_d2d_chain *next;
} t_topo_d2d_chain;
/*--------------------------------------------------------------------------*/
typedef struct t_topo_a2b_chain
{
  t_topo_a2b a2b;
  struct t_topo_a2b_chain *prev;
  struct t_topo_a2b_chain *next;
} t_topo_a2b_chain;
/*--------------------------------------------------------------------------*/
typedef struct t_topo_sat_chain
{
  t_topo_sat sat;
  struct t_topo_sat_chain *prev;
  struct t_topo_sat_chain *next;
} t_topo_sat_chain;
/*--------------------------------------------------------------------------*/
typedef struct t_topo_lan_chain
{
  char lan[MAX_NAME_LEN];
  struct t_topo_lan_chain *prev;
  struct t_topo_lan_chain *next;
} t_topo_lan_chain;
/*--------------------------------------------------------------------------*/
typedef struct t_topo_edge_chain
{
  char name[MAX_NAME_LEN];
  int num;
  char lan[MAX_NAME_LEN];
  struct t_topo_edge_chain *prev;
  struct t_topo_edge_chain *next;
} t_topo_edge_chain;
/*--------------------------------------------------------------------------*/
typedef struct t_topo_differences
{
  t_topo_kvm_chain  *add_kvm;
  t_topo_kvm_chain  *del_kvm;
  t_topo_c2c_chain  *add_c2c;
  t_topo_c2c_chain  *del_c2c;
  t_topo_d2d_chain  *add_d2d;
  t_topo_d2d_chain  *del_d2d;
  t_topo_a2b_chain  *add_a2b;
  t_topo_a2b_chain  *del_a2b;
  t_topo_sat_chain  *add_sat;
  t_topo_sat_chain  *del_sat;
  t_topo_lan_chain  *add_lan;
  t_topo_lan_chain  *del_lan;
  t_topo_edge_chain *add_edge;
  t_topo_edge_chain *del_edge;
} t_topo_differences;
/*--------------------------------------------------------------------------*/
t_topo_differences *topo_get_diffs(t_topo_info *topo, t_topo_info *old_topo);
void topo_free_diffs(t_topo_differences *diffs);
t_topo_info *topo_duplicate(t_topo_info *ref);
int topo_compare(t_topo_info *topoA, t_topo_info *topoB);
void topo_free_topo(t_topo_info *topo);
/*--------------------------------------------------------------------------*/

