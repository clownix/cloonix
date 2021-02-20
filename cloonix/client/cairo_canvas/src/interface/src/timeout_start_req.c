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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doorways_sock.h"
#include "client_clownix.h"
#include "bank.h"
#include "interface.h"
#include "timeout_start_req.h"

t_topo_info *get_current_topo(void);




/****************************************************************************/
static void gene_create_item_req(int bank_type, char *name, char *path,
                                 int mutype, double x, double y)
{
  t_item_req *pa;
  pa = (t_item_req *) clownix_malloc(sizeof(t_item_req), 12);
  memset(pa, 0, sizeof(t_item_req));
  pa->bank_type = bank_type;
  strncpy(pa->name, name, MAX_NAME_LEN-1);
  strncpy(pa->path, path, MAX_PATH_LEN-1);
  pa->mutype = mutype;
  pa->x = x;
  pa->y = y;
  clownix_timeout_add(1, timer_create_item_req, (void *)pa, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void gene_delete_item_req(int bank_type, char *name)
{
  t_delete_item_req *pa;
  pa = (t_delete_item_req *)clownix_malloc(sizeof(t_delete_item_req), 12);
  memset(pa, 0, sizeof(t_delete_item_req));
  pa->bank_type = bank_type;
  strncpy(pa->name, name, MAX_NAME_LEN-1);
  clownix_timeout_add(1, timer_delete_item_req, (void *)pa, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void to_cloonix_switch_create_node(double x, double y,
                                   double *tx, double *ty)
{
  t_item_node_req *pa;
  pa = (t_item_node_req *) clownix_malloc(sizeof(t_item_node_req), 12);
  memset(pa, 0, sizeof(t_item_node_req));
  pa->x = x;
  pa->y = y;
  memcpy(pa->tx, tx, MAX_SOCK_VM+MAX_WLAN_VM * sizeof(double));
  memcpy(pa->ty, ty, MAX_SOCK_VM+MAX_WLAN_VM * sizeof(double));
  clownix_timeout_add(1, timer_create_item_node_req,(void *)pa, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void to_cloonix_switch_create_sat(char *name, int mutype, 
                                  t_d2d_req_info *d2d_req_info,
                                  t_c2c_req_info *c2c_req_info,
                                  double x, double y)
{
  t_item_req *pa;
  pa = (t_item_req *) clownix_malloc(sizeof(t_item_req), 12);
  memset(pa, 0, sizeof(t_item_req));
  pa->bank_type = bank_type_sat;
  strncpy(pa->name, name, MAX_NAME_LEN-1);
  pa->mutype = mutype;
  if (c2c_req_info)
    memcpy(&(pa->c2c_req_info), c2c_req_info, sizeof(t_c2c_req_info));
  if (d2d_req_info)
    memcpy(&(pa->d2d_req_info), d2d_req_info, sizeof(t_d2d_req_info));
  pa->x = x;
  pa->y = y;
  clownix_timeout_add(1, timer_create_item_req, (void *)pa, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void to_cloonix_switch_create_lan(char *lan, double x, double y)
{
  gene_create_item_req(bank_type_lan, lan, "", mulan_type, x, y); 
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void to_cloonix_switch_delete_node(char *name)
{
  t_bank_item *node = look_for_node_with_id(name);
  if (node)
    gene_delete_item_req(node->bank_type, name);      
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void to_cloonix_switch_delete_sat(char *name)
{
  gene_delete_item_req(bank_type_sat, name);      
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void to_cloonix_switch_delete_lan(char *lan)
{
  t_topo_info *topo;
  topo = get_current_topo();
  topo_info_update(topo);
  gene_delete_item_req(bank_type_lan, lan);      
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void to_cloonix_switch_create_edge(char *name, int num, char *lan)
{
  t_edge_req *pa;
  pa=(t_edge_req *)clownix_malloc(sizeof(t_edge_req), 12);
  memset(pa, 0, sizeof(t_edge_req));
  strncpy(pa->name, name, MAX_NAME_LEN-1);
  strncpy(pa->lan, lan, MAX_NAME_LEN-1);
  pa->num = num;
  clownix_timeout_add(1,timer_create_edge_req,(void *)pa, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void to_cloonix_switch_delete_edge(char *name, int num, char *lan)
{
  t_edge_req *pa;
  pa=(t_edge_req *)clownix_malloc(sizeof(t_edge_req), 12);
  memset(pa, 0, sizeof(t_edge_req));
  strncpy(pa->name, name, MAX_NAME_LEN-1);
  strncpy(pa->lan, lan, MAX_NAME_LEN-1);
  pa->num = num;
  clownix_timeout_add(1,timer_delete_edge_req,(void *)pa, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

