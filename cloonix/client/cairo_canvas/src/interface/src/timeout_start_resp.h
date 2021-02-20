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
typedef struct t_item_obj_resp
{
  t_topo_kvm *kvm;
  t_topo_c2c *c2c;
  t_topo_d2d *d2d;
  t_topo_a2b *a2b;
  t_topo_sat *sat;
} t_item_obj_resp;
/*--------------------------------------------------------------------------*/
typedef struct t_item_lan_resp
{
  int bank_type;
  int mutype;
  char name[MAX_NAME_LEN];
} t_item_lan_resp;
/*--------------------------------------------------------------------------*/
typedef struct t_item_delete_resp
{
  int bank_type;
  char name[MAX_NAME_LEN];
} t_item_delete_resp;
/*--------------------------------------------------------------------------*/
typedef struct t_edge_resp
{
  int bank_type;
  char name[MAX_PATH_LEN];
  char lan[MAX_NAME_LEN];
  int num;
} t_edge_resp;
/*--------------------------------------------------------------------------*/
void timer_create_item_resp(void *param);
void timer_create_edge_resp(void *param);
void timer_create_obj_resp(void *data);
/*--------------------------------------------------------------------------*/
void timer_delete_item_resp(void *param);
void timer_delete_edge_resp(void *param);
/*--------------------------------------------------------------------------*/








