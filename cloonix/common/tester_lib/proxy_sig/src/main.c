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

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doorways_sock.h"
#include "util_sock.h"

#define MAX_TX_BUF 200
#define MAX_TX_LEN 200

static char g_str[MAX_TX_BUF+1][MAX_TX_LEN+1];
static int g_str_full;
static int g_str_idx;
static int g_is_server;
static int g_llid_sig;


/*****************************************************************************/
static int my_rand(int max)
{
  unsigned int result = rand();
  result %= max;
  return (int) result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int rand_tx_buf(void)
{
  int result = my_rand(MAX_TX_BUF);
  if (result == 0)
    result = 1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void random_choice_str(char *name, int max_len)
{
  int i, len = my_rand(max_len-1);
  len += 1;
  memset (name, 0 , max_len);
  for (i=0; i<len; i++)
    name[i] = 'A'+ my_rand(26);
  name[len] = 0;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void get_random_str(void)
{
  int i;
  for (i=0; i<MAX_TX_BUF; i++)
    {
    memset(g_str[i], 0, MAX_TX_LEN);
    random_choice_str(g_str[i], MAX_TX_LEN);
    g_str_full = 1;
    g_str_idx = 0;
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void sig_err_cb (int llid, int err, int from)
{ 
  KOUT("ERROR SIG %d %d %d %d", g_llid_sig, llid, err, from);
} 
/*--------------------------------------------------------------------------*/
  
/****************************************************************************/
static void sig_rx_cb (int llid, int len, char *buf)
{ 
  if ((g_is_server) && (g_llid_sig))
    {
    proxy_sig_tx(g_llid_sig, strlen(buf) + 1, buf);
printf("TX CLIENT %d\n", len);
    }
  else
    {
printf("RX CLIENT %d\n", len);
//    if (strcmp(buf, g_str[g_str_idx]))
//      printf("BAD RX CLIENT \n%s\n%s\n", buf, g_str[g_str_idx]);
//    g_str_idx += 1;
//    if (g_str_idx == MAX_TX_BUF)
//      {
//      g_str_idx = 0;
//      g_str_full = 0;
//      }
    }
} 
/*--------------------------------------------------------------------------*/
 
/****************************************************************************/
static void connect_from_sig_client(int llid, int llid_new)
{
  msg_mngt_set_callbacks (llid_new, sig_err_cb, sig_rx_cb);
  g_llid_sig = llid_new;
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
} 
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void heartbeat (int delta)
{
  int i, max;
  if ((g_is_server == 0) && (g_llid_sig))
    {
    get_random_str();
    max = rand_tx_buf();
    for (i=0; i<max; i++)
      {
      proxy_sig_tx(g_llid_sig, strlen(g_str[i]) + 1, g_str[i]);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int main(int argc, char **argv)
{
  char *sock_path = "/tmp/proxy_sig_tst.sock";
  if (argc != 2)
    {
    printf("\nMUST GIVE ARG -s or -c\n\n");
    exit(-1);
    }
  else if (!strcmp(argv[1],"-s"))
    g_is_server = 1;
  else if (!strcmp(argv[1],"-c"))
    g_is_server = 0;
  else
    {
    printf("\nARG -s or -c\n\n");
    exit(-1);
    }
  g_llid_sig = 0;
  doorways_sock_init(0);
  msg_mngt_init("proxy", IO_MAX_BUF_LEN);
  msg_mngt_heartbeat_init(heartbeat);

  if (g_is_server)
    {
    proxy_sig_server(sock_path, connect_from_sig_client);
    printf("server %s\n", sock_path);
    }
  else
    {
    g_llid_sig = proxy_sig_client(sock_path, sig_err_cb, sig_rx_cb);
    if (g_llid_sig == 0)
      {
      printf("\nBad connection %s\n\n", sock_path);
      exit(-1);
      }
    printf("client %s\n", sock_path);
    }
  msg_mngt_loop();
  return 0; 
}
/*--------------------------------------------------------------------------*/
