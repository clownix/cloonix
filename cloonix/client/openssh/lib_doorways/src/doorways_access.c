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
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "io_clownix.h"
#include "util_sock.h"
#include "doorways_sock.h"
#include "lib_doorways.h"

#define MAX_CLI_RX 50000

enum {
  state_none = 0,
  state_waiting_first_read,
  state_waiting_connection,
  state_nominal_exchange
};

static int g_state;
static int g_offset_first_read;
static char *g_first_read;
static char g_buf[MAX_CLI_RX];
static int  g_door_llid;
static int  g_cli_llid;
static int  g_connect_llid;
static char g_cloonix_passwd[MSG_DIGEST_LEN+1];
static char g_cloonix_doors[MAX_PATH_LEN];
static char g_cloonix_nat[MAX_NAME_LEN];
static char g_address_in_vm[MAX_PATH_LEN];
static char g_sockcli[MAX_PATH_LEN];

int get_ip_port_from_path(char *param, uint32_t *ip, int *port);


/*****************************************************************************/
static void change_state(int state)
{
//  KERR("STATE: %d --> %d", g_state, state);
  g_state = state;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void local_exit(int val)
{
  unlink(g_sockcli);
  exit(val);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void heartbeat(int delta)
{
  static int count = 0;
  (void) delta;
  count++;
  if (count == 600)
    {
    if (!g_door_llid)
      {
      close(get_fd_with_llid(g_connect_llid));
      doorways_sock_client_inet_delete(g_connect_llid);
      fprintf(stderr, "\nTimeout during connect: %s\n\n", g_cloonix_doors);
      local_exit(1);
      }
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void tx_doorways(int len, char *buf)
{
  if (!msg_exist_channel(g_door_llid))
    {
    fprintf(stderr, "ERROR TX, llid dead\n%d\n", len);
    local_exit(1);
    }
  if (doorways_tx(g_door_llid,0,doors_type_openssh,doors_val_none,len,buf))
    {
    fprintf(stderr, "ERROR TX, bad tx:\n%d\n", len);
    local_exit(1);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void rx_doorways(int len, char *buf)
{
  if (!msg_exist_channel(g_cli_llid))
    {
    fprintf(stderr, "ERROR g_cli_llid: %d\n", len);
    local_exit(1);
    }
  watch_tx(g_cli_llid, len, buf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void send_first_read_residual_data(void)
{
  int len_done, len_left_todo;
  char *ptr;
  if (!g_first_read)
    {
    fprintf(stderr, "FATAL ERROR\n");
    local_exit(1);
    }

  ptr = strstr(g_first_read, "cloonix_info_end");
  ptr = ptr + strlen("cloonix_info_end");
  len_done = (int) (ptr - g_first_read);
  len_left_todo = g_offset_first_read - len_done;
  if (len_left_todo < 0)
    {
    fprintf(stderr, "Wrong len %d %d", len_done, g_offset_first_read);
    local_exit(1);
    }
  tx_doorways(len_left_todo, ptr);
  free(g_first_read);
  g_first_read = NULL;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void cb_doors_rx(int llid, int tid, int type, int val, 
                        int len, char *buf)
{
  char nat_name[MAX_NAME_LEN];
  char *nat_msg = g_address_in_vm;
  (void) llid;
  (void) tid;
  if (type == doors_type_openssh)
    {
    if ((val == doors_val_link_ok) || (val == doors_val_link_ko))
      {
      if (sscanf(buf,"OPENSSH_DOORWAYS_RESP nat=%s", nat_name) == 1)
        {
        if (doorways_tx(g_door_llid, 0, doors_type_openssh,
                        doors_val_none, strlen(nat_msg)+1, nat_msg))
          {
          local_exit(1);
          }
        }
      else
        {
        local_exit(1);
        }
      }
    else if (val == doors_val_none)
      {
      rx_doorways(len, buf);
      }
    else if (val == doors_val_sig)
      {
      if (!strcmp(buf, "UNIX2INET_ACK_OPEN"))
        {
        send_first_read_residual_data();
        change_state(state_nominal_exchange);
        }
      }
    else
      {
      local_exit(1);
      }
    }
  else
    {
    local_exit(1);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void cb_doors_end(int llid)
{
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
  local_exit(0);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int callback_connect(void *ptr, int llid, int fd)
{
  char buf[2*MAX_NAME_LEN];
  (void) ptr;
  if (g_door_llid == 0)
    {
    g_door_llid = doorways_sock_client_inet_end(doors_type_openssh, llid, fd,
                                                g_cloonix_passwd,
                                                cb_doors_end, cb_doors_rx);
    if (!g_door_llid)
      {
      local_exit(1);
      }
    if (!msg_exist_channel(g_door_llid))
      {
      local_exit(1);
      }
    memset(buf, 0, 2*MAX_NAME_LEN);
    snprintf(buf, 2*MAX_NAME_LEN - 1, "OPENSSH_DOORWAYS_REQ nat=%s",
             g_cloonix_nat);
    if (doorways_tx(g_door_llid, 0, doors_type_openssh,
                    doors_val_init_link, strlen(buf)+1, buf))
      {
      local_exit(1);
      }
    }
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int cloonix_connect_remote(char *cloonix_doors)
{
  uint32_t ip;
  int port;
  if (get_ip_port_from_path(cloonix_doors, &ip, &port) == -1)
    {
    fprintf(stderr, "\nBad address %s\n\n", cloonix_doors);
    local_exit(1);
    }
  g_door_llid = 0;
  g_connect_llid = doorways_sock_client_inet_start(ip,port,callback_connect);
  if (!g_connect_llid)
    {
    fprintf(stderr, "\nCannot reach doorways %s\n\n", cloonix_doors);
    local_exit(1);
    }
  return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int input_extract_init(char *inputs)
{
  int result = -1;
  uint32_t ip;
  int port;
  char *ptr;
  memset(g_cloonix_passwd, 0, MSG_DIGEST_LEN+1);
  memset(g_cloonix_doors, 0, MAX_PATH_LEN);
  memset(g_cloonix_nat, 0, MAX_NAME_LEN);
  memset(g_address_in_vm, 0, MAX_PATH_LEN);

  ptr = strchr(inputs, '=');
  if (!ptr)
    fprintf(stderr, "BAD INPUT1 %s", inputs);
  else
    { 
    ptr += 1;
    ptr = strchr(ptr, '=');
    if (!ptr)
      fprintf(stderr, "BAD INPUT11 %s", inputs);
    else
      { 
      *ptr = ' ';
      ptr = strchr(ptr, '=');
      if (!ptr)
        fprintf(stderr, "BAD INPUT111 %s", inputs);
      else
        {
        *ptr = ' ';
        ptr = strchr(ptr, '@');
        if (!ptr)
          fprintf(stderr, "BAD INPUT2 %s", inputs);
        else
          {
          *ptr = ' ';
          if (sscanf(inputs, "%s %s %s %s", g_cloonix_doors,
                                            g_cloonix_passwd,
                                            g_cloonix_nat,
                                            g_address_in_vm) != 4)
            fprintf(stderr, "BAD INPUT3 %s", inputs);
          else
            {
            if (get_ip_port_from_path(g_cloonix_doors, &ip, &port))
              fprintf(stderr, "BAD INPUT4 %s", inputs);
            else
              {
              ptr = strstr(g_address_in_vm, "cloonix_info_end");
              if (!ptr)
                fprintf(stderr, "BAD INPUT5 %s", inputs);
              else
                {
                ptr = ptr + strlen("cloonix_info_end");
                *ptr = 0;
                result = 0;
                }
              }
            }
          }
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void first_read_process(int len, char *buf)
{
  char *ptr;
  if ((g_offset_first_read + len) >= (2*MAX_CLI_RX - 1))
    {
    fprintf(stderr, "Bad first read %d %d", len, g_offset_first_read);
    local_exit(1);
    }
  memcpy(g_first_read+g_offset_first_read, g_buf, len);
  g_offset_first_read += len;
  ptr = strstr(g_first_read, "cloonix_info_end");
  if ((ptr) && (g_state == state_waiting_first_read))
    {
    if (input_extract_init(g_first_read))
      local_exit(1);
    change_state(state_waiting_connection);
    doorways_sock_init();
    cloonix_connect_remote(g_cloonix_doors);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void err_cli_cb (void *ptr, int llid, int err, int from)
{
  local_exit(1);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int rx_cli_cb(void *ptr, int llid, int fd)
{
  int len = read(fd, g_buf, MAX_CLI_RX);
  if (len == 0)
    {
    fprintf(stderr, "Client bad read 0 len %d", errno);
    local_exit(1);
    }
  else if (len < 0)
    {
    if ((errno != EINTR) && (errno != EWOULDBLOCK) && (errno != EAGAIN))
      {
      fprintf(stderr, "Client bad read %d", errno);
      local_exit(1);
      }
    }
  else
    {
    if (g_state == state_nominal_exchange)
      tx_doorways(len, g_buf);
    else
      first_read_process(len, g_buf);
    }
  return len;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int listen_event_cb(void *ptr, int llid, int fd)
{
  int cli_fd;
  util_fd_accept(fd, &cli_fd, __FUNCTION__);
  if (cli_fd < 0)
    {
    fprintf(stderr, "BAD cli_fd");
    local_exit(1);
    }
  else
    {
    g_cli_llid = msg_watch_fd(cli_fd, rx_cli_cb, err_cli_cb, "door2ag");
    if (g_cli_llid <= 0)
      {
      fprintf(stderr, "BAD g_cli_llid");
      local_exit(1);
      }
    if (msg_exist_channel(llid))
      msg_delete_channel(llid);
    }
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void listen_err_cb(void *ptr, int llid, int err, int from)
{
  fprintf(stderr, "Listen channel error");
  local_exit(1);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int unix_socket_create(char *sockcli)
{
  int fd, llid, result = -1;
  fd = util_socket_listen_unix(sockcli);
  if (fd < 0)
    fprintf(stderr, "BAD fd %s", sockcli);
  else
    {
    llid = msg_watch_fd(fd, listen_event_cb, listen_err_cb, "sockcli");
    if (llid <= 0)
      fprintf(stderr, "BAD watch %s", sockcli);
    else
      result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void doorways_access_init(char *sockcli)
{
  memset(g_sockcli, 0, MAX_PATH_LEN);
  strncpy(g_sockcli, sockcli, MAX_PATH_LEN-1);
  change_state(state_waiting_first_read);
  g_offset_first_read = 0;
  g_first_read = malloc(2*MAX_CLI_RX);
  memset(g_first_read, 0, 2*MAX_CLI_RX);
  msg_mngt_init("openssh", IO_MAX_BUF_LEN);
  msg_mngt_heartbeat_init(heartbeat);
  if (!unix_socket_create(sockcli))
    msg_mngt_loop();
}
/*--------------------------------------------------------------------------*/

