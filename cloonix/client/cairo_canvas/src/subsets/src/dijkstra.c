/*****************************************************************************/
/*    Copyright (C) 2006-2018 cloonix@cloonix.net License AGPL-3             */
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
#include "subsets.h"
#include "dijkstra.h"
/*****************************************************************************/
#define SIG_TOPO_LOADING 0xCAFECAFE
#define SIG_TOPO_WAITEND 0xDECADECA
/*****************************************************************************/
typedef char t_node_name[TOPO_MAX_NAME_LEN]; 
/*****************************************************************************/
typedef struct t_links
{
  char name[TOPO_MAX_NAME_LEN];
  unsigned long dist;
  unsigned long idx_name;
} t_links;
/*****************************************************************************/
typedef struct t_topo_summary
{
  t_node_name *nodes_tab;
  int32_t *node_free_index;
  int node_read_idx;
  int node_write_idx;
  t_links **links_tab;
  t_links *nei_links_tab;
} t_topo_summary;
/*****************************************************************************/
typedef struct t_element
  {
  int index;
  unsigned long dist;
  } t_element;
/*****************************************************************************/
typedef struct t_chain_inode
  {
  int iname;
  unsigned long dist;
  struct t_chain_inode *next;
  struct t_chain_inode *prev;
  } t_chain_inode;
/****************************************************************************/

typedef struct t_topo
  {
  unsigned long sig_topo_state;
  unsigned long max_nodes;
  unsigned long max_neighbors;
  unsigned long max_links;
  unsigned long hash_val; 
  unsigned long hash_bits;
  int nb_nodes_added;
  int nb_nodes_del;
  int nb_nodes;
  unsigned long nb_links;
  unsigned long read_idx;
  unsigned long write_idx;
  t_topo_summary summary;
  t_element *nei_matrix;
  t_element **matrix;
  t_element *best_prev;
  t_chain_inode *list_node;
  int32_t *hash2index_tab;
  t_node_name *hash2name_tab;
  } t_topo;
/*****************************************************************************/
static t_topo mytopo;
/*---------------------------------------------------------------------------*/
static int nb_topoalloc = 0;
static int nb_node_alloc = 0;
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/* FUNCTION:           summary_node_alloc                                    */
/*---------------------------------------------------------------------------*/
static void summary_node_alloc (int *p_idx)
{
  t_topo *top = &mytopo;
  if(top->summary.node_read_idx == top->summary.node_write_idx)
    KOUT(" ");
  *p_idx = top->summary.node_free_index[top->summary.node_read_idx];
  top->summary.node_read_idx  = 
  (top->summary.node_read_idx + 1) % (top->max_nodes - 1);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/* FUNCTION:           summary_node_free                                     */
/*---------------------------------------------------------------------------*/
static void summary_node_free (int idx)
{
  t_topo *top = &mytopo;
  top->summary.node_free_index[top->summary.node_write_idx] =  idx;
  top->summary.node_write_idx =
  (top->summary.node_write_idx + 1) % (top->max_nodes - 1);
}
/*****************************************************************************/

/*****************************************************************************/
/* FUNCTION:            topoalloc                                           */
/*---------------------------------------------------------------------------*/
static void *topoalloc (size_t size)
{
  void *res = clownix_malloc(size, 22);
  nb_topoalloc++;
  if (!res)
    KOUT(" ");
  return(res);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/* FUNCTION:           topofree                                             */
/*---------------------------------------------------------------------------*/
static void topofree (void * addr)
{
  if (addr)
    {
    nb_topoalloc--;
    clownix_free (addr, __FUNCTION__);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/* FUNCTION:           node_alloc                                           */
/*---------------------------------------------------------------------------*/
static void *node_alloc (size_t size)
{
  void *res = clownix_malloc(size, 22);
  nb_node_alloc++;
  if (!res)
    KOUT(" ");
  return(res);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/* FUNCTION:           node_free                                             */
/*---------------------------------------------------------------------------*/
static void node_free (void * addr)
{
  if (addr)
    {
    nb_node_alloc--;
    clownix_free (addr, __FUNCTION__);
    }
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
/* FUNCTION:           chk_node_name                                         */
/*---------------------------------------------------------------------------*/
void chk_node_name(char *name, const char *fct)
{
  if (!name)
    KOUT("%s", fct);
  if ((name[0] == 0) || (strlen(name) >= TOPO_MAX_NAME_LEN))
    KOUT("%s  %s", fct, name);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/* FUNCTION:           alloc_node                                            */
/*---------------------------------------------------------------------------*/
static t_chain_node *alloc_node(char *name, int nb_id, unsigned long dist)
{
  t_chain_node *result;
  chk_node_name(name, __FUNCTION__);
  result = (t_chain_node *)node_alloc(sizeof(t_chain_node));
  strcpy (result->name, name);
  result->nb_id = nb_id;
  result->dist = dist;
  result->next = NULL;
  result->prev = NULL;
  return result;
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
/* FUNCTION:           get_hashing_val                                       */
/*---------------------------------------------------------------------------*/
static void get_hashing_val(int maxnode, unsigned long *hash_val, 
                                         unsigned long *hash_bits)
{
  unsigned long power_of_2;
  int i, found = 0;
  int chosen_size_min = 5*maxnode + 10;
  *hash_val = 0;
  *hash_bits = 0;
  for (i=0;(!found);i++)
    {
    power_of_2 = 1 << i;
    if (power_of_2 > chosen_size_min)
      {
      *hash_val = (unsigned long) power_of_2 - 1;
      *hash_bits = i;
      found = 1;
      }
    }
  if (power_of_2 >= 0xFFFF)
    KOUT("%lu\n", power_of_2);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/* FUNCTION:           index_hash_code                                       */
/*---------------------------------------------------------------------------*/
static int index_hash_code (char *node_name, unsigned long hash_val, 
                                             unsigned long hash_bits)
{
  int i, half_len = strlen(node_name)/2;
  unsigned long hash = 0;
  unsigned long index;
  for (i=0; i<=half_len; i++)
    hash ^= node_name[2*i]<<8 | node_name[2*i+1]; 
  hash ^= (hash >> (16 - hash_bits));
  index = hash & hash_val;
  if ((index == 0)||(index == hash_val))
    index = 1;
  return (index);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/* FUNCTION:           name2index                                            */
/*---------------------------------------------------------------------------*/
static int name2index(char *node_name)
{
  int index;
  unsigned long count = 0;
  int end = 0;
  t_topo *top = &mytopo;
  index = index_hash_code (node_name, top->hash_val, top->hash_bits);
  while ((strcmp(top->hash2name_tab[index], node_name))&&(!end))
    {
    count++;
    index++;
    index %= top->hash_val;
    if (index == 0) 
      index++;
    if (count == top->hash_val)
      {
      index = 0;
      end = 1;
      }
    }
  index = top->hash2index_tab[index];
  return index;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/* FUNCTION:           index2name                                            */
/*---------------------------------------------------------------------------*/
static char *index2name(int idx)
{
  t_topo *top = &mytopo;
  char *result = NULL;
  if (top->summary.nodes_tab[idx][0])
    result = top->summary.nodes_tab[idx];
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/* FUNCTION:           create_index_2_name                                   */
/*---------------------------------------------------------------------------*/
static void create_index_2_name(char *node_name, int idx)
{
  int hash_index;
  unsigned long  count = 0;
  t_topo *top = &mytopo;

  chk_node_name(node_name, __FUNCTION__);
  hash_index = index_hash_code (node_name, top->hash_val, top->hash_bits);
  while ((top->hash2name_tab[hash_index][0] != 0)||(hash_index == 0))
    {
    count++;
    hash_index++;
    hash_index %= top->hash_val;
    if (hash_index == 0)
      hash_index++;
    if (count == top->hash_val)
      KOUT(" ");
    }
  strcpy (top->hash2name_tab[hash_index], node_name);
  strcpy (top->summary.nodes_tab[idx], node_name); 
  top->hash2index_tab[hash_index] = idx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/* FUNCTION:           delete_index_2_name                                   */
/*---------------------------------------------------------------------------*/
static void delete_index_2_name(char *node_name)
{
  int index, hash_index, count = 0;
  t_topo *top = &mytopo;
  index = index_hash_code (node_name, top->hash_val, top->hash_bits);
  while (strcmp(top->hash2name_tab[index], node_name))
    {
    count++;
    index++;
    index %= top->hash_val;
    if (index == 0)
      index++;
    if (count == top->hash_val)
      KOUT(" ");
    }
  hash_index = index;
  index = top->hash2index_tab[hash_index];
  if (index == 0)
      KOUT(" ");
  if (strcmp(top->summary.nodes_tab[index], node_name))
      KOUT(" ");
  top->hash2name_tab[hash_index][0] = 0;
  top->hash2index_tab[hash_index] = 0;
  top->summary.nodes_tab[index][0] = 0;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
/* FUNCTION:           put_in_list                                           */
/*---------------------------------------------------------------------------*/
static t_chain_inode *put_in_list(t_chain_inode *list,t_chain_inode *node)
{
  t_chain_inode *curr;
  t_chain_inode *sav_curr=NULL;

  if (!node->iname)
    KOUT(" ");
  if (list == NULL)
    {
    curr = node;
    }
  else
    {
    for (curr=list; (curr!=NULL)&&(curr->dist < node->dist); curr=curr->next)
      {
      sav_curr = curr;
      }
    if (curr == NULL)
      {
      node->prev = sav_curr;
      sav_curr->next = node;
      curr = list;
      }
    else if (curr->prev == NULL)
      {
      if (curr != list)
        KOUT(" ");
      node->next = curr; 
      curr->prev = node;
      curr = node;
      }
    else 
      {
      node->prev = curr->prev;
      node->next = curr;
      curr->prev->next = node;
      curr->prev = node;
      curr = list;
      }
    }
  return curr;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/* FUNCTION:           fast_put_in_list                                      */
/*---------------------------------------------------------------------------*/
static void fast_put_in_list( t_chain_inode *list, t_chain_inode *node)
{
  t_chain_inode *curr;
  t_chain_inode *sav_curr=NULL;

    for (curr=list; (curr!=NULL)&&(curr->dist < node->dist); curr=curr->next)
      {
      }
    if (curr != NULL)
     {
      node->prev = curr->prev;
      node->next = curr;
      curr->prev->next = node;
      curr->prev = node;
      }
    else
      {
      for (curr=list; (curr!=NULL)&&(curr->dist < node->dist); curr=curr->next)
        {
        sav_curr = curr;
        }
      node->prev = sav_curr;
      sav_curr->next = node;
      }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/* FUNCTION:           free_list                                             */
/*---------------------------------------------------------------------------*/
static void free_list(t_chain_node *list)
{
  t_chain_node *p_node;
  t_chain_node *p_node_next;

  p_node = list;
  while (p_node)
    {
    p_node_next = p_node->next;
    node_free(p_node);
    p_node = p_node_next;
    }
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
/* FUNCTION:           dijkstra                                              */
/*---------------------------------------------------------------------------*/
static void dijkstra (int idx_start)
{
  struct t_chain_inode *wtlist = NULL, *wt1;
  int i, j, cand, newest, idst;
  unsigned long q0, q1, q2, q3, dist;
  int32_t *index;
  t_topo *top = &mytopo;
  memset(top->nei_matrix,0,top->max_nodes*top->max_neighbors*sizeof(t_element));
  memset(top->list_node, 0, top->max_nodes*sizeof(t_chain_inode));
  for (i=1; i < top->max_nodes+1; i++)
    {
    top->best_prev[i].index = 0;
    top->best_prev[i].dist = MY_INFINITY;
    }
  index = (int *)clownix_malloc(top->max_nodes*sizeof(int64_t), 22);
  memset (index, 0, top->max_nodes*sizeof(int64_t));
  for (i=1; i<top->nb_nodes_added+1; i++)
    {
    if (top->summary.nodes_tab[i][0] == 0)
      continue;
    for (j=0; j<top->max_neighbors; j++)
      {
      if (top->summary.links_tab[i][j].name[0])
        {
        idst = top->summary.links_tab[i][j].idx_name;
        dist = top->summary.links_tab[i][j].dist;
        top->matrix[i][index[i]].index = idst;
        top->matrix[i][index[i]].dist = dist;
        if (i == idx_start)
          {
          top->best_prev[idst].index = i;
          top->best_prev[idst].dist = dist;
          top->list_node[idst].iname = idst;
          top->list_node[idst].dist = dist;
          top->list_node[idst].next = NULL;
          top->list_node[idst].prev = NULL;
          wtlist = put_in_list(wtlist, &(top->list_node[idst]));
          }
        index[i] += 1;
        }
      }
    }
  clownix_free (index, __FUNCTION__);
  top->best_prev[idx_start].index = idx_start;
  top->best_prev[idx_start].dist = 0;
  for (i=1; i < top->nb_nodes_added+1; i++)
    {
    if (top->summary.nodes_tab[i][0] == 0)
      continue;
    if ((i != idx_start)&&(!top->list_node[i].iname))
      {
      top->list_node[i].iname = i;
      top->list_node[i].dist = MY_INFINITY;
      top->list_node[i].next = NULL;
      top->list_node[i].prev = NULL;
      wtlist = put_in_list(wtlist, &(top->list_node[i]));
      }
    }
  for (wt1 = wtlist; wt1 != NULL; wt1 = wt1->next)
    {
    newest = wt1->iname;
    for (i=0; i<top->max_neighbors; i++)
      {
      cand = top->matrix[newest][i].index;
      if (!cand) break;      
      if (cand == idx_start)
        continue;
      q0 = top->best_prev[newest].dist;
      q2 = top->matrix[newest][i].dist;
      if ((q2 != MY_INFINITY)&&(q0 != MY_INFINITY))
        {
        q1 = top->best_prev[cand].dist; 
        q3 = q2+q0;
        if (q3 < q1)
          {
          top->best_prev[cand].index = newest;
          top->best_prev[cand].dist = q3;
          top->list_node[cand].dist = q3;
          if (top->list_node[cand].prev->dist > q3)
            {
            top->list_node[cand].prev->next = top->list_node[cand].next;
            if (top->list_node[cand].next)
              top->list_node[cand].next->prev = top->list_node[cand].prev;
            top->list_node[cand].next = NULL;
            top->list_node[cand].prev = NULL;
            fast_put_in_list(&(top->list_node[newest]),&(top->list_node[cand]));
            }
          }
        }
      }
    }
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
/* FUNCTION:           get_path                                              */
/*---------------------------------------------------------------------------*/
t_chain_node **get_path(char *start, int *nb)
{
  int i, start_idx, prev_idx, len;
  char *node;
  char *prev_node;
  t_chain_node *p_node=NULL;
  t_chain_node *p_last_path;
  t_chain_node *p_ref_node;
  t_chain_node **path;
  t_topo *top = &mytopo;
  if (top->nb_nodes < 2)
    KOUT("%d", top->nb_nodes);
  chk_node_name(start, __FUNCTION__);
  start_idx = name2index(start);
  if (start_idx == 0)
    KOUT(" ");
  *nb = 0;
  len = (top->nb_nodes_added+1)*sizeof(t_chain_node *);
  path = (t_chain_node **)node_alloc(len);
  memset (path, 0, len);
  dijkstra (start_idx);
  for (i=1; i<top->nb_nodes_added+1; i++)
    {
    if (path[*nb])
      KOUT(" ");
    node = index2name(i);
    if (!node)
      continue;
    p_last_path = alloc_node(node, i, top->best_prev[i].dist);
    p_ref_node = alloc_node(node, i, top->best_prev[i].dist);
    path[*nb] = p_ref_node;
    p_ref_node->next = p_last_path; 
    p_last_path->prev = p_ref_node;
    prev_idx = i;
    if (top->best_prev[prev_idx].dist != MY_INFINITY)
      {
      do
        {
        prev_idx = top->best_prev[prev_idx].index;
        prev_node = index2name(prev_idx);
        if (!prev_node)
          KOUT(" ");
        if (top->best_prev[prev_idx].dist == MY_INFINITY)
          KOUT(" ");
        p_node=alloc_node(prev_node, prev_idx, top->best_prev[prev_idx].dist);
        p_node->prev = p_last_path;
        p_last_path->next = p_node;
        p_last_path = p_node;
        } while (prev_idx != start_idx); 
      while (p_node)
        {
        if (p_node->prev != path[*nb])
          {
          p_node->next = p_node->prev;
          p_node = p_node->prev;
          }
        else
          {
          path[*nb]->next = p_last_path;
          p_node->next = NULL;
          break;
          }
        } 
      }
    (*nb)++;
    }
  if (top->nb_nodes_added != top->nb_nodes + top->nb_nodes_del)
    KOUT("%d %d %d\n", top->nb_nodes, top->nb_nodes_del, top->nb_nodes_added);
  if (*nb != top->nb_nodes)
    KOUT("%d %d\n", *nb, top->nb_nodes);
  return path;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
/* FUNCTION:           alloc_topo                                            */
/*---------------------------------------------------------------------------*/
void reset_topo (void)
{
  int i;
  t_topo *top = &mytopo;
  memset(top->summary.nodes_tab,0,top->max_nodes*sizeof(t_node_name));
  memset(top->summary.nei_links_tab, 0, top->max_links*sizeof(t_links));
  memset(top->hash2name_tab, 0, top->hash_val*sizeof(t_node_name));
  for (i=0; i<top->hash_val; i++)
    top->hash2index_tab[i] = 0;
  top->nb_nodes = 0;
  top->nb_nodes_added = 0;
  top->nb_nodes_del = 0;
  top->nb_links = 0;
  for(i = 0; i < top->max_nodes - 1; i++)
    top->summary.node_free_index[i] = i+1;
  top->summary.node_read_idx = 0;
  top->summary.node_write_idx =  top->max_nodes - 2;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/* FUNCTION:           alloc_topo                                            */
/*---------------------------------------------------------------------------*/
void alloc_topo (int imax_nodes, int imax_neighbors)
{
  int i;
  unsigned long hash_val, hash_bits;
  int max_nodes, max_neighbors;
  int alloc_size;
  t_topo *top = &mytopo;
  if (top->sig_topo_state)
    KOUT(" ");
  nb_topoalloc = 0;
  max_nodes = imax_nodes + 20;
  max_neighbors = imax_neighbors + 20;
  get_hashing_val( max_nodes, &hash_val, &hash_bits);
  top->sig_topo_state = SIG_TOPO_LOADING;
  top->max_nodes = max_nodes;
  top->max_neighbors = max_neighbors;
  top->max_links = max_nodes*max_neighbors;
  top->hash_val = hash_val;
  top->hash_bits = hash_bits;
  alloc_size = max_nodes * sizeof(t_node_name);
  top->summary.nodes_tab = (t_node_name *) topoalloc(alloc_size);
  alloc_size = top->max_links*sizeof(t_links);
  top->summary.nei_links_tab = (t_links *) topoalloc(alloc_size);
  alloc_size = max_nodes * sizeof(t_links *);
  top->summary.links_tab = (t_links **) topoalloc(alloc_size);
  top->summary.node_free_index = (int32_t *) topoalloc((max_nodes)*sizeof(int64_t));
  top->hash2index_tab = (int32_t *) topoalloc(top->hash_val*sizeof(int64_t));
  alloc_size = top->hash_val * sizeof(t_node_name);
  top->hash2name_tab = (t_node_name *) topoalloc(alloc_size);
  alloc_size = ((max_nodes*max_neighbors+1)+max_nodes+1)*sizeof(t_element);
  alloc_size += (max_nodes+1)*sizeof(t_chain_inode);
  top->nei_matrix = (t_element *) topoalloc(alloc_size);
  top->matrix = (t_element **) topoalloc(max_nodes*sizeof(t_element *));
  top->best_prev = &(top->nei_matrix[top->max_links]);
  top->list_node=(t_chain_inode*)(&(top->nei_matrix[top->max_links+max_nodes]));
  for (i=0; i<max_nodes; i++)
    {
    top->matrix[i] = &(top->nei_matrix[max_neighbors*i]);
    top->summary.links_tab[i] = &(top->summary.nei_links_tab[max_neighbors*i]);
    }
  reset_topo();
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/* FUNCTION:           free_topo                                             */
/*---------------------------------------------------------------------------*/
void free_topo  (void)
{
  t_topo *top = &mytopo;
  topofree(top->hash2name_tab);
  topofree(top->hash2index_tab);
  topofree(top->summary.nei_links_tab);
  topofree(top->summary.links_tab);
  topofree(top->summary.nodes_tab);
  topofree(top->summary.node_free_index);
  topofree(top->nei_matrix);
  topofree(top->matrix);
  memset (top, 0, sizeof(t_topo));
  if (nb_topoalloc)
    KOUT("%d", nb_topoalloc);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/* FUNCTION:             free_path                                           */
/*---------------------------------------------------------------------------*/
void free_path (int nb, t_chain_node **path)
{
  int i;
  for (i=0; i<nb; i++)
    free_list(path[i]);
  node_free(path);
  if (nb_node_alloc)
    KOUT("%d", nb_node_alloc);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/* FUNCTION:           create_node_topo                                      */
/*---------------------------------------------------------------------------*/
void create_node_topo(char *node)
{
  int i, found_idx;
  t_topo *top = &mytopo;
  char *search;
  if (top->sig_topo_state != SIG_TOPO_LOADING)
    KOUT(" ");
  chk_node_name(node, __FUNCTION__);
  found_idx = name2index(node);
  if (found_idx)
    KOUT("Duplicate node");
  for (i=1; i<top->nb_nodes_added+1; i++)
    {
    search = index2name(i);
    if (search && (strcmp(search, node) == 0))
      KOUT("Duplicate node");
    }
  summary_node_alloc (&found_idx);
  create_index_2_name (node, found_idx);
  top->nb_nodes += 1;
  top->nb_nodes_added += 1;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/* FUNCTION:           delete_node_topo                                      */
/*---------------------------------------------------------------------------*/
void delete_node_topo(char *node)
{
  int i,j;
  t_topo *top = &mytopo;
  int node_idx = name2index(node);
  if (!node_idx)
    KOUT("Node not found");
  if (!top->nb_nodes)
    KOUT(" ");
  for (j=0; j<top->max_neighbors; j++)
    {
    if (top->summary.links_tab[node_idx][j].name[0])
      {
      memset ((&top->summary.links_tab[node_idx][j]), 0, sizeof(t_links));
      top->nb_links--;
      }
    }
  for (i=1; i<top->max_nodes; i++)
    {
    if (top->summary.nodes_tab[i][0])
      {
      for (j=0; j<top->max_neighbors; j++)
        {
        if (!strcmp(top->summary.links_tab[i][j].name, node))
          {
          memset (&top->summary.links_tab[i][j], 0, sizeof(t_links));
          top->nb_links--;
          }
        }
      }
    }
    summary_node_free (node_idx);
    delete_index_2_name(node);
    top->nb_nodes -= 1;
    top->nb_nodes_del += 1;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/* FUNCTION:           create_link_topo                                      */
/*---------------------------------------------------------------------------*/
void create_link_topo(char *start, char *end, unsigned long dist)
{
  int i, start_idx, end_idx;
  t_topo *top = &mytopo;
  int done = 0;
  if (top->sig_topo_state != SIG_TOPO_LOADING)
    KOUT(" ");
  chk_node_name(start, __FUNCTION__);
  chk_node_name(end, __FUNCTION__);
  if (!strcmp(start, end))
    KOUT(" ");
  start_idx = name2index(start);
  end_idx = name2index(end);
  if (!(start_idx)||(!end_idx)||(start_idx==end_idx))
    KOUT(" ");
  for (i=0; i<top->max_neighbors; i++)
    if (!strcmp(top->summary.links_tab[start_idx][i].name, end))
      KOUT("%s", end);
  for (i=0; !done && (i<top->max_neighbors); i++)
    {
    if (!top->summary.links_tab[start_idx][i].name[0])
      {
      strcpy (top->summary.links_tab[start_idx][i].name, end);
      top->summary.links_tab[start_idx][i].dist = dist;
      top->summary.links_tab[start_idx][i].idx_name = end_idx;
      top->nb_links++;
      done = 1;
      }
    }
  if (!done)
    KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/* FUNCTION:            modif_link_topo                                      */
/*---------------------------------------------------------------------------*/
void modif_link_topo(char *start, char *end, unsigned long dist)
{
  int i, link_idx=-1, idx_start, idx_end;
  t_topo *top = &mytopo;
  idx_start = name2index(start);
  idx_end = name2index(end);
  if ((!idx_start)||(!idx_end)||(idx_start == idx_end))
    KOUT("Bad node: %s or %s\n", start, end);
  for (i=0; i<top->max_neighbors; i++)
    {
    if (!strcmp(top->summary.links_tab[idx_start][i].name, end))
      {
      link_idx = i;
      break;
      }
    }
  if (link_idx == -1)
    KOUT("Unknown link: %s %s\n", start, end);
  top->summary.links_tab[idx_start][link_idx].dist = dist;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/* FUNCTION:                  node_exists                                    */
/*---------------------------------------------------------------------------*/
int node_exists(char *target)
{
  int result = name2index(target);
  return result;
}
/*---------------------------------------------------------------------------*/





