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
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_daemon.h"
#include "hop_event.h"
#include "cfg_store.h"
#include "event_subscriber.h"
#include "cnt.h"
#include "utils_cmd_line_maker.h"
#include "podman_images.h"


static t_image_doc *g_image_head0;
static t_image_doc *g_image_head1;
static int g_image_current;

/*****************************************************************************/
static t_image_doc *get_master_image_doc_head(void)
{
  t_image_doc *result;
  if (g_image_current == 0)
    result = g_image_head0;
  else if (g_image_current == 1)
    result = g_image_head1;
  else
    KOUT("ERROR %d", g_image_current);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_image_doc *get_slave_image_doc_head(void)
{
  t_image_doc *result;

  if (g_image_current == 0)
    result = g_image_head1;
  else if (g_image_current == 1)
    result = g_image_head0;
  else
    KOUT("ERROR %d", g_image_current);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void set_slave_image_doc_head(t_image_doc *newhead)
{
  if (g_image_current == 0)
    g_image_head1 = newhead;
  else if (g_image_current == 1)
    g_image_head0 = newhead;
  else
    KOUT("ERROR %d", g_image_current);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void add_slave_image_doc(char *brandtype, char *image_name)
{
  t_image_doc *cur;
  if (strcmp(brandtype, "podman"))
    KERR("ERROR: %s", brandtype);
  cur = (t_image_doc *) malloc(sizeof(t_image_doc)); 
  memset(cur, 0, sizeof(t_image_doc));
  strncpy(cur->brandtype, brandtype, MAX_NAME_LEN-1);
  strncpy(cur->image_name, image_name, MAX_NAME_LEN-1);
  cur->next = get_slave_image_doc_head();
  set_slave_image_doc_head(cur);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void clean_slave_image_chain(void)
{
  t_image_doc *next, *cur = get_slave_image_doc_head();
  while(cur)
    {
    next = cur->next;
    free(cur);
    cur = next;
    }
  set_slave_image_doc_head(NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void swap_slave_master_image(void)
{
  if (g_image_current == 0)
    g_image_current = 1;
  else if (g_image_current == 1)
    g_image_current = 0;
  else
    KOUT("ERROR %d", g_image_current);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int podman_images_exists(char *brandtype, char *image_name)
{
  int result = 0;
  t_image_doc *cur;
  if (strcmp(brandtype, "podman"))
    KERR("ERROR: %s", brandtype);
  cur = get_master_image_doc_head();
  while(cur)
    {
    if ((!strcmp(brandtype, cur->brandtype)) &&
        (!strcmp(image_name, cur->image_name)))
      break;
    cur = cur->next;
    }
  if (cur)
    result = 1;
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int count_brandtype_nb(char *brandtype)
{
  int result = 0;
  t_image_doc *cur = get_master_image_doc_head();
  while(cur)
    {
    if (!strcmp(brandtype, cur->brandtype))
      result += 1;
    cur = cur->next;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int podman_image_get_list(char *brandtype, t_slowperiodic **slowperiodic)
{
  int i, nb, len, result = 0;
  t_image_doc *cur;
  if ((brandtype==NULL) || (strlen(brandtype)==0))
    KOUT("ERROR");
  nb = count_brandtype_nb(brandtype);
  if (nb)
    {
    len = nb * sizeof(t_slowperiodic);
    *slowperiodic=(t_slowperiodic *)clownix_malloc(len, 5);
    memset(*slowperiodic, 0, len);
    cur = get_master_image_doc_head();
    while(cur)
      {
      if (!strcmp(brandtype, cur->brandtype))
        {
        if (result >= nb)
          KOUT("ERROR %d %d", result, nb);
        strncpy((*slowperiodic)[result++].name, cur->image_name, MAX_NAME_LEN-1);
        }
      cur = cur->next;
      }
    }
  for (i=0; i<result; i++)
    {
    if (strlen((*slowperiodic)[i].name) == 0)
      KOUT("ERROR %d %d", i, nb);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void podman_images_init(void)
{
  g_image_head0 = NULL;
  g_image_head1 = NULL;
  g_image_current = 0;
}
/*--------------------------------------------------------------------------*/

