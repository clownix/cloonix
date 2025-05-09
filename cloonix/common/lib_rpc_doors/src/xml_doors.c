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
#include <sys/socket.h>
#include <sys/types.h>
#include <math.h>
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "xml_doors.h"
#include "doors_rpc.h"
/*--------------------------------------------------------------------------*/

enum 
  {
  bnd_min = 0,
  bnd_info,
  bnd_event,
  bnd_command,
  bnd_status,
  bnd_pid_req,
  bnd_pid_resp,
  bnd_add_vm,
  bnd_del_vm,
  bnd_max,
  };

/*--------------------------------------------------------------------------*/
static char *sndbuf=NULL;
/*--------------------------------------------------------------------------*/
static char bound_list[bnd_max][MAX_CLOWNIX_BOUND_LEN];
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void my_msg_mngt_tx(int llid, int len, char *buf)
{
  if (len > MAX_SIZE_BIGGEST_MSG - 1000)
    KOUT("%d %d", len, MAX_SIZE_BIGGEST_MSG);
  if (len > MAX_SIZE_BIGGEST_MSG/2)
    KERR("WARNING LEN MSG %d %d", len, MAX_SIZE_BIGGEST_MSG);
  if (msg_exist_channel(llid))
    {
    string_tx(llid, len, buf);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
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
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void doors_send_info(int llid, int tid, char *type, char *info)
{
  int len;
  if (!type||(strlen(type)==0)||(strlen(type)>=MAX_NAME_LEN))
    KOUT(" ");
  if (!info || (strlen(info)==0))
    KOUT(" ");
  len = sprintf(sndbuf, INFO_O, tid, type);
  len += sprintf(sndbuf+len, INFO_I, info);
  len += sprintf(sndbuf+len, INFO_C);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void doors_send_event(int llid, int tid, char *name, char *line)
{
  int len;
  if (!name||(strlen(name)==0)||(strlen(name)>=MAX_NAME_LEN))
    KOUT(" ");
  if (!line || (strlen(line)==0))
    KOUT(" ");
  len = sprintf(sndbuf, EVENT_O, tid, name);
  len += sprintf(sndbuf+len, EVENT_I, line);
  len += sprintf(sndbuf+len, EVENT_C);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void doors_send_command(int llid, int tid, char *name, char *cmd)
{
  int len;
  if (!name||(strlen(name)==0)||(strlen(name)>=MAX_NAME_LEN))
    KOUT(" ");
  if (!cmd || (strlen(cmd)==0))
    KOUT(" ");
  len = sprintf(sndbuf, COMMAND_O, tid, name);
  len += sprintf(sndbuf+len, COMMAND_I, cmd);
  len += sprintf(sndbuf+len, COMMAND_C);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void doors_send_status(int llid, int tid, char *name, char *status)
{
  int len;
  if (!name||(strlen(name)==0)||(strlen(name)>=MAX_NAME_LEN))
    KOUT(" ");
  if (!status || (strlen(status)==0))
    KOUT(" ");
  len = sprintf(sndbuf, STATUS_O, tid, name);
  len += sprintf(sndbuf+len, STATUS_I, status);
  len += sprintf(sndbuf+len, STATUS_C);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void doors_send_add_vm(int llid,int tid,char *name, char *way2agent)
{
  int len;
  if (!name||(strlen(name)==0)||(strlen(name)>=MAX_NAME_LEN))
    KOUT(" ");
  if (!way2agent||(strlen(way2agent)==0)||(strlen(way2agent)>=MAX_PATH_LEN))
    KOUT(" ");
  len = sprintf(sndbuf, ADD_VM, tid, name, way2agent);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void doors_send_del_vm(int llid, int tid, char *name)
{
  int len;
  if (!name||(strlen(name)==0)||(strlen(name)>=MAX_NAME_LEN))
    KOUT(" ");
  len = sprintf(sndbuf, DEL_VM, tid, name);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void dispatcher(int llid, int bnd_evt, char *msg)
{
  int tid;
  char *ptrs, *ptre;
  char name[MAX_NAME_LEN];
  char sock[MAX_PATH_LEN];
  switch(bnd_evt)
    {

    case bnd_info:
      if (sscanf(msg, INFO_O, &tid, name) != 2)
        KOUT("%s", msg);
      ptrs = strstr(msg, "<doors_info_delimiter>");
      ptre = strstr(msg, "</doors_info_delimiter>");
      if ((!ptrs) || (!ptre))
        KOUT("%s", msg);
      *ptre = 0;
      ptrs += strlen("<doors_info_delimiter>");
      doors_recv_info(llid, tid, name, ptrs);
      break;

    case bnd_event:
      if (sscanf(msg, EVENT_O, &tid, name) != 2)
        KOUT("%s", msg);
      ptrs = strstr(msg, "<doors_event_delimiter>");
      ptre = strstr(msg, "</doors_event_delimiter>");
      if ((!ptrs) || (!ptre))
        KOUT("%s", msg);
      *ptre = 0;
      ptrs += strlen("<doors_event_delimiter>");
      doors_recv_event(llid, tid, name, ptrs);
      break;

    case bnd_command:
      if (sscanf(msg, COMMAND_O, &tid, name) != 2)
        KOUT("%s", msg);
      ptrs = strstr(msg, "<doors_command_delimiter>");
      ptre = strstr(msg, "</doors_command_delimiter>");
      if ((!ptrs) || (!ptre))
        KOUT("%s", msg);
      *ptre = 0;
      ptrs += strlen("<doors_command_delimiter>");
      doors_recv_command(llid, tid, name, ptrs);
      break;
    case bnd_status:
      if (sscanf(msg, STATUS_O, &tid, name) != 2)
        KOUT("%s", msg);
      ptrs = strstr(msg, "<doors_status_delimiter>");
      ptre = strstr(msg, "</doors_status_delimiter>");
      if ((!ptrs) || (!ptre))
        KOUT("%s", msg);
      *ptre = 0;
      ptrs += strlen("<doors_status_delimiter>");
      doors_recv_status(llid, tid, name, ptrs);
      break;
    case bnd_add_vm:
      if (sscanf(msg, ADD_VM, &tid, name, sock) != 3)
        KOUT("%s", msg);
      doors_recv_add_vm(llid, tid, name, sock);
      break;
    case bnd_del_vm:
      if (sscanf(msg, DEL_VM, &tid, name) != 2)
        KOUT("%s", msg);
      doors_recv_del_vm(llid, tid, name);
      break;
    default:
      KOUT(" ");
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int get_bnd_event(char *bound)
{
  int i, result = 0;
  for (i=bnd_min; i<bnd_max; i++)
    {
    if (!strcmp(bound, bound_list[i]))
      {
      result = i;
      break;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int doors_xml_decoder (int llid, int len, char *str_rx)
{
  int result = -1;
  int bnd_arrival_event;
  char bound[MAX_CLOWNIX_BOUND_LEN];
  if (len != ((int) strlen(str_rx) + 1))
    KOUT(" ");
  extract_boundary(str_rx, bound);
  bnd_arrival_event = get_bnd_event(bound);
  if (bnd_arrival_event)
    {
    dispatcher(llid, bnd_arrival_event, str_rx);
    result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void doors_xml_init(void)
{
  sndbuf = get_bigbuf();
  memset (bound_list, 0, bnd_max * MAX_CLOWNIX_BOUND_LEN);
  extract_boundary (INFO_O,    bound_list[bnd_info]);
  extract_boundary (EVENT_O,    bound_list[bnd_event]);
  extract_boundary (COMMAND_O,  bound_list[bnd_command]);
  extract_boundary (STATUS_O,   bound_list[bnd_status]);
  extract_boundary (ADD_VM,     bound_list[bnd_add_vm]);
  extract_boundary (DEL_VM,     bound_list[bnd_del_vm]);
}
/*---------------------------------------------------------------------------*/
