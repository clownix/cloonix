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
#include <stdint.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <errno.h>
#include <fcntl.h>

#include "io_clownix.h"
#include "util_sock.h"
#include "rpc_clownix.h"
#include "src/utils.h"
#include "ssh_cisco_nat.h"
#include "machine.h"


#define CLOONIX_INFO "user %s ip %s port %d "

/*--------------------------------------------------------------------------*/
typedef struct t_ssh_cisco
{
  int llid;
  int info_received;
  struct t_ssh_cisco *prev;
  struct t_ssh_cisco *next;
} t_ssh_cisco;
/*--------------------------------------------------------------------------*/
static t_ssh_cisco *g_head_ssh_cisco;
static uint32_t g_offset;


/****************************************************************************/
static t_ssh_cisco *find_ssh_cisco(int llid)
{
  t_ssh_cisco *cur = g_head_ssh_cisco;
  while(cur)
    {
    if (cur->llid == llid)
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void alloc_ssh_cisco(int llid)
{
  t_ssh_cisco *cur = (t_ssh_cisco *) utils_malloc(sizeof(t_ssh_cisco));
  if (cur == NULL)
    KOUT("ERROR MALLOC");
  memset(cur, 0, sizeof(t_ssh_cisco));
  cur->llid = llid;
  if (g_head_ssh_cisco)
    g_head_ssh_cisco->prev = cur;
  cur->next = g_head_ssh_cisco;
  g_head_ssh_cisco = cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_ssh_cisco(t_ssh_cisco *cur)
{
  if (msg_exist_channel(cur->llid))
    msg_delete_channel(cur->llid);
  if (cur->info_received == 1)
    ssh_cisco_nat_rx_err(cur->llid);
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_ssh_cisco)
    g_head_ssh_cisco = cur->next;
  utils_free(cur);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int get_info_from_buf(int len, char *ibuf, char *remote_user,
                             uint32_t *remote_ip, uint16_t *remote_port)
{
  int port, result = -1;
  char *ptr;
  char *buf = (char *) utils_malloc(len);
  char str_ip[MAX_NAME_LEN];
  if (buf == NULL)
    KOUT("ERROR MALLOC");
  memcpy(buf, ibuf, len);
  ptr = strchr(buf, '=');
  while (ptr)
    {
    *ptr = ' ';
    ptr = strchr(buf, '=');
    }
  ptr = (strstr(buf, "cloonix_info_end"));
  if (!(ptr))
    KERR("ERROR %d %s", len, buf);
  else
    {
    *ptr = 0;
    if (sscanf(buf, CLOONIX_INFO, remote_user, str_ip, &port) != 3)
      KERR("ERROR %d %s", len, buf);
    else if (ip_string_to_int (remote_ip, str_ip))
      KERR("ERROR %d %s", len, buf);
    else
      {
      *remote_port = (port & 0xFFFF);
      ptr = ptr + strlen("cloonix_info_end");
      if ((*ptr) != 0)
        KERR("ERROR %d %s", len, buf);
      else
        result = 0;
      }
    }
  utils_free(buf);
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_cb(int llid, int fd)
{
  int data_len, result = 0;
  uint8_t data[MAX_RXTX_LEN];
  char user[MAX_NAME_LEN];
  char vm[MAX_NAME_LEN];
  uint32_t ip;
  uint16_t port;
  t_ssh_cisco *cur = find_ssh_cisco(llid);
  data_len = read(fd, data, MAX_RXTX_LEN - g_offset - 4);
  if (!cur)
    {
    if (msg_exist_channel(llid))
      msg_delete_channel(llid);
    KERR("ERROR %d", llid);
    }
  else if (data_len == 0)
    {
    free_ssh_cisco(cur);
    }
  else if (data_len < 0)
    {
    if ((errno != EAGAIN) && (errno != EINTR))
      {
      KERR("ERROR %d", llid);
      free_ssh_cisco(cur);
      }
    }
  else
    {
    if (cur->info_received == 0)
      {
      if (get_info_from_buf(data_len, (char *) data, user, &ip, &port))
        {
        KERR("ERROR %s", data);
        free_ssh_cisco(cur);
        }
      else if (!machine_name_exists_with_ip(ip, vm))
        {
        KERR("ERROR %s", data);
        free_ssh_cisco(cur);
        }
      else
        {
        cur->info_received = 1;
        ssh_cisco_nat_connect(llid, vm, ip, port);
        }
      }
    else
      {
      ssh_cisco_nat_rx_from_llid(llid, data_len, data);
      }
    result = data_len;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void err_cb(int llid, int err, int from)
{
  t_ssh_cisco *cur = find_ssh_cisco(llid);
  if (!cur)
    {
    if (msg_exist_channel(llid))
      msg_delete_channel(llid);
    KERR("ERROR CISCO SSH TRAFFIC ERROR %d %d", err, from);
    }
  else
    {
    free_ssh_cisco(cur);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void listen_error(int llid, int err, int from)
{
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
  KERR("ERROR CISCO SSH LISTEN PROBLEM %d %d", err, from);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int listen_event(int llid, int fd)
{
  int traffic_fd, traffic_llid;
  util_fd_accept(fd, &traffic_fd, __FUNCTION__);
  traffic_llid = msg_watch_fd(traffic_fd, rx_cb, err_cb, "cisco_u2i");
  if (traffic_llid == 0)
    KERR("ERROR CONNECT FOR CISCO SSH PROBLEM");
  else
    alloc_ssh_cisco(traffic_llid);
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void ssh_cisco_llid_close(int llid)
{
  t_ssh_cisco *cur = find_ssh_cisco(llid);
  if (cur)
    {
    cur->info_received = 0; 
    free_ssh_cisco(cur);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void ssh_cisco_llid_transmit(int llid, int len, uint8_t *data)
{
  if (msg_exist_channel(llid))
    {
    watch_tx(llid, len, (char *) data);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void ssh_cisco_llid_init(char *cisco_u2i)
{
  int listen_llid;
  int fd = util_socket_listen_unix(cisco_u2i);
  if (fd < 0)
    {
    KERR("ERROR SOCKET FOR CISCO SSH PROBLEM %s", cisco_u2i);
    }
  else
    {
    listen_llid = msg_watch_fd(fd, listen_event , listen_error, "cisco_u2i");
    if (listen_llid == 0)
      KERR("ERROR SOCKET FOR CISCO SSH PROBLEM %s", cisco_u2i);
    }
  g_offset  = ETHERNET_HEADER_LEN;
  g_offset += IPV4_HEADER_LEN;
  g_offset += TCP_HEADER_LEN;
}
/*--------------------------------------------------------------------------*/

