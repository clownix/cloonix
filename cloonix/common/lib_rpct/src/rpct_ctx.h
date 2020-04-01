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


/****************************************************************************/
typedef struct t_sub_llid
{
  int llid;
  int tid;
  int flags_hop;
  struct t_sub_llid *prev;
  struct t_sub_llid *next;
} t_sub_llid;

typedef struct t_report_sub
{
  int llid;
  struct t_report_sub *prev;
  struct t_report_sub *next;
} t_report_sub;
/*--------------------------------------------------------------------------*/
typedef struct t_rpct_ctx
{
  char name[MAX_NAME_LEN];
  int g_pid;
  int g_start_off_second;
  char *g_buf_tx;
  t_sub_llid *head_sub_llid;
  t_report_sub *g_head_sub;
  char   g_bound_list[bnd_rpct_max][MAX_CLOWNIX_BOUND_LEN];
  t_rpct_tx g_string_tx;
}t_rpct_ctx;
/*---------------------------------------------------------------------------*/
char *get_buf_tx(void *ptr);
t_rpct_ctx *get_rpct_ctx(void *ptr);
void report_heartbeat(void *ptr);
/*---------------------------------------------------------------------------*/
