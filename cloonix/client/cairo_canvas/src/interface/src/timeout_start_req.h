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
typedef struct t_item_node_req
{
  double x;
  double y;
  double tx[MAX_SOCK_VM+MAX_WLAN_VM];
  double ty[MAX_SOCK_VM+MAX_WLAN_VM];
} t_item_node_req;
/*--------------------------------------------------------------------------*/
typedef struct t_item_req
{
  int bank_type;
  char name[MAX_NAME_LEN];
  char path[MAX_PATH_LEN];
  int mutype;
  t_c2c_req_info c2c_req_info;
  double x;
  double y;
  double xa;
  double ya;
  double xb;
  double yb;
} t_item_req;
/*--------------------------------------------------------------------------*/
typedef struct t_edge_req
{
  char name[MAX_PATH_LEN];
  char lan[MAX_NAME_LEN];
  int  num;
} t_edge_req;
/*--------------------------------------------------------------------------*/
typedef struct t_delete_item_req
{
  int bank_type;
  char name[MAX_NAME_LEN];
} t_delete_item_req;
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
void timer_create_item_node_req(void *param);
void timer_create_item_sat_req(void *param);
void timer_create_item_req(void *param);
void timer_create_edge_req(void *param);
/*--------------------------------------------------------------------------*/
void timer_delete_item_req(void *param);
void timer_delete_edge_req(void *param);
/*--------------------------------------------------------------------------*/
void topo_info_update(t_topo_info *topo); 
/*--------------------------------------------------------------------------*/








