/*****************************************************************************/
/*    Copyright (C) 2006-2023 clownix@clownix.net License AGPL-3             */
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
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doorways_sock.h"
#include "client_clownix.h"
#include "interface.h"
#include "commun_consts.h"
#include "bank.h"
#include "lib_topo.h"
#include "utils.h"
#include "move.h"
#include "layout_x_y.h"
#include "main_timer_loop.h"

/****************************************************************************/
#define START_POS 100
void topo_get_matrix_inv_transform_point(double *x, double *y);
/*--------------------------------------------------------------------------*/


/****************************************************************************/
typedef struct t_node_layout
{
  char name[MAX_NAME_LEN];
  int num;
  int hidden_on_graph;
  double x; 
  double y;
  double tx[MAX_ETH_VM];
  double ty[MAX_ETH_VM];
  int32_t thidden_on_graph[MAX_ETH_VM];
  struct t_node_layout *prev;
  struct t_node_layout *next;
} t_node_layout;
/*--------------------------------------------------------------------------*/
typedef struct t_gene_layout
{
  int bank_type;
  char name[MAX_NAME_LEN];
  int hidden_on_graph;
  double x; 
  double y;
  double xa;
  double ya;
  double xb;
  double yb;
  struct t_gene_layout *prev;
  struct t_gene_layout *next;
} t_gene_layout;
/*--------------------------------------------------------------------------*/
static t_node_layout *head_node_layout;
static t_gene_layout *head_gene_layout;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void unset_node_layout(char *name)
{
  t_node_layout *cur = head_node_layout;
  while (cur && strcmp(cur->name, name))
    cur = cur->next;
  if (cur)
    {
    if (cur->next)
      cur->next->prev = cur->prev;
    if (cur->prev)
      cur->prev->next = cur->next;
    if (head_node_layout == cur)
      head_node_layout = cur->next;
    clownix_free(cur, __FUNCTION__);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void unset_gene_layout(int bank_type, char *name)
{
  t_gene_layout *cur = head_gene_layout;
  while (cur && 
        (!((bank_type == cur->bank_type) && (!strcmp(cur->name, name)))))
    cur = cur->next;
  if (cur)
    {
    if (cur->next)
      cur->next->prev = cur->prev;
    if (cur->prev)
      cur->prev->next = cur->next;
    if (head_gene_layout == cur)
      head_gene_layout = cur->next;
    clownix_free(cur, __FUNCTION__);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void set_node_layout_x_y(char *name, 
                         double x, double y, int hidden_on_graph,
                         double *tx, double *ty,
                         int32_t *thidden_on_graph)
{
  int i;
  t_node_layout *nl;
  unset_node_layout(name);
  nl = (t_node_layout *)clownix_malloc(sizeof(t_node_layout),16);
  memset(nl, 0, sizeof(t_node_layout));
  strncpy(nl->name, name, MAX_NAME_LEN-1);
  nl->x = x;
  nl->y = y;
  nl->hidden_on_graph = hidden_on_graph;
  memcpy(nl->tx, tx, (MAX_ETH_VM)*sizeof(double));
  memcpy(nl->ty, ty, (MAX_ETH_VM)*sizeof(double));
  for (i=0; i<MAX_ETH_VM; i++)
    nl->thidden_on_graph[i] = thidden_on_graph[i];
  nl->next = head_node_layout;
  if (nl->next)
    nl->next->prev = nl;
  head_node_layout = nl;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void get_node_layout_x_y(char *name,
                         double *x, double *y, int *hidden_on_graph,
                         double *tx, double *ty,
                         int32_t *thidden_on_graph)
{
  int rest, i;
  t_node_layout *cur = head_node_layout;
  while (cur && strcmp(cur->name, name))
    cur = cur->next;
  if (cur)
    {
    *x = cur->x;
    *y = cur->y;
    *hidden_on_graph = cur->hidden_on_graph;
    memcpy(tx, cur->tx, (MAX_ETH_VM)*sizeof(double));
    memcpy(ty, cur->ty, (MAX_ETH_VM)*sizeof(double));
    for (i=0; i<MAX_ETH_VM; i++)
      thidden_on_graph[i] = cur->thidden_on_graph[i];
    unset_node_layout(name);
    }
  else
    {
    *x = START_POS;
    *y = START_POS;
    *hidden_on_graph = 0;
    memset(tx, 0, (MAX_ETH_VM)*sizeof(double));
    memset(ty, 0, (MAX_ETH_VM)*sizeof(double));
    for (i=0; i<MAX_ETH_VM; i++)
      {
      thidden_on_graph[i] = 0;
      rest = i%4;
      if (rest == 0)
        ty[i] = NODE_DIA * VAL_INTF_POS_NODE;
      if (rest == 1)
        tx[i] = NODE_DIA * VAL_INTF_POS_NODE;
      if (rest == 2)
        ty[i] = -NODE_DIA * VAL_INTF_POS_NODE;
      if (rest == 3)
        tx[i] = -NODE_DIA * VAL_INTF_POS_NODE;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void set_gene_layout_x_y(int bank_type, char *name,
                         double x, double y,
                         double xa, double ya, 
                         double xb, double yb,
                         int hidden_on_graph)
{
  t_gene_layout *vtl;
  unset_gene_layout(bank_type, name);
  vtl = (t_gene_layout *) clownix_malloc(sizeof(t_gene_layout), 16);
  memset(vtl, 0, sizeof(t_gene_layout));
  vtl->bank_type = bank_type;
  strcpy(vtl->name, name);
  vtl->x = x;
  vtl->y = y;
  vtl->xa = xa;
  vtl->ya = ya;
  vtl->xb = xb;
  vtl->yb = yb;
  vtl->hidden_on_graph = hidden_on_graph;
  vtl->next = head_gene_layout;
  if (vtl->next)
    vtl->next->prev = vtl;
  vtl->prev = NULL;
  head_gene_layout = vtl;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void get_gene_layout_x_y(int bank_type, char *name,
                         double *x, double *y,
                         double *xa, double *ya, 
                         double *xb, double *yb,
                         int *hidden_on_graph)
{
  t_gene_layout *cur = head_gene_layout;
  while (cur && 
        (!((bank_type == cur->bank_type) && (!strcmp(cur->name, name)))))
    cur = cur->next;
  if (!cur)
    {
    *x = START_POS;
    *y = START_POS;
    *hidden_on_graph = 0;
    topo_get_matrix_inv_transform_point(x, y);
    }
  else
    {
    *x = cur->x;
    *y = cur->y;
    *xa = cur->xa;
    *ya = cur->ya;
    *xb = cur->xb;
    *yb = cur->yb;
    *hidden_on_graph = cur->hidden_on_graph;
    unset_gene_layout(bank_type, name);
    }
}
/*--------------------------------------------------------------------------*/


