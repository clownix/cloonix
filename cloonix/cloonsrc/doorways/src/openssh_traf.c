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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "io_clownix.h"
#include "dispach.h"
#include "sock.h"

char *get_u2i_nat_path_with_name(char *nat);


/****************************************************************************/
static void send_to_openssh_client(int dido_llid, int val, int len, char *buf)
{
  if (dispach_send_to_openssh_client(dido_llid, val, len, buf))
    {
    KERR("%d %d %d", dido_llid, len, val);
    }
  else
    {
    if (val != doors_val_none)
      DOUT(FLAG_HOP_DOORS, "CLIENT_TX: %d %s", dido_llid, buf);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void openssh_tx_to_nat(int inside_llid, int len, char *buf)
{
  watch_tx(inside_llid, len, buf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int nat_rx_cb(int llid, int fd)
{
  int len, init_done;
  char *buf = get_g_buf();
  int dido_llid = dispatch_get_dido_llid_with_inside_llid(llid, &init_done);
  if (dido_llid > 0)
    {
    len = read (fd, buf, MAX_DOORWAYS_BUF_LEN);
    if (len > 0)
      {
      if (init_done == 1)
        {
        send_to_openssh_client(dido_llid, doors_val_none, len, buf);
        }
      else
        {
        send_to_openssh_client(dido_llid, doors_val_sig, len, buf);
        dispatch_set_init_done_with_inside_llid(llid);
        }
      }
    else
      {
      KERR("%d %d", dido_llid, llid);
      dispach_door_end(dido_llid);
      if (msg_exist_channel(llid))
        msg_delete_channel(llid);
      }
    }
  else
    {
    KERR("%d %d", dido_llid, llid);
    if (msg_exist_channel(llid))
      msg_delete_channel(llid);
    }
  return len;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_close_dido_llid(void *data)
{
  unsigned long ul_llid = (unsigned long) data;
  int init_done, llid = (int)ul_llid;
  int dido_llid = dispatch_get_dido_llid_with_inside_llid(llid, &init_done);
  if (dido_llid)
    dispach_door_end(dido_llid);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void nat_err_cb (int llid, int err, int from)
{
  char *nack = "UNIX2INET_NACK_CUTOFF";
  unsigned long ul_llid = (unsigned long) llid;
  int init_done;
  int dido_llid = dispatch_get_dido_llid_with_inside_llid(llid, &init_done);
  clownix_timeout_add(20,timer_close_dido_llid,(void *)ul_llid,NULL,NULL);
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
  send_to_openssh_client(dido_llid, doors_val_sig, strlen(nack) + 1, nack);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int plug_to_nat_llid(char *unix_sock)
{
  char nousebuf[MAX_PATH_LEN];
  int fd, llid, len, result = -1;
  fd = sock_nonblock_client_unix(unix_sock);
  if (fd >= 0)
    {
    do
      {
      len = read(fd, nousebuf, MAX_PATH_LEN);
      if (len && (len != -1))
        KERR("%d", len);
      }
    while (len > 0);
    llid = msg_watch_fd(fd, nat_rx_cb, nat_err_cb, "cloon");
    if (llid == 0)
      KOUT(" ");
    result = llid;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int openssh_rx_from_client_init(int dido_llid, int len, char *buf_rx)
{
  char *unix_sock;
  char nat[MAX_NAME_LEN];
  char resp[MAX_PATH_LEN];
  int llid, result = -1;
  memset(resp, 0, MAX_PATH_LEN);
  if (sscanf(buf_rx, "OPENSSH_DOORWAYS_REQ nat=%s", nat) == 1)
    {
    unix_sock = get_u2i_nat_path_with_name(nat);
    llid = plug_to_nat_llid(unix_sock);
    if (llid > 0)
      {
      snprintf(resp, MAX_PATH_LEN-1, "OPENSSH_DOORWAYS_RESP nat=%s", nat);
      result = llid;
      }
    else
      snprintf(resp, MAX_PATH_LEN-1, "KOCONN %s", unix_sock); 
    }
  else
    snprintf(resp, MAX_PATH_LEN-1, "KOSCAN"); 
  send_to_openssh_client(dido_llid, doors_val_link_ko, 
                         strlen(resp) + 1, resp);
  return result;
}
/*--------------------------------------------------------------------------*/


