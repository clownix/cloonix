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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdarg.h>


#include "ioc_top.h"
/*---------------------------------------------------------------------------*/





typedef struct t_sub_llid
{
  int llid;
  int tid;
  int flags_hop;
  struct t_sub_llid *prev;
  struct t_sub_llid *next;
} t_sub_llid;

static char g_bound_list[bnd_rpct_max][MAX_CLOWNIX_BOUND_LEN];
static t_rpct_tx g_string_tx;
static char *g_buf_tx;
static t_sub_llid *g_head_sub_llid;


/*****************************************************************************/
static char *get_hop_free_txt(char *msg)
{
  int len;
  char *ptrs, *ptre, *txt;
  ptrs = strstr(msg, "<hop_free_txt_joker>");
  if (!ptrs)
    KOUT("%s", msg);
  ptrs += strlen("<hop_free_txt_joker>");
  ptre = strstr(ptrs, "</hop_free_txt_joker>");
  if (!ptre)
    KOUT("%s", msg);
  len = ptre - ptrs;
  txt = (char *) malloc(len+1);
  memcpy(txt, ptrs, len);
  txt[len] = 0;
  return txt;
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
static void interface_send_evt_txt (int flags_hop, char *line)
{
  t_sub_llid *elem = g_head_sub_llid;
  t_sub_llid *next;
  while(elem)
    {
    next = elem->next;
    if (elem->flags_hop & flags_hop)
      {
      rpct_send_hop_msg(elem->llid, elem->tid, flags_hop, line);
      }
    elem = next;
    }
}
/*-----------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_hop_print_add_sub(int llid, int tid, int flags)
{
  insert_in_sub(&(g_head_sub_llid), llid, tid, flags);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_hop_print_del_sub(int llid)
{
  extract_from_sub(&(g_head_sub_llid), llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_hop_print(int flags_hop, const char * format, ...)
{
  va_list arguments;
  char line[MAX_HOP_PRINT_LEN];
  int len;

  va_start (arguments, format);
  memset(line, 0, MAX_HOP_PRINT_LEN);
  len = snprintf(line, MAX_HOP_PRINT_LEN-1, "%07u: ",
                 (unsigned int) cloonix_get_msec());
  vsnprintf (line+len, MAX_HOP_PRINT_LEN-1, format, arguments);
  interface_send_evt_txt (flags_hop, line);
  va_end (arguments);
}
/*-----------------------------------------------------------------------*/


/*****************************************************************************/
static void msg_tx(int llid, int len, char *buf)
{
  buf[len] = 0;
  g_string_tx(llid, len+1, buf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_buf_tx(void)
{
  return(g_buf_tx);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_send_pid_req(int llid, int tid, char *name, int num)
{
  int len;
  char *buf = get_buf_tx();
  if (!name || !strlen(name))
    KOUT(" ");
  len = sprintf(buf, HOP_PID_REQ, tid, name, num);
  msg_tx(llid, len, buf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_send_kil_req(int llid, int tid)
{
  int len;
  char *buf = get_buf_tx();
  len = sprintf(buf, HOP_KIL_REQ, tid);
  msg_tx(llid, len, buf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_send_pid_resp(int llid, int tid, char *name, int num,
                        int toppid, int pid)
{
  int len;
  char *buf = get_buf_tx();
  if (!name || !strlen(name))
    KOUT(" ");
  len = sprintf(buf, HOP_PID_RESP, tid, name, num, toppid, pid);
  msg_tx(llid, len, buf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_send_hop_sub(int llid, int tid, int flags_hop)
{
  int len;
  char *buf = get_buf_tx();
  len = sprintf(buf, HOP_EVT_SUB, tid, flags_hop);
  msg_tx(llid, len, buf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_send_hop_unsub(int llid, int tid)
{
  int len;
  char *buf = get_buf_tx();
  len = sprintf(buf, HOP_EVT_UNSUB, tid);
  msg_tx(llid, len, buf);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void rpct_send_hop_msg(int llid, int tid,
                      int flags_hop, char *txt)
{
  int len;
  char *buf = get_buf_tx();
  len = sprintf(buf, HOP_EVT_O, tid, flags_hop);
  len += sprintf(buf+len, HOP_FREE_TXT, txt);
  len += sprintf(buf+len, HOP_EVT_C);
  msg_tx(llid, len, buf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_send_sigdiag_msg(int llid, int tid, char *line)
{
  int len;
  char *buf = get_buf_tx();
  if (!line)
    KOUT(" ");
  if (strlen(line) < 1)
    KOUT(" ");
  if (strlen(line) >= MAX_RPC_MSG_LEN)
    line[MAX_RPC_MSG_LEN-1] = 0;
  len = sprintf(buf, MUSIGDIAG_MSG_O, tid);
  len += sprintf(buf+len, MUSIGDIAG_MSG_I, line);
  len += sprintf(buf+len, MUSIGDIAG_MSG_C);
  msg_tx(llid, len, buf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_send_poldiag_msg(int llid, int tid, char *line)
{
  int len;
  char *buf = get_buf_tx();
  if (!line)
    KOUT(" ");
  if (strlen(line) < 1)
    KOUT(" ");
  if (strlen(line) >= MAX_RPC_MSG_LEN)
    line[MAX_RPC_MSG_LEN-1] = 0;
  len = sprintf(buf, MUPOLDIAG_MSG_O, tid);
  len += sprintf(buf+len, MUPOLDIAG_MSG_I, line);
  len += sprintf(buf+len, MUPOLDIAG_MSG_C);
  msg_tx(llid, len, buf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void dispatcher(int llid, int bnd_evt, char *msg)
{
  int flags_hop, toppid, pid, tid, len, num;
  char *ptrs, *ptre, *line;
  char name[MAX_NAME_LEN];


  switch(bnd_evt)
    {

    case bnd_rpct_pid_req:
      if (sscanf(msg, HOP_PID_REQ, &tid, name, &num) != 3)
        KOUT("%s", msg);
      rpct_recv_pid_req(llid, tid, name, num);
      break;

    case bnd_rpct_kil_req:
      if (sscanf(msg, HOP_KIL_REQ, &tid) != 1)
        KOUT("%s", msg);
      rpct_recv_kil_req(llid, tid);
      break;

    case bnd_rpct_pid_resp:
      if (sscanf(msg, HOP_PID_RESP, &tid, name, &num, &toppid, &pid) != 5)
        KOUT("%s", msg);
      rpct_recv_pid_resp(llid, tid, name, num, toppid, pid);
      break;

    case bnd_rpct_hop_evt_sub:
      if (sscanf(msg, HOP_EVT_SUB, &tid, &flags_hop) != 2)
        KOUT("%s", msg);
      rpct_recv_hop_sub(llid, tid, flags_hop);
      break;

    case bnd_rpct_hop_evt_unsub:
      if (sscanf(msg, HOP_EVT_UNSUB, &tid) != 1)
        KOUT("%s", msg);
      rpct_recv_hop_unsub(llid, tid);
      break;

    case bnd_rpct_sigdiag_msg:
      if (sscanf(msg, MUSIGDIAG_MSG_O, &tid) != 1)
        KOUT("%s", msg);
      ptrs = strstr(msg, "<sigdiag_msg_delimiter>");
      if (!ptrs)
        KOUT("%s", msg);
      ptrs += strlen("<sigdiag_msg_delimiter>");
      ptre = strstr(ptrs, "</sigdiag_msg_delimiter>");
      if (!ptre)
        KOUT("%s", msg);
      len = ptre - ptrs;
      line = (char *) malloc(len+1);
      memset(line, 0, len+1);
      memcpy(line, ptrs, len);
      rpct_recv_sigdiag_msg(llid, tid, line);
      free(line);
      break;

    case bnd_rpct_poldiag_msg:
      if (sscanf(msg, MUPOLDIAG_MSG_O, &tid) != 1)
        KOUT("%s", msg);
      ptrs = strstr(msg, "<poldiag_msg_delimiter>");
      if (!ptrs)
        KOUT("%s", msg);
      ptrs += strlen("<poldiag_msg_delimiter>");
      ptre = strstr(ptrs, "</poldiag_msg_delimiter>");
      if (!ptre)
        KOUT("%s", msg);
      len = ptre - ptrs;
      line = (char *) malloc(len+1);
      memset(line, 0, len+1);
      memcpy(line, ptrs, len);
      rpct_recv_poldiag_msg(llid, tid, line);
      free(line);
      break;

    case bnd_rpct_hop_evt_msg:
      if (sscanf(msg, HOP_EVT_O, &tid, &flags_hop) != 2)
        KOUT("%s", msg);
      line = get_hop_free_txt(msg);
      rpct_recv_hop_msg(llid, tid, flags_hop, line);
      free(line);
      break;


    default:
      KOUT(" ");
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void extract_boundary(char *input, char *output)
{
  int bound_len;
  if (input[0] != '<')
    KOUT("%s\n", input);
  bound_len = strcspn(input, ">");
  if (bound_len >= MAX_CLOWNIX_BOUND_LEN)
    KOUT("%s\n", input);
  if (bound_len < MIN_CLOWNIX_BOUND_LEN)
    KOUT("%s\n", input);
  memcpy(output, input, bound_len);
  output[bound_len] = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int get_bnd_evt(char *bound)
{
  int i, result = 0;
  for (i=bnd_rpct_min; i<bnd_rpct_max; i++)
    {
    if (!strcmp(bound, g_bound_list[i]))
      {
      result = i;
      break;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int rpct_decoder(int llid, int len, char *str_rx)
{
  int  result = -1;
  int  bnd_evt;
  char bound[MAX_CLOWNIX_BOUND_LEN];
  if (len != ((int) strlen(str_rx) + 1))
    KOUT("%d %d %s", len, ((int) strlen(str_rx) + 1), str_rx);
  extract_boundary(str_rx, bound);
  bnd_evt = get_bnd_evt(bound);
  if (bnd_evt)
    {
    dispatcher(llid, bnd_evt, str_rx);
    result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_redirect_string_tx(t_rpct_tx rpc_tx)
{
  g_string_tx = rpc_tx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_init(t_rpct_tx rpc_tx)
{
  g_string_tx = rpc_tx;
  g_buf_tx = (char *) malloc(100000);
  extract_boundary(MUSIGDIAG_MSG_O,  g_bound_list[bnd_rpct_sigdiag_msg]);
  extract_boundary(MUPOLDIAG_MSG_O,  g_bound_list[bnd_rpct_poldiag_msg]);
  extract_boundary(HOP_PID_REQ,      g_bound_list[bnd_rpct_pid_req]);
  extract_boundary(HOP_KIL_REQ,      g_bound_list[bnd_rpct_kil_req]);
  extract_boundary(HOP_PID_RESP,     g_bound_list[bnd_rpct_pid_resp]);
  extract_boundary(HOP_EVT_SUB,      g_bound_list[bnd_rpct_hop_evt_sub]);
  extract_boundary(HOP_EVT_UNSUB,    g_bound_list[bnd_rpct_hop_evt_unsub]);
  extract_boundary(HOP_EVT_O,        g_bound_list[bnd_rpct_hop_evt_msg]);

}
/*---------------------------------------------------------------------------*/
