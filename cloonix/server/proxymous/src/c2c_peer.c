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
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/select.h>
#include <fcntl.h>
#include <linux/tcp.h>
#include <time.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doorways_sock.h"
#include "proxy_crun.h"
#include "util_sock.h"

typedef struct t_c2c_peer
{
  int pair_listen_llid;
  int pair_llid;
  char name[MAX_NAME_LEN];
  uint32_t ip;
  uint16_t port;
  char dist_passwd[MSG_DIGEST_LEN];
  struct t_c2c_peer *prev;
  struct t_c2c_peer *next;
} t_c2c_peer;

static t_c2c_peer *g_c2c_peer_head;

/*****************************************************************************/
static t_c2c_peer *find_c2c_peer_with_name(char *name)
{
  t_c2c_peer *cur = g_c2c_peer_head;
  while (cur)
    {
    if (!strcmp(cur->name, name))
      break;
    cur = cur->next;
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_c2c_peer *find_c2c_peer_with_listen_llid(int pair_listen_llid)
{
  t_c2c_peer *cur = g_c2c_peer_head;
  while (cur)
    {
    if ((pair_listen_llid) && (cur->pair_listen_llid == pair_listen_llid))
      break;
    cur = cur->next;
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_c2c_peer *find_c2c_peer_with_llid(int pair_llid)
{
  t_c2c_peer *cur = g_c2c_peer_head;
  while (cur)
    {
    if ((pair_llid) && (cur->pair_llid == pair_llid))
      break;
    cur = cur->next;
    }
  return cur;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void alloc_c2c_peer(int pair_listen_llid, char *name, uint32_t ip,
                           uint16_t port, char *dist_passwd)
{
  t_c2c_peer *cur = (t_c2c_peer *) malloc(sizeof(t_c2c_peer));
  memset(cur, 0, sizeof(t_c2c_peer)); 
  cur->pair_listen_llid = pair_listen_llid;
  cur->ip = ip;
  cur->port = port;
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  strncpy(cur->dist_passwd, dist_passwd, MSG_DIGEST_LEN-1);
  if (g_c2c_peer_head)
    g_c2c_peer_head->prev = cur;
  cur->next = g_c2c_peer_head;
  g_c2c_peer_head = cur;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void free_c2c_peer(t_c2c_peer *cur)
{
  udp_proxy_end();
  close(get_fd_with_llid(cur->pair_listen_llid));
  doorways_sock_client_inet_delete(cur->pair_listen_llid);
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_c2c_peer_head) 
    g_c2c_peer_head = cur->next;
  free(cur);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void peer_err_cb (int llid)
{
  t_c2c_peer *cur = find_c2c_peer_with_llid(llid);
  char *locnet = get_cloonix_name();
  if (!cur)
    KERR("ERROR %s %d", locnet, llid);
  else 
    {
    if (cur->pair_llid != llid)
      KERR("ERROR %s %d %d %s", locnet, cur->pair_llid, llid, cur->name);
    if (msg_exist_channel(llid))
      msg_delete_channel(llid);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void peer_rx_cb(int llid, int tid,
                       int len_bufraw, char *doors_bufraw,
                       int type, int val, int len, char *buf)
{
  t_c2c_peer *cur = find_c2c_peer_with_llid(llid);
  char *locnet = get_cloonix_name();
  if (!cur)
    KERR("ERROR %s %d %s", locnet, len, buf);
  else
    {
    if (llid != cur->pair_llid)
      KERR("ERROR %s %d %s %d %d", locnet, len, buf, llid, cur->pair_llid);
    else if (!msg_exist_channel(llid))
      KERR("ERROR %s %d %s", locnet, len, buf);
    else if (type != doors_type_switch)
      KERR("ERROR %s %d %s", locnet, len, buf);
    else if (val == doors_val_link_ko)
      KERR("ERROR %s %d %s", locnet, len, buf);
    else if (val != doors_val_link_ok)
      {
      sig_process_proxy_peer_data_to_crun(cur->name, len, buf);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int callback_connect(int llid, int fd)
{
  t_c2c_peer *cur = find_c2c_peer_with_listen_llid(llid);
  int pair_llid;
  char *locnet = get_cloonix_name();
  if (!cur)
    KERR("WARNING %s %d", locnet, llid);
  else
    {
    if (cur->pair_llid)
      KERR("WARNING %s %s", locnet, cur->name);
    else
      {
      pair_llid = doorways_sock_client_inet_end(doors_type_switch,
                                                llid, fd,
                                                cur->dist_passwd,
                                                peer_err_cb, peer_rx_cb);
      if (!pair_llid)
        KERR("WARNING %s %s", locnet, cur->name);
      else
        {
        cur->pair_llid = pair_llid;
        if (doorways_sig_bufraw(pair_llid, llid, doors_type_switch,
                                doors_val_init_link, "OK"))
          KERR("WARNING %s %s", locnet, cur->name);
        sig_process_tx_proxy_callback_connect(cur->name, pair_llid);
        if (msg_exist_channel(cur->pair_listen_llid))
          msg_delete_channel(cur->pair_listen_llid);
        cur->pair_listen_llid = 0;
        }
      }
    }
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int c2c_peer_dist_tcp_ip_port(char *name, uint32_t ip, 
                              uint16_t tcp_port, char *dist_passwd)
{
  int llid, result = -1;
  char *locnet = get_cloonix_name();
  t_c2c_peer *cur = find_c2c_peer_with_name(name);
  if (cur && (cur->pair_listen_llid) && (cur->pair_llid))
    KERR("WARNING ALREADY IN PROCESS %s %s %X %hu", locnet, name, ip, tcp_port);
  else
    {
    if (cur)
      free_c2c_peer(cur);
    llid = doorways_sock_client_inet_start(ip, tcp_port, callback_connect);
    if (llid == 0)
      KERR("ERROR %s %s %X %hu", locnet, name, ip, tcp_port);
    else
      {
      result = 0;
      alloc_c2c_peer(llid, name, ip, tcp_port, dist_passwd);
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int c2c_peer_proxy_data_from_crun(char *name, int len, char *buf)
{
  int result = -1;
  char *locnet = get_cloonix_name();
  t_c2c_peer *cur = find_c2c_peer_with_name(name);
  if (!cur)
    KERR("ERROR PEERED NOT FOUND %s %s", locnet, name);
  else if (!msg_exist_channel(cur->pair_llid))
    KERR("ERROR PEERED NOT OK %s %s", locnet, name);
  else 
    {
    if (doorways_tx_bufraw(cur->pair_llid, 0, doors_type_switch,
                                       doors_val_c2c, len, buf))
    KERR("ERROR PEERED TX OK %s %s", locnet, name);
  else
    result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void c2c_peer_init(void)
{
  g_c2c_peer_head = NULL;
}
/*---------------------------------------------------------------------------*/
