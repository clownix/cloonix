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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "subsets.h"
#include "dijkstra.h"
#include "io_clownix.h"

/*****************************************************************************/
typedef struct t_link
{
  char src[TOPO_MAX_NAME_LEN+1];
  char dst[TOPO_MAX_NAME_LEN+1];
  struct t_link *prev;
  struct t_link *next;
} t_link;
/*---------------------------------------------------------------------------*/
typedef struct t_node
{
  char name[TOPO_MAX_NAME_LEN+1];
  struct t_node *prev;
  struct t_node *next;
} t_node;
/*---------------------------------------------------------------------------*/
static int glob_max_node;
static int glob_max_neigh;
static t_node *node_head;
static t_link *link_head;
static t_subsets *glob_subset = NULL;
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_node *find_node(char *name)
{
  t_node *cur = node_head;
  while(cur && strcmp(cur->name, name))
    cur = cur->next;
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_link *find_link(char *src, char *dst)
{
  t_link *cur = link_head;
  while  (cur && 
         !((!strcmp(cur->src, src)) && (!strcmp(cur->dst, dst))) &&
         !((!strcmp(cur->src, dst)) && (!strcmp(cur->dst, src))))
    cur = cur->next;
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_link *find_link_one_node(char *src)
{
  t_link *cur = link_head;
  while  (cur && (strcmp(cur->src, src)) && (strcmp(cur->dst, src)))
    cur = cur->next;
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void insert_node(char *name)
{
  t_node *node;
  if (strlen(name) != 4)
    KOUT("%s", name);
  if (!find_node(name))
    {
    node = (t_node *) clownix_malloc(sizeof(t_node), 17);
    memset(node, 0, sizeof(t_node));
    strcpy(node->name, name);
    if (node_head)
      node_head->prev = node;
    node->next = node_head;
    node_head = node;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void insert_link(char *src, char *dst)
{
  t_link *link;
  if (!find_link(src, dst))
    {
    if(!strcmp(src, dst))
      KOUT("%s", src);
    link = (t_link *) clownix_malloc(sizeof(t_link), 17);
    memset(link, 0, sizeof(t_link));
    strcpy(link->src, src);
    strcpy(link->dst, dst);
    if (link_head)
      link_head->prev = link;
    link->next = link_head;
    link_head = link;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void extract_link(char *src, char *dst)
{
  t_link *link = find_link(src, dst);
  if (link)
    {
    if (link->prev)
      link->prev->next = link->next;
    if (link->next)
      link->next->prev = link->prev;
    if (link_head == link)
      link_head = link->next;
    clownix_free(link, __FUNCTION__);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void extract_node(char *name)
{
  t_node *node = find_node(name);
  t_link *link;
  if (node)
    {
    if (node->prev)
      node->prev->next = node->next;
    if (node->next)
      node->next->prev = node->prev;
    if (node_head == node)
      node_head = node->next;
    clownix_free(node, __FUNCTION__);
    }
  link = find_link_one_node(name);
  while(link)
    {
    extract_link(link->src, link->dst);
    link = find_link_one_node(name);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void free_subsets(void)
{
  t_subsets *next, *cur = glob_subset;
  t_subelem *next_elem, *elem;
  glob_subset = NULL;
  while(cur)
    {
    next = cur->next;
    elem = cur->subelem;
    while(elem)
      {
      next_elem = elem->next;
      clownix_free(elem, __FUNCTION__);
      elem = next_elem;
      }
    clownix_free(cur, __FUNCTION__);
    cur = next;
    } 
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
t_subsets *get_subsets(void)
{
  return (glob_subset);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
/* FUNCTION:          sub_mak_link                                           */
/*---------------------------------------------------------------------------*/
void sub_mak_link(char *a, char *b)
{
  insert_node(a);
  insert_node(b);
  insert_link(a, b);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void add_subset(int nb, t_chain_node **path)
{
  int i;
  t_subsets *subset;
  t_subelem *elem;
  subset = (t_subsets *) clownix_malloc(sizeof(t_subsets), 21);
  memset(subset, 0, sizeof(t_subsets));
  for (i=0; i<nb; i++)
    {
    if (path[i]->dist != MY_INFINITY)
      {
      extract_node(path[i]->name);
      elem = (t_subelem *) clownix_malloc(sizeof(t_subelem), 21);
      memset(elem, 0, sizeof(t_subelem));
      strcpy(elem->name, path[i]->name);
      elem->next = subset->subelem;
      subset->subelem = elem;
      }
    }
  subset->next = glob_subset;
  glob_subset = subset;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
/* FUNCTION:           sub_breakdown                                         */
/*---------------------------------------------------------------------------*/
void sub_breakdown(void)
{
  t_chain_node **path;
  char start[4];
  int nb;
  t_node *next_node, *node;
  t_link *link;
  while(node_head)
    { 
    node = node_head;
    link = link_head;
    strcpy(start, node->name);
    nb = 0;
    while(node)
      {
      nb++;
      node = node->next;
      }
    if (nb > glob_max_node)
      KOUT("%d %d", nb, glob_max_node);
    if (nb < 2)
      {
      node = node_head;
      while(node)
        {
        next_node = node->next;
        extract_node(node->name);
        node = next_node;
        }
      }
    else
      {
      node = node_head;
      while(node)
        {
        create_node_topo(node->name);
        node = node->next;
        }
      while(link)
        {
        create_link_topo(link->src, link->dst, 1);
        create_link_topo(link->dst, link->src, 1);
        link = link->next;
        }
      path = get_path(start, &nb);
      add_subset(nb, path);
      free_path(nb, path);
      reset_topo();
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/* FUNCTION:              subsets_init                                       */
/*---------------------------------------------------------------------------*/
void sub_init(int max_nodes, int max_neigh)
{
  glob_max_node = max_nodes + 10;
  glob_max_neigh = max_neigh + 10;
  free_topo();
  alloc_topo (glob_max_node, glob_max_neigh);
  node_head = NULL;
  link_head = NULL;
  glob_subset = NULL;
}
/*---------------------------------------------------------------------------*/

