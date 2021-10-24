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
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <errno.h>
#include "io_clownix.h"
#include "channel.h"
#include "msg_layer.h"
#include "chunk.h"

/*****************************************************************************/
static void alloc_boundary(t_data_channel *dchan, char *boundary,
                           t_data_chunk *chunk_bound, int i_bound)

{
  dchan->boundary = (char *)clownix_malloc(strlen(boundary) + 1, IOCMLC);
  strcpy(dchan->boundary, boundary);
  dchan->rx_bound_start = chunk_bound;
  dchan->i_bound_start  = i_bound;
  dchan->decoding_state = rx_type_open_bound_found;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void free_boundary(t_data_channel *dchan)
{
  clownix_free (dchan->boundary, __FUNCTION__);
  dchan->boundary = NULL;
  dchan->rx_bound_start = NULL;
  dchan->i_bound_start = 0;
  dchan->decoding_state = rx_type_ascii_start;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static char *find_pattern(char *msg, int len, char *pat, int patlen)
{
  int i;
  char *ptr = msg, *ptr_found = msg, *m, *p;
  int real_patlen = strlen(pat);
  if (patlen > len)
    ptr_found = NULL;
  while (ptr_found)
    {
    ptr_found = (char *)memchr(ptr, pat[0], len - (ptr - msg) - patlen + 1 );
    if (ptr_found)
      {
      ptr = ptr_found+1;
      m = ptr_found;
      p = pat;
      for (i=0, m = ptr_found, p = pat; (i < real_patlen) ; m++, p++, i++)
        {
        if (*m != *p)
          {
          break;
          }
        }
      if (i == real_patlen)
        break;
      }
    }
  return ptr_found; 
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int get_close_boundary_from_chunk(t_data_chunk *first,
                                         char *boundary, 
                                         t_data_chunk **hit_a_problem)
{
  int i,keep_on = 1, result = 0;
  int boundlen, nb_done;  
  int state = 0;
  int total_byte_count = 0;
  t_data_chunk *cur = first;
  *hit_a_problem = NULL;
  if (cur)
    {
    if (!cur->len)
      KOUT(" ");
    if (boundary == NULL)
      KOUT(" ");
    boundlen = strlen(boundary);
    if (boundlen < 4) 
      KOUT(" ");
    while (keep_on && cur)
      {
      if (cur->len_done > cur->len)
        KOUT(" ");
      for (i=cur->len_done; i<cur->len; i++)
        {
        if (cur->chunk[i] == 0)  
          {
          keep_on = 0;
          cur->start_len_done = i;
          cur->len_done = i+1;
          *hit_a_problem = cur;
          KERR("%s %d %d %s", cur->chunk, cur->len, 
                              cur->len_done, &(cur->chunk[i+1]));
          break;
          }
        if (state == 0)
          {
          if (cur->chunk[i] != boundary[0])  
            {
            cur->len_done += 1;
            continue;
            }
          nb_done = 1;
          state = 1;
          }
        else if (state == 1)
          {
          if (cur->chunk[i] == boundary[nb_done++])
            { 
            if (nb_done == boundlen)
              {
              keep_on = 0;
              result = 1;
              break;
              }
            }
          else
            {
            if (cur->chunk[i] == boundary[0])
              {
              nb_done = 1;
              state = 1;
              }
            else
              {
              cur->len_done += 1;
              push_done_limit(first, cur);
              nb_done = 0;
              state = 0;
              }
            }
          }
        else
          KOUT(" ");
        }
      total_byte_count += cur->len_done;
      if (total_byte_count >= MAX_SIZE_BIGGEST_MSG)
        {
        KERR(" ");
        keep_on = 0;
        *hit_a_problem = cur;
        break;
        }
      cur = cur->next;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static char *look_for_boundary(char *msg, int len, char *pat)
{
  char *ptr_end;
  int patlen = strlen(pat);
  ptr_end = find_pattern(msg, len, pat, patlen);
  if (ptr_end)
    ptr_end += patlen;
  else
    KOUT("%s %s %d %d", msg, pat, len, patlen);
  return (ptr_end);
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
static t_data_chunk *detect_xml_start_in_chunk(t_data_chunk *first)
{
  int i, j, keep_on = 1;
  t_data_chunk *cur = first;
  t_data_chunk *result = NULL;
  if (!cur)
    KOUT(" ");
  while (keep_on && cur)
    {
    for (i=0; i<cur->len; i++) 
      {
      if (cur->chunk[i] == '<')
        {
        for (j=0; j<i; j++) 
          {
          if (cur->chunk[j] == 0)
            cur->chunk[j] = ' ';
          }
        result = cur;
        keep_on = 0;
        break;
        }
      }
    cur = cur->next;
    }
  return result;
}
/*---------------------------------------------------------------------------*/





/*****************************************************************************/
static int is_good_first_letter_xml(char c)
{
  int result = 0;
  if (((c>='a')&&(c<='z')) ||
      ((c>='A')&&(c<='Z')) ||
       (c==':') ||
       (c=='_'))
    result = 1;
  return result;
}
/*---------------------------------------------------------------------------*/
/*****************************************************************************/
static int is_good_next_letter_xml(char c)
{
  int result = is_good_first_letter_xml(c);
  if (!result)
    if (((c >= '0') && (c <= '9')) ||
        (c == '-') || (c == '.'))
      result = 1;
  return result;
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
static char *get_xml_start_boundary_from_chunk(t_data_chunk *first, 
                                               t_data_chunk **chunk_bound, 
                                               int *i_bound)
{
  char *xml_boundary = NULL;
  int nb_char_in_close_bound, nb_char_in_bound = 0;
  int i, i_start, found = 0, found2;
  int keep_on = 1;
  t_data_chunk *cur = first;
  if (!cur)
    KOUT(" ");
  while (cur)
    {
    for (i=cur->start_len_done; i<cur->len; i++)
      {
      if (cur->chunk[i] == 0)  
        {
        found = 0;
        }
      if (found == 2)
        {
        if (is_good_next_letter_xml(cur->chunk[i]))
          nb_char_in_bound += 1;
        else if (cur->chunk[i] == '>')
          {
          nb_char_in_bound += 1;
          found = 4;
          keep_on = 0;
          break;
          }
        else if (cur->chunk[i] == '<')
          {
          nb_char_in_bound = 1;
          found = 1;
          *chunk_bound = cur;
          *i_bound = i;
          }
        else
          found = 0;
        }
      else if (found == 1)
        {
        if (is_good_first_letter_xml(cur->chunk[i]))
          {
          nb_char_in_bound += 1;
          found = 2;
          }
        else if (cur->chunk[i] == '<')
          {
          nb_char_in_bound = 1;
          found = 1;
          *chunk_bound = cur;
          *i_bound = i;
          }
        else
          found = 0;
        }
      else if (cur->chunk[i] == '<')
        {
        nb_char_in_bound = 1;
        found = 1;
        *chunk_bound = cur;
        *i_bound = i;
        }
      }
    if (!keep_on)
      break;
    cur = cur->next;
    }

  if (found == 4)
    {
    nb_char_in_close_bound = nb_char_in_bound + 1;
    xml_boundary = (char *)clownix_malloc(nb_char_in_close_bound + 2, IOCMLC);
    memset(xml_boundary, 0, nb_char_in_close_bound + 2);
    xml_boundary[0] = '<';
    xml_boundary[1] = '/';
    cur = *chunk_bound;
    i_start = *i_bound;
    found2 = 0;
    keep_on = 1;
    if (nb_char_in_bound < 3)
      KOUT(" %s %d %c", cur->chunk, i_start, cur->chunk[i_start]);
    nb_char_in_bound = 2;
    while (keep_on)
      {
      if (!cur)
        KOUT(" ");
      for (i=i_start; i<cur->len; i++)
        {
        if (found2)
          {
          xml_boundary[nb_char_in_bound++] = cur->chunk[i];
          if (nb_char_in_bound == nb_char_in_close_bound)
            {
            keep_on = 0;
            break;
            }
          }
        else if (cur->chunk[i] == '<') 
          found2 = 1;
        else
          KOUT(" %s %d %c", cur->chunk, i, cur->chunk[i]);
        }
      i_start = 0;
      cur = cur->next;
      }
    }
  return (xml_boundary);
}
/*---------------------------------------------------------------------------*/
           
/*****************************************************************************/
/*                 chunks_state_rx_type_ascii_start                          */
/*---------------------------------------------------------------------------*/
static void chunks_state_rx_type_ascii_start(t_data_channel *dchan)
{
  t_data_chunk *chunk_of_start;
  t_data_chunk *chunk_bound;
  int i_bound;
  char *boundary;
  if (dchan->boundary)
    KOUT(" ");
  if (dchan->rx && dchan->rx->len)
    {
    chunk_of_start = detect_xml_start_in_chunk(dchan->rx);
    if (chunk_of_start)
      {
      chain_del(&(dchan->rx), chunk_of_start);
      boundary = get_xml_start_boundary_from_chunk(dchan->rx, 
                                                   &chunk_bound, &i_bound);
      if (boundary)
        {
        chain_del(&(dchan->rx), chunk_bound);
        alloc_boundary(dchan, boundary, chunk_bound, i_bound);
        clownix_free(boundary, __FUNCTION__);
        }
      }
    else
      {
      chain_del(&(dchan->rx), NULL);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int chunks_state_rx_type_close_bound_found(t_data_channel *dchan,
                                                  char **found, int *found_len)
{
  int tot_len, len_what_is_left, res, result = 0;
  t_data_chunk *hit_a_problem;
  char *end_found, *tot_rx, *what_is_left;
  if (!dchan->boundary)
    KOUT(" ");
  res = get_close_boundary_from_chunk(dchan->rx,dchan->boundary,&hit_a_problem);
  if (res == 1)
    {
    if (!dchan->rx_bound_start)
      KOUT(" ");
    if (dchan->rx != dchan->rx_bound_start)
      KOUT(" ");
    tot_len = make_a_buf_copy(dchan->rx, dchan->i_bound_start, &tot_rx);
    end_found = look_for_boundary(tot_rx, tot_len, dchan->boundary);
    if (end_found)
      {
      result = 1;
      chain_del(&(dchan->rx), NULL);
      if (!dchan->boundary)
        KOUT(" ");
      free_boundary(dchan);
      *found = tot_rx;
      *found_len = end_found - tot_rx;
      len_what_is_left = tot_len - *found_len;
      *found_len += 1;
      if (len_what_is_left)
        {
        what_is_left = (char *) clownix_malloc(len_what_is_left+1, IOCMLC);
        what_is_left[len_what_is_left] = 0;
        memcpy(what_is_left, end_found, len_what_is_left);
        chain_append(&(dchan->rx), len_what_is_left, what_is_left);
        end_found[0] = 0;
        }
      }
    else
      clownix_free(tot_rx, __FUNCTION__);
    }
  else if (hit_a_problem)
    {
    chain_del(&(dchan->rx), hit_a_problem);
    if (!dchan->boundary)
      KOUT(" ");
    free_boundary(dchan);
    }
  return result;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void string_rx_to_process(t_data_channel *dchan)
{
  int found_len;
  char *found;
  if (dchan->decoding_state == rx_type_ascii_start)
    chunks_state_rx_type_ascii_start(dchan);
  while(dchan->decoding_state == rx_type_open_bound_found)
    {
    if (chunks_state_rx_type_close_bound_found(dchan, &found, &found_len))
      {
      dchan->rx_callback(dchan->llid, found_len, found);
      clownix_free(found, __FUNCTION__);
      if (dchan->llid)
        chunks_state_rx_type_ascii_start(dchan);
      else
        break;
      }
    else if (dchan->decoding_state == rx_type_ascii_start)
      chunks_state_rx_type_ascii_start(dchan);
    else
      break;
    }
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void chunk_chain_delete(t_data_channel *dchan)
{
  chain_del(&(dchan->rx), NULL);
  if (dchan->decoding_state == rx_type_ascii_start)
    {
    if (dchan->boundary)
      KOUT(" ");
    }
  else if (dchan->decoding_state == rx_type_open_bound_found)
    {
    if (!dchan->boundary)
      KOUT(" ");
    free_boundary(dchan);
    }
  else if ((dchan->decoding_state == rx_type_watch)             ||
           (dchan->decoding_state == rx_type_listen)            ||
           (dchan->decoding_state == rx_type_doorways)) 
    {
    }
  else
    KOUT("ERROR %d", dchan->decoding_state);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void new_rx_to_process(t_data_channel *dchan, int len, char *new_rx)
{
  chain_append(&dchan->rx, len, new_rx);
  if ((dchan->decoding_state == rx_type_ascii_start)||
      (dchan->decoding_state == rx_type_open_bound_found))
    string_rx_to_process(dchan);
  else
    KOUT("%d", dchan->decoding_state);
}
/*---------------------------------------------------------------------------*/



