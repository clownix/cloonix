/*****************************************************************************/
/*    Copyright (C) 2006-2025 clownix@clownix.net License AGPL-3             */
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
#include "bijection.h"

#define TOPO_MAX_NAME_LEN 4

#define BASIC_MASK 0x1FFF
#define BIJ_MAX_NAME_LEN (MAX_NAME_LEN+2+2)

/*---------------------------------------------------------------------------*/
typedef struct t_bij_elem
{
  char name[BIJ_MAX_NAME_LEN];
  int idx;
  int nb_users;
} t_bij_elem;
/*---------------------------------------------------------------------------*/
typedef struct t_bij
{
  int total_elems;
  t_bij_elem *tab_elems[BASIC_MASK+1];
} t_bij;
/*---------------------------------------------------------------------------*/

static t_bij glob_bij;

/*****************************************************************************/
/*                           get_hash_of_id                                  */
/*---------------------------------------------------------------------------*/
static int get_hash_of_id(int len, void *ident)
{
  int i, result;
  short *short_ident = (short *) ident;
  char *char_ident;
  short last;
  short hash = 0;
  if (len < 1)
    KOUT(" ");
  else if (len == 1)
    {
    char_ident = (char *) ident;
    result = (*char_ident & BASIC_MASK);
    }
  else
    {
    for (i=0; i<(len/2); i++)
      hash ^= short_ident[i];
    if (len%2)
      {
      char_ident = (char *) ident;
      last = (short) (char_ident[len - 1] & BASIC_MASK);
      hash ^= last;
      }
    result = (hash & BASIC_MASK);
    }
  if (result == 0)
    result = 1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/*                         find_elem_with_name                          */
/*---------------------------------------------------------------------------*/
static int find_elem_with_name(char *name)
{
  t_bij *bij = &glob_bij;
  int result = 0;
  int i, index, idx = get_hash_of_id(strlen(name), (void *)name);
  for (i = idx; i < idx + BASIC_MASK; i++)
    {
    index = i;
    if (index >= BASIC_MASK)
      {
      index -= BASIC_MASK;
      if (index == 0)
        index = 1;
      }
    if ((bij->tab_elems[index]) &&
        (!strcmp(bij->tab_elems[index]->name, name)))
      {
      result = index;  
      break;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
/*                         find_free_idx_for_name                            */
/*---------------------------------------------------------------------------*/
static int find_free_idx_for_name(char *name)
{
  t_bij *bij = &glob_bij;
  int count = 0;
  int idx = get_hash_of_id(strlen(name), (void *)name);
  while(bij->tab_elems[idx])
    {
    count++;
    if (count == BASIC_MASK)
      {
      idx = 0;
      break;
      }
    idx++;
    if (idx == BASIC_MASK)
      idx = 1;
    }
  return idx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/*                                 bij_add                                   */
/*---------------------------------------------------------------------------*/
static char *bij_add (char *name)
{
  t_bij *bij = &glob_bij;
  int idx, idx_bis;
  int result = -1;
  t_bij_elem *target;
  static char tag[MAX_NAME_LEN];
  if ((strlen(name) < BIJ_MAX_NAME_LEN) &&
      (bij->total_elems < BASIC_MASK/2))
    {
    idx = find_elem_with_name(name);
    if (!idx)
      {
      idx = find_free_idx_for_name(name);
      if (idx)
        {
        target = (t_bij_elem *) clownix_malloc(sizeof(t_bij_elem), 13);
        memset(target, 0, sizeof(t_bij_elem));
        target->idx = idx;
        strcpy(target->name, name); 
        bij->tab_elems[idx] = target;
        bij->total_elems++;
        result = 0;
        target->nb_users += 1;
        }
      }
    else
      {
      target = bij->tab_elems[idx];
      result = 0;
      target->nb_users += 1;
      }
    }
  if (result != 0)
    KOUT(" ");
  idx_bis = find_elem_with_name(name);
  if (idx != idx_bis)
    KOUT(" ");
  sprintf(tag, "%04X", idx);
  if (strlen(tag) != TOPO_MAX_NAME_LEN)
    KOUT(" ");
  return tag;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/*                             bij_del                                       */
/*---------------------------------------------------------------------------*/
static int bij_del (char *name)
{
  t_bij *bij = &glob_bij;
  int idx;
  int result = -1;
  if ((strlen(name) < BIJ_MAX_NAME_LEN) &&
      (bij->total_elems > 0))
    {
    idx = find_elem_with_name(name);
    if (idx)
      {
      if ((bij->tab_elems[idx]->nb_users > 0) &&
          (bij->total_elems > 0))
        {
        bij->tab_elems[idx]->nb_users -= 1;
        if (bij->tab_elems[idx]->nb_users == 0)
          {
          result = 0;
          clownix_free(bij->tab_elems[idx], __FUNCTION__);
          bij->tab_elems[idx] = NULL;
          bij->total_elems--;
          }
        }
      }
    }
  if (result)
    KOUT(" ");
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static char *bij_get_name_with_tag(char *tag)
{
  t_bij *bij = &glob_bij;
  int num;
  char *result = NULL;
  if (strlen(tag) != TOPO_MAX_NAME_LEN)
    KOUT(" ");
  if (sscanf(tag, "%04X", &num) != 1)
    KOUT(" ");
  if (bij->tab_elems[num])
    result = bij->tab_elems[num]->name;
  if (!result)
    KOUT(" ");
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *bij_alloc_tag(int bank_type, char *name, int num)
{
  char bij_name[BIJ_MAX_NAME_LEN];
  sprintf(bij_name, "%02X.%02X.%s", bank_type, num, name);
  return(bij_add (bij_name));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void bij_free_tag(char *tag)
{
  char *bij_name = bij_get_name_with_tag(tag);
  bij_del(bij_name);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void bij_get(char *tag, int *bank_type, char **name, int *num)
{
  char *bij_name = bij_get_name_with_tag(tag);
  static char nm[MAX_NAME_LEN];
  if (sscanf(bij_name, "%02X.%02X.%s", bank_type, num, nm) != 3)
    KOUT(" ");
  *name = nm; 
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void bij_init(void)
{
  memset(&glob_bij, 0, sizeof(t_bij));
}
/*---------------------------------------------------------------------------*/

