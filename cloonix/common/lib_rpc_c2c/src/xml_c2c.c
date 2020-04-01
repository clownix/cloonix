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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "io_clownix.h"
#include "xml_c2c.h"
#include "rpc_c2c.h"


/*---------------------------------------------------------------------------*/
static char *sndbuf=NULL;
/*---------------------------------------------------------------------------*/
enum 
  {
  bnd_min = 0,
  bnd_c2c_peer_create,
  bnd_c2c_ack_peer_create,
  bnd_c2c_peer_ping,
  bnd_c2c_peer_delete,
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
    g_llid_tx(llid, len + 1, buf);
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
static int check_str(char *str)
{
  int result = -1;
  if (!str)
    KERR(" ");
  else if (strlen(str) == 0)
    KERR(" ");
  else if (strlen(str) >= MAX_NAME_LEN)
    KERR(" ");
  else
    result = 0;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_c2c_peer_create(int llid, int tid, char *name,
                          char *master_cloonix, int master_idx,
                          int ip_c2c_slave, int port_c2c_slave)
{
  int len;
  if (check_str(name))
    KOUT(" ");
  if (check_str(master_cloonix))
    KOUT(" ");
  len = sprintf(sndbuf, C2C_PEER_CREATE, tid, name, master_cloonix, 
                                         master_idx, 
                                         ip_c2c_slave, port_c2c_slave);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_c2c_ack_peer_create(int llid, int tid, char *name,
                              char *master_cloonix, char *slave_cloonix,
                              int master_idx, int slave_idx, int status)
{
  int len;
  if (check_str(name))
    KOUT(" ");
  if (check_str(master_cloonix))
    KOUT(" ");
  if (check_str(slave_cloonix))
    KOUT(" ");
  len = sprintf(sndbuf, C2C_ACK_PEER_CREATE, tid, name,
                                             master_cloonix, slave_cloonix,
                                             master_idx, slave_idx, status);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_c2c_peer_delete(int llid, int tid, char *name)
{
  int len;
  if (check_str(name))
    KOUT(" ");
  len = sprintf(sndbuf, C2C_PEER_DELETE, tid, name);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_c2c_peer_ping(int llid, int tid, char *name)
{
  int len;
  if (check_str(name))
    KOUT(" ");
  len = sprintf(sndbuf, C2C_PEER_PING, tid, name);
  my_msg_mngt_tx(llid, len, sndbuf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void dispatcher(int llid, int bnd_evt, char *msg)
{
  int tid, status, master_idx, slave_idx;
  int ip_slave, port_slave;
  char name[MAX_NAME_LEN];
  char master_cloonix[MAX_NAME_LEN];
  char slave_cloonix[MAX_NAME_LEN];
  switch(bnd_evt)
    {

    case bnd_c2c_peer_create:
      if (sscanf(msg, C2C_PEER_CREATE, &tid, name, master_cloonix, &master_idx, 
                                       &ip_slave, &port_slave) != 6)
        KOUT("%s", msg);
      recv_c2c_peer_create(llid, tid, name, master_cloonix, master_idx, 
                           ip_slave, port_slave);
      break;

    case bnd_c2c_ack_peer_create:
      if (sscanf(msg, C2C_ACK_PEER_CREATE, &tid, name, 
                                           master_cloonix, slave_cloonix, 
                                           &master_idx, &slave_idx, 
                                           &status) != 7)
        KOUT("%s", msg);
      recv_c2c_ack_peer_create(llid, tid, name, master_cloonix, 
                               slave_cloonix, master_idx, slave_idx, status);
      break;

    case bnd_c2c_peer_ping:
      if (sscanf(msg, C2C_PEER_PING, &tid, name) != 2)
        KOUT("%s", msg);
      recv_c2c_peer_ping(llid, tid, name);
      break;

    case bnd_c2c_peer_delete:
      if (sscanf(msg, C2C_PEER_DELETE, &tid, name) != 2) 
        KOUT("%s", msg);
      recv_c2c_peer_delete(llid, tid, name);
      break;

    default:
      KOUT("%s", msg);
    }
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
int doors_io_c2c_decoder (int llid, int len, char *chunk)
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
void doors_io_c2c_tx_set(t_llid_tx llid_tx)
{
  g_llid_tx = llid_tx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void doors_io_c2c_init(t_llid_tx llid_tx)
{
  g_llid_tx = llid_tx;
  if (!g_llid_tx)
    KOUT(" ");
  sndbuf = get_bigbuf();
  memset (bound_list, 0, bnd_max * MAX_CLOWNIX_BOUND_LEN);
  extract_boundary (C2C_PEER_CREATE, bound_list[bnd_c2c_peer_create]);
  extract_boundary (C2C_ACK_PEER_CREATE, bound_list[bnd_c2c_ack_peer_create]);
  extract_boundary (C2C_PEER_PING, bound_list[bnd_c2c_peer_ping]);
  extract_boundary (C2C_PEER_DELETE, bound_list[bnd_c2c_peer_delete]);
}
/*---------------------------------------------------------------------------*/


