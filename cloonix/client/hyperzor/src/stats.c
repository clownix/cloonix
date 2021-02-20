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
#include <string.h>
#include <gtk/gtk.h>
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "header_sock.h"
#include "stats.h"

#define MAX_NUM_LEN 16

enum{
  type_item_vm=1,
  type_item_sat,
};

typedef struct t_stats_sysdf
{
  char time_ms[MAX_NUM_LEN];
  char uptime[MAX_NUM_LEN];
  char load1[MAX_NUM_LEN];
  char load5[MAX_NUM_LEN];
  char load15[MAX_NUM_LEN];
  char totalram[MAX_NUM_LEN];
  char freeram[MAX_NUM_LEN];
  char cachedram[MAX_NUM_LEN];
  char sharedram[MAX_NUM_LEN];
  char bufferram[MAX_NUM_LEN];
  char totalswap[MAX_NUM_LEN];
  char freeswap[MAX_NUM_LEN];
  char procs[MAX_NUM_LEN];
  char totalhigh[MAX_NUM_LEN];
  char freehigh[MAX_NUM_LEN];
  char mem_unit[MAX_NUM_LEN];
  char process_utime[MAX_NUM_LEN];
  char process_stime[MAX_NUM_LEN];
  char process_cutime[MAX_NUM_LEN];
  char process_cstime[MAX_NUM_LEN];
  char process_rss[MAX_NUM_LEN];
  char df[MAX_RPC_MSG_LEN];
} t_stats_sysdf;


typedef struct t_stats_count_chain
{
  t_stats_counts counts;
  struct t_stats_count_chain *prev;
  struct t_stats_count_chain *next;
} t_stats_count_chain;

typedef struct t_stats_item
{
  char name[MAX_NAME_LEN];
  int type_item;
  int nb_stats_count_chain;
  int *nb_stats_count_in_chain;
  t_stats_count_chain **stats_count_chain_head;
  GtkWidget **grid_vals;
  struct t_stats_item *prev;
  struct t_stats_item *next;
} t_stats_item;
/*--------------------------------------------------------------------------*/
typedef struct t_stats_net
{
  char name[MAX_NAME_LEN];
  t_stats_item *head_item;
  struct t_stats_net *prev;
  struct t_stats_net *next;
} t_stats_net;
/*--------------------------------------------------------------------------*/
static t_stats_net *g_head_net;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_stats_net *find_net_with_name(char *name)
{
  t_stats_net *cur = g_head_net;
  while(cur)
    {
    if (!strcmp(cur->name, name))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_stats_item *find_item_with_name(t_stats_net *net, char *name)
{
  t_stats_item *cur = net->head_item;
  while(cur)
    {
    if (!strcmp(name, cur->name))
      break;
    cur = cur->next;
    }
  return(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void alloc_item(t_stats_net *net, char *name)
{
  t_stats_item *cur;
  cur = (t_stats_item *) clownix_malloc(sizeof(t_stats_item), 5);
  memset(cur, 0, sizeof(t_stats_item));
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  cur->next = net->head_item;
  if (net->head_item)
    net->head_item->prev = cur;
  net->head_item = cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_item(t_stats_net *net, t_stats_item *item)
{
  int i;
  if (item->grid_vals)
    {
    for (i=0; i<sysdf_max; i++)
      g_free(item->grid_vals[i]);
    clownix_free(item->grid_vals, __FUNCTION__);
    item->grid_vals = NULL;
    }
  if (item->prev)
    item->prev->next = item->next;
  if (item->next)
    item->next->prev = item->prev;
  if (item == net->head_item)
    net->head_item = item->next;
  clownix_free(item, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void alloc_net(char *name)
{
  t_stats_net *cur;
  cur = find_net_with_name(name);
  if (cur)
    KOUT("%s", name);
  cur = (t_stats_net *) clownix_malloc(sizeof(t_stats_net), 5);
  memset(cur, 0, sizeof(t_stats_net));
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  cur->next = g_head_net;
  if (g_head_net)
    g_head_net->prev = cur;
  g_head_net = cur;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void free_net(t_stats_net *net)
{
  while(net->head_item)
    free_item(net, net->head_item);
  if (net->prev)
    net->prev->next = net->next;
  if (net->next)
    net->next->prev = net->prev;
  if (net == g_head_net)
    g_head_net = net->prev;
  clownix_free(net, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void stats_endp(char *net_name, char *name, int num,
                          t_stats_counts *stats_counts)
{
  t_stats_net *net = find_net_with_name(net_name);
  t_stats_item *item; 
  if (net)
    {
    item = find_item_with_name(net, name);
    if (!item)
      KERR("%s %s", net_name, name);
    else
      {
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
/*
static void remove_last_sysdf(t_stats_item *item)
{
  t_stats_sysdf_chain *prev, *cur = item->stats_sysdf_chain_head;
  if (!cur)
    KERR(" ");
  else
    {
    while(cur)
      {
      prev = cur;
      cur = cur->next;
      }
    if (!prev)
      KOUT(" ");
    if (prev->next)  
      KOUT(" ");
    if (prev->prev)  
      prev->prev->next = NULL;
    if (prev == item->stats_sysdf_chain_head)
      item->stats_sysdf_chain_head = NULL;
    clownix_free(prev, __FUNCTION__); 
    item->nb_stats_sysdf_in_chain -= 1;
    }
}
*/
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void update_sysdf(t_stats_sysdf *sys, GtkWidget **gl)
{
  gtk_label_set_text(GTK_LABEL(gl[sysdf_time_ms]), sys->time_ms);
  gtk_label_set_text(GTK_LABEL(gl[sysdf_uptime]), sys->uptime);
  gtk_label_set_text(GTK_LABEL(gl[sysdf_load1]), sys->load1);
  gtk_label_set_text(GTK_LABEL(gl[sysdf_load5]), sys->load5);
  gtk_label_set_text(GTK_LABEL(gl[sysdf_load15]), sys->load15);
  gtk_label_set_text(GTK_LABEL(gl[sysdf_totalram]), sys->totalram);
  gtk_label_set_text(GTK_LABEL(gl[sysdf_freeram]), sys->freeram);
  gtk_label_set_text(GTK_LABEL(gl[sysdf_cachedram]), sys->cachedram);
  gtk_label_set_text(GTK_LABEL(gl[sysdf_sharedram]), sys->sharedram);
  gtk_label_set_text(GTK_LABEL(gl[sysdf_bufferram]), sys->bufferram);
  gtk_label_set_text(GTK_LABEL(gl[sysdf_totalswap]), sys->totalswap);
  gtk_label_set_text(GTK_LABEL(gl[sysdf_freeswap]), sys->freeswap);
  gtk_label_set_text(GTK_LABEL(gl[sysdf_procs]), sys->procs);
  gtk_label_set_text(GTK_LABEL(gl[sysdf_totalhigh]), sys->totalhigh);
  gtk_label_set_text(GTK_LABEL(gl[sysdf_freehigh]), sys->freehigh);
  gtk_label_set_text(GTK_LABEL(gl[sysdf_mem_unit]), sys->mem_unit);
  gtk_label_set_text(GTK_LABEL(gl[sysdf_process_utime]),sys->process_utime);
  gtk_label_set_text(GTK_LABEL(gl[sysdf_process_stime]),sys->process_stime);
  gtk_label_set_text(GTK_LABEL(gl[sysdf_process_cutime]),sys->process_cutime);
  gtk_label_set_text(GTK_LABEL(gl[sysdf_process_cstime]),sys->process_cstime);
  gtk_label_set_text(GTK_LABEL(gl[sysdf_process_rss]), sys->process_rss);
  gtk_label_set_text(GTK_LABEL(gl[sysdf_df]), sys->df);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void fill_fields(t_stats_sysinfo *si, char *df, t_stats_sysdf *sysdf)
{
  snprintf(sysdf->time_ms,        MAX_NUM_LEN-1, "%d", si->time_ms);
  snprintf(sysdf->uptime,         MAX_NUM_LEN-1, "%lu", si->uptime);
  snprintf(sysdf->load1,          MAX_NUM_LEN-1, "%lu", si->load1);
  snprintf(sysdf->load5,          MAX_NUM_LEN-1, "%lu", si->load5);
  snprintf(sysdf->load15,         MAX_NUM_LEN-1, "%lu", si->load15);
  snprintf(sysdf->totalram,       MAX_NUM_LEN-1, "%lu", si->totalram);
  snprintf(sysdf->freeram,        MAX_NUM_LEN-1, "%lu", si->freeram);
  snprintf(sysdf->cachedram,      MAX_NUM_LEN-1, "%lu", si->cachedram);
  snprintf(sysdf->sharedram,      MAX_NUM_LEN-1, "%lu", si->sharedram);
  snprintf(sysdf->bufferram,      MAX_NUM_LEN-1, "%lu", si->bufferram);
  snprintf(sysdf->totalswap,      MAX_NUM_LEN-1, "%lu", si->totalswap);
  snprintf(sysdf->freeswap,       MAX_NUM_LEN-1, "%lu", si->freeswap);
  snprintf(sysdf->procs,          MAX_NUM_LEN-1, "%lu", si->procs);
  snprintf(sysdf->totalhigh,      MAX_NUM_LEN-1, "%lu", si->totalhigh);
  snprintf(sysdf->freehigh,       MAX_NUM_LEN-1, "%lu", si->freehigh);
  snprintf(sysdf->mem_unit,       MAX_NUM_LEN-1, "%lu", si->mem_unit);
  snprintf(sysdf->process_utime,  MAX_NUM_LEN-1, "%lu", si->process_utime);
  snprintf(sysdf->process_stime,  MAX_NUM_LEN-1, "%lu", si->process_stime);
  snprintf(sysdf->process_cutime, MAX_NUM_LEN-1, "%lu", si->process_cutime);
  snprintf(sysdf->process_cstime, MAX_NUM_LEN-1, "%lu", si->process_cstime);
  snprintf(sysdf->process_rss,    MAX_NUM_LEN-1, "%lu", si->process_rss);
  snprintf(sysdf->df, MAX_RPC_MSG_LEN-1, "%s", df);


}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
/*
static void insert_sysdf(t_stats_item *item, t_stats_sysinfo *si, char *df)
{
  t_stats_sysdf_chain *cur;
  cur =(t_stats_sysdf_chain *)clownix_malloc(sizeof(t_stats_sysdf_chain), 5);
  memset(cur, 0, sizeof(t_stats_sysdf_chain));
  fill_fields(si, df, &(cur->sysdf)); 
  if (item->stats_sysdf_chain_head)
    item->stats_sysdf_chain_head->prev = cur;
  cur->next = item->stats_sysdf_chain_head;
  item->stats_sysdf_chain_head = cur;
}
*/
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void stats_sysinfo(char *net_name, char *name, t_stats_sysinfo *si, char *df)
{
  t_stats_sysdf sysdf;
  t_stats_net *net = find_net_with_name(net_name);
  t_stats_item *item;
  if (net)
    {
    item = find_item_with_name(net, name);
    if (!item)
      KERR("%s %s", net_name, name);
    else
      {
      if (item->grid_vals)
        {
        fill_fields(si, df, &sysdf);
        update_sysdf(&sysdf, item->grid_vals);
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void stats_free_grid_var(char *net_name, char *name)
{
  t_stats_net *net = find_net_with_name(net_name);
  t_stats_item *item;
  int i;
  if (!net)
    KERR("%s %s", net_name, name);
  else
    {
    item = find_item_with_name(net, name);
    if (item)
      {
      if (item->grid_vals == NULL)
        KERR("%s %s", net_name, name);
      else
        {
        for (i=0; i<sysdf_max; i++)
          g_free(item->grid_vals[i]);
        clownix_free(item->grid_vals, __FUNCTION__);
        item->grid_vals = NULL;
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
GtkWidget **stats_alloc_grid_var(char *net_name, char *name)
{
  int i;
  GtkWidget **result = NULL;
  t_stats_net *net = find_net_with_name(net_name);
  t_stats_item *item;
  if (!net)
    KERR("%s %s", net_name, name);
  else
    {
    item = find_item_with_name(net, name);
    if (!item)
      KERR("%s %s", net_name, name);
    else
      {
      if (item->grid_vals == NULL)
        {
        item->grid_vals =
        (GtkWidget **)clownix_malloc(sysdf_max*sizeof(GtkWidget *), 7);
        memset(item->grid_vals, 0, sysdf_max*sizeof(GtkWidget *));
        for (i=0; i<sysdf_max; i++)
          item->grid_vals[i] = gtk_label_new(NULL);
        result = item->grid_vals;
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void stats_net_alloc(char *net_name)
{
  t_stats_net *net = find_net_with_name(net_name);
  if (net)
    KERR("%s", net_name);
  else
    alloc_net(net_name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void stats_net_free(char *net_name)
{
  t_stats_net *net = find_net_with_name(net_name);
  if (net)
    free_net(net);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void stats_item_alloc(char *net_name, char *name)
{
  t_stats_net *net = find_net_with_name(net_name);
  t_stats_item *item; 
  if (!net)
    KERR("%s", net_name);
  else
    {
    item = find_item_with_name(net, name);
    if (item)
      KERR("%s %s", net_name, name);
    else
      {
      alloc_item(net, name);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void stats_item_free(char *net_name, char *name)
{
  t_stats_net *net = find_net_with_name(net_name);
  t_stats_item *item; 
  if (net)
    {
    item = find_item_with_name(net, name);
    if (!item)
      KERR("%s %s", net_name, name);
    else
      {
      free_item(net, item);
      }
    }
}
/*--------------------------------------------------------------------------*/
