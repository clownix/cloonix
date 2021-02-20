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
#define MY_INFINITY 0xFFFFFFFF
/****************************************************************************/
typedef struct t_chain_node
{
  char name[TOPO_MAX_NAME_LEN];
  int nb_id;
  unsigned long dist;
  struct t_chain_node *next;
  struct t_chain_node *prev;
} t_chain_node;
/****************************************************************************/
void chk_node_name(char *name, const char *fct);
int node_exists(char *target);
void alloc_topo (int max_nodes, int max_neighbors);
void create_node_topo(char *name);
void delete_node_topo(char *node);
void create_link_topo(char *start, char *end, unsigned long dist);
void modif_link_topo (char *start, char *end, unsigned long dist);
void reset_topo (void);
void free_topo (void);
/*---------------------------------------------------------------------------*/
t_chain_node **get_path(char *start, int *nb);
void free_path(int nb, t_chain_node **path);
/*****************************************************************************/




