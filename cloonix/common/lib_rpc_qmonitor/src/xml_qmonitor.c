/*****************************************************************************/
/*    Copyright (C) 2006-2019 cloonix@cloonix.net License AGPL-3             */
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
#include "io_clownix.h"
#include "xml_qmonitor.h"
#include "rpc_qmonitor.h"

/*---------------------------------------------------------------------------*/
static char *sndbuf=NULL;
/*---------------------------------------------------------------------------*/


enum 
  {
  bnd_min = 0,
  bnd_qmonitor,
  bnd_sub2qmonitor,
  bnd_max,
  };
static char bound_list[bnd_max][MAX_CLOWNIX_BOUND_LEN];
/*---------------------------------------------------------------------------*/
static t_llid_tx g_llid_tx;
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void my_msg_mngt_tx(int llid, int len, char *buf)
{
  if (len > MAX_SIZE_BIGGEST_MSG - 1000)
    KOUT("%d %d", len, MAX_SIZE_BIGGEST_MSG);
  if (len > MAX_SIZE_BIGGEST_MSG/2)
    KERR("WARNING LEN MSG %d %d", len, MAX_SIZE_BIGGEST_MSG);
  if (msg_exist_channel(llid))
    {
    if (!g_llid_tx)
      KOUT(" ");
    buf[len] = 0;
    g_llid_tx(llid, len+1, buf);
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
static int get_bnd_event(char *bound)
{
  int i, result = 0;
  for (i=bnd_min; i<bnd_max; i++) 
    if (!strcmp(bound, bound_list[i]))
      {
      result = i;
      break;
      }
  return result;
}
/*---------------------------------------------------------------------------*/
 
/*****************************************************************************/
void send_sub2qmonitor(int llid, int tid, char *name, int on_off)
{
  int len;
  len = sprintf(sndbuf, SUB2QMONITOR, tid, name, on_off);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_qmonitor(int llid, int tid, char *name, char *data)
{
  int len;
  len = sprintf(sndbuf, QMONITOR_O, tid, name);
  len += sprintf(sndbuf+len, QMONITOR_I, data);
  len += sprintf(sndbuf+len, QMONITOR_C);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void dispatcher(int llid, int bnd_evt, char *msg)
{
  int tid, on_off;
  char name[MAX_NAME_LEN];
  char *ptrs, *ptre;
  switch(bnd_evt)
    {

    case bnd_sub2qmonitor:
      if (sscanf(msg, SUB2QMONITOR, &tid, name, &on_off) != 3)
        KOUT("%s", msg);
      recv_sub2qmonitor(llid, tid, name, on_off);
      break;

    case bnd_qmonitor:
      if (sscanf(msg, QMONITOR_O, &tid, name) != 2)
        KOUT("%s", msg);
      ptrs = strstr(msg, "<q_monitor_asciidata_delimiter>");
      ptre = strstr(msg, "</q_monitor_asciidata_delimiter>");
      if ((!ptrs) || (!ptre))
        KOUT("%s", msg);
      *ptre = 0;
      ptrs += strlen("<q_monitor_asciidata_delimiter>");
      recv_qmonitor(llid, tid, name, ptrs);
      break;

    default:
      KOUT("%s", msg);

    }
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
int doors_io_qmonitor_decoder (int llid, int len, char *chunk)
{
  int result = -1;
  int bnd_event;
  char bound[MAX_CLOWNIX_BOUND_LEN];
  if (len != ((int) strlen(chunk)) + 1)
    KOUT(" %d %d %s\n", len, (int)strlen(chunk), chunk);
  extract_boundary(chunk, bound);
  bnd_event = get_bnd_event(bound);
  if (bnd_event)
    {
    dispatcher(llid, bnd_event, chunk);
    result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void doors_io_qmonitor_xml_init(t_llid_tx llid_tx)
{
  g_llid_tx = llid_tx;
  if (!g_llid_tx)
    KOUT(" ");
  sndbuf = get_bigbuf();
  memset (bound_list, 0, bnd_max * MAX_CLOWNIX_BOUND_LEN);
  extract_boundary (QMONITOR_O, bound_list[bnd_qmonitor]);
  extract_boundary (SUB2QMONITOR, bound_list[bnd_sub2qmonitor]);
}
/*---------------------------------------------------------------------------*/



