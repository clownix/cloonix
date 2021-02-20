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
#include <cairo.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <string.h>

#include "io_clownix.h"
#include "bdplot.h"
#include "gtkplot.h"



typedef struct t_bdplot
{
  char name[MAX_NAME_LEN];
  int num;
  int tx;
  int rx;
  int last_date_ms;
  void *gtk_plot_ctx;
  struct t_bdplot *prev;
  struct t_bdplot *next;
} t_bdplot;

static t_bdplot *g_bdplot_head;

/****************************************************************************/
static t_bdplot *bdplot_find(char *name, int num)
{
  t_bdplot *cur = g_bdplot_head;
  while(cur)
    {
    if ((!strcmp(cur->name, name)) && (cur->num == num))
      break;
    cur = cur->next;
    }
  return cur;  
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void bdplot_alloc(char *name, int num, void *gtk_plot_ctx)
{
  t_bdplot *cur = (t_bdplot *) malloc(sizeof(t_bdplot));
  memset(cur, 0, sizeof(t_bdplot));
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  cur->num = num; 
  cur->gtk_plot_ctx = gtk_plot_ctx; 
  if (g_bdplot_head)
    g_bdplot_head->prev = cur;
  cur->next = g_bdplot_head;
  g_bdplot_head = cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void bdplot_free(t_bdplot *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (g_bdplot_head == cur)
    g_bdplot_head = cur->next;
  free(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void bdplot_newdata(char *name, int num, int date_ms, int tx, int rx)
{
  float date_s;
  float bd[NCURVES];
  int delta;
  t_bdplot *cur = bdplot_find(name, num);
  if(cur)
    {
    cur->tx += tx;
    cur->rx += rx;
    delta = (date_ms - cur->last_date_ms);
    if (delta > 200)
      {
      date_s = (float)date_ms/1000;
      bd[1]  = (float) cur->tx;
      bd[0]  = (float) cur->rx;
      gtkplot_newdata(cur->gtk_plot_ctx, date_s, bd);
      cur->last_date_ms = date_ms;
      cur->tx = 0; 
      cur->rx = 0; 
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void bdplot_create(char *name, int num)
{
  void *gtk_plot_ctx;
  t_bdplot *cur = bdplot_find(name, num);
  if(cur)
    KERR("%s %d already exists", name, num);
  else
    {
    gtk_plot_ctx = gtkplot_alloc(name, num);
    bdplot_alloc(name, num, gtk_plot_ctx);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void bdplot_destroy(char *name, int num)
{
  t_bdplot *cur = bdplot_find(name, num);
  if(!cur)
    KERR("%s %d not found", name, num);
  else
    {
    bdplot_free(cur);
    gtkplot_free(cur->gtk_plot_ctx); 
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void bdplot_init(void)
{
  g_bdplot_head = NULL;
}
/*--------------------------------------------------------------------------*/


