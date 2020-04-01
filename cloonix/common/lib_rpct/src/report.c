/*****************************************************************************/
/*    Copyright (C) 2006-2020 clownix@clownix.net License AGPL-3             */
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
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>


#include "ioc_top.h"
#include "rpct_ctx.h"

/*****************************************************************************/
static t_report_sub *find_report_sub(t_report_sub *head, int llid)
{
  t_report_sub *cur = head;
  while (cur && (cur->llid != llid))
    cur = cur->next;
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void alloc_report_sub(t_report_sub **head, int llid)
{
  t_report_sub *cur = (t_report_sub *)malloc(sizeof(t_report_sub));
  memset(cur, 0, sizeof(t_report_sub));
  cur->llid = llid;
  if (*head)
    (*head)->prev = cur;
  cur->next = *head;
  *head = cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void free_report_sub(t_report_sub **head, t_report_sub *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == *head)
    *head = cur->next;
  free(cur);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_report_sub(void *ptr, int llid, int sub)
{
  t_rpct_ctx *ctx = get_rpct_ctx(ptr);
  t_report_sub *cur = find_report_sub(ctx->g_head_sub, llid);
  if (sub)
    {
    if (!cur)
      alloc_report_sub(&(ctx->g_head_sub), llid);
    }
  else
    {
    if (cur)
      free_report_sub(&(ctx->g_head_sub), cur);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void send_all_reports(void *ptr, int llid)
{
  t_blkd_item *item;
  int *llid_list;
  int i, llid_list_max;
  llid_list = get_llid_blkd_list(ptr);
  llid_list_max = get_llid_blkd_list_max(ptr);
  for (i=0; i < llid_list_max; i++)
    {
    if (llid_list[i])
      {
      item = get_llid_blkd_report_item(ptr, llid_list[i]);
      if ((item) && (strncmp(item->sock, "Listen", strlen("Listen"))))
        {
        if (llid)
          {
          rpct_send_report(ptr, llid, item);
          }
        else
          rpct_recv_report(ptr, llid, item);
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void report_heartbeat(void *ptr)
{
  t_rpct_ctx *ctx = get_rpct_ctx(ptr);
  t_report_sub *cur = ctx->g_head_sub;
  while(cur)
    {
    send_all_reports(ptr, cur->llid);
    cur = cur->next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_clean_all(void *ptr)
{
  t_rpct_ctx *ctx = get_rpct_ctx(ptr);
  t_report_sub *next, *cur = ctx->g_head_sub;
  while (cur)
    {
    next = cur->next;
    free_report_sub(&(ctx->g_head_sub), cur);
    cur = next;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void insert_in_sub(t_sub_llid **head, int llid,
                          int tid, int flags_hop)
{
  t_sub_llid *elem = *head;
  while(elem)
    {
    if (elem->llid == llid)
      break;
    elem = elem->next;
    }
  if (elem)
    {
    elem->tid = tid;
    elem->flags_hop |= flags_hop;
    }
  else
    {
    elem = (t_sub_llid *) malloc(sizeof(t_sub_llid));
    memset(elem, 0, sizeof(t_sub_llid));
    elem->llid = llid;
    elem->tid = tid;
    elem->flags_hop = flags_hop;
    elem->next = *head;
    if (*head)
      (*head)->prev = elem;
    *head = elem;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void extract_from_sub(t_sub_llid **head, int llid)
{
  t_sub_llid *elem = *head;
  while(elem)
    {
    if (elem->llid == llid)
      break;
    elem = elem->next;
    }
  if (elem)
    {
    if (elem->prev)
      elem->prev->next = elem->next;
    if (elem->next)
      elem->next->prev = elem->prev;
    if (*head == elem)
      *head = elem->next;
    free(elem);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void interface_send_evt_txt (void *ptr, int flags_hop, char *line)
{
  t_rpct_ctx *ctx = get_rpct_ctx(ptr);
  t_sub_llid *elem = ctx->head_sub_llid;
  t_sub_llid *next;
  while(elem)
    {
    next = elem->next;
    if (elem->flags_hop & flags_hop)
      {
      rpct_send_hop_msg(ptr, elem->llid, elem->tid, flags_hop, line);
      }
    elem = next;
    }
}
/*-----------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_hop_print_add_sub(void *ptr, int llid, int tid, int flags)
{
  t_rpct_ctx *ctx = get_rpct_ctx(ptr);
  insert_in_sub(&(ctx->head_sub_llid), llid, tid, flags);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_hop_print_del_sub(void *ptr, int llid)
{
  t_rpct_ctx *ctx = get_rpct_ctx(ptr);
  extract_from_sub(&(ctx->head_sub_llid), llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_hop_print (void *ptr, int flags_hop, const char * format, ...)
{
  va_list arguments;
  char line[MAX_HOP_PRINT_LEN];
  int len;

  va_start (arguments, format);
  memset(line, 0, MAX_HOP_PRINT_LEN);
  len = snprintf(line, MAX_HOP_PRINT_LEN-1, "%07u: ", 
                 (unsigned int) cloonix_get_msec());
  vsnprintf (line+len, MAX_HOP_PRINT_LEN-1, format, arguments);
  interface_send_evt_txt (ptr, flags_hop, line);
  va_end (arguments);
}
/*-----------------------------------------------------------------------*/


