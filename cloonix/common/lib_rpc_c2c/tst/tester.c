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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include "io_clownix.h"
#include "rpc_c2c.h"

#define IP "127.0.0.1"
#define PORT 0xA658

static int i_am_client = 0;

static int count_peer_create = 0;
static int count_ack_peer_create = 0;
static int count_peer_delete = 0;
static int count_peer_ping = 0;



void rpct_recv_app_msg(void *ptr, int llid, int tid, char *line){KOUT(" ");};
void rpct_recv_diag_msg(void *ptr, int llid, int tid, char *line){KOUT(" ");};
void rpct_recv_evt_msg(void *ptr, int llid, int tid, char *line){KOUT(" ");};
void rpct_recv_cli_req(void *ptr, int llid, int tid,
                    int cli_llid, int cli_tid, char *line){KOUT(" ");};
void rpct_recv_cli_resp(void *ptr, int llid, int tid,
                     int cli_llid, int cli_tid, char *line){KOUT(" ");};
void rpct_recv_kil_req(void *ptr, int llid, int tid){KOUT(" ");};
void rpct_recv_pid_req(void *ptr, int llid, int tid, char *name, int num){KOUT(" ");};
void rpct_recv_pid_resp(void *ptr, int llid, int tid, char *name, int num,
                        int toppid, int pid){KOUT(" ");};
void rpct_recv_hop_sub(void *ptr, int llid, int tid, int flags_hop){KOUT(" ");};
void rpct_recv_hop_unsub(void *ptr, int llid, int tid){KOUT(" ");};
void rpct_recv_hop_msg(void *ptr, int llid, int tid,
                      int flags_hop, char *txt){KOUT(" ");};
void rpct_recv_report(void *ptr, int llid, t_blkd_item *item){KOUT(" ");};



/****************************************************************************/
static void print_all_count(void)
{
  printf("\n\n");
  printf("START COUNT\n");
  printf("%d\n", count_peer_create);
  printf("%d\n", count_ack_peer_create);
  printf("%d\n", count_peer_delete);
  printf("%d\n", count_peer_ping);
  printf("END COUNT\n");
  printf("\n\n");
}
/*--------------------------------------------------------------------------*/
static int my_rand(int max)
{
  unsigned int result = rand();
  result %= max;
  return (int) result; 
}
/****************************************************************************/
static void random_choice_str(char *name, int max_len)
{
  int i, len = my_rand(max_len-1);
  len += 1;
  memset (name, 0 , max_len);
  for (i=0; i<len; i++)
    name[i] = 'A'+ my_rand(26);
  name[len] = 0;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void heartbeat (int delta)
{
  int i;
  unsigned long *mallocs;
  static int count = 0;
  count++;
  if (!(count%1000))
    {
    print_all_count();
    printf("MALLOCS: delta %d \n", delta);
    mallocs = get_clownix_malloc_nb();
    for (i=0; i<MAX_MALLOC_TYPES; i++)
      printf("%d:%02lu ", i, mallocs[i]);
    printf("\n\n");
    }
}
/*--------------------------------------------------------------------------*/

/***************************************************************************/
void recv_c2c_peer_create(int llid, int itid, char *iname,
                          char *icloonix_name, int idoors_idx, 
                          int iip_slave, int iport_slave)
{
  static int tid, doors_idx, ip_slave, port_slave;
  static char name[MAX_NAME_LEN];
  static char cloonix_name[MAX_NAME_LEN];
  if (i_am_client)
    {
    if (count_peer_create)
      {
      if (strcmp(icloonix_name, cloonix_name))
        KOUT(" ");
      if (strcmp(iname, name))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      if (idoors_idx != doors_idx)
        KOUT(" ");
      if (iip_slave != ip_slave)
        KOUT(" ");
      if (iport_slave != port_slave)
        KOUT(" ");
      }
    tid = rand();
    doors_idx = rand();
    ip_slave = rand();
    port_slave = rand();
    random_choice_str(name, MAX_NAME_LEN);
    random_choice_str(cloonix_name, MAX_NAME_LEN);
    send_c2c_peer_create(llid, tid, name, cloonix_name, doors_idx, 
                         ip_slave, port_slave);
    }
  else
    send_c2c_peer_create(llid, itid, iname, icloonix_name, idoors_idx,
                         iip_slave, iport_slave);
  count_peer_create++;
}
/*--------------------------------------------------------------------------*/

/***************************************************************************/
void recv_c2c_ack_peer_create(int llid, int itid, char *iname,
                                    char *iorig_cloonix_name,
                                    char *ipeer_cloonix_name,
                                    int iorig_doors_idx,
                                    int ipeer_doors_idx,
                                    int istatus)
{
  static int tid;
  static int status;
  static char name[MAX_NAME_LEN];
  static int orig_doors_idx;
  static int peer_doors_idx;
  static char orig_cloonix_name[MAX_NAME_LEN];
  static char peer_cloonix_name[MAX_NAME_LEN];
  if (i_am_client)
    {
    if (count_ack_peer_create)
      {
      if (strcmp(iorig_cloonix_name, orig_cloonix_name))
        KOUT(" ");
      if (strcmp(ipeer_cloonix_name, peer_cloonix_name))
        KOUT(" ");
      if (strcmp(iname, name))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      if (istatus != status)
        KOUT(" ");
      if (iorig_doors_idx != orig_doors_idx)
        KOUT(" ");
      if (ipeer_doors_idx != peer_doors_idx)
        KOUT(" ");
      }
    tid = rand();
    status = rand();
    random_choice_str(name, MAX_NAME_LEN);
    orig_doors_idx = rand();
    peer_doors_idx = rand();
    random_choice_str(orig_cloonix_name, MAX_NAME_LEN);
    random_choice_str(peer_cloonix_name, MAX_NAME_LEN);
    send_c2c_ack_peer_create(llid, tid, name,
                                   orig_cloonix_name, peer_cloonix_name,
                                   orig_doors_idx, peer_doors_idx, status);
    }
  else
    send_c2c_ack_peer_create(llid, itid, iname,
                                   iorig_cloonix_name, ipeer_cloonix_name,
                                   iorig_doors_idx, ipeer_doors_idx, istatus);
  count_ack_peer_create++;
}
/*--------------------------------------------------------------------------*/

/***************************************************************************/
void recv_c2c_peer_ping(int llid, int itid, char *iname)
{
  static int tid;
  static char name[MAX_NAME_LEN];
  if (i_am_client)
    {
    if (count_peer_ping)
      {
      if (strcmp(iname, name))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    random_choice_str(name, MAX_NAME_LEN);
    send_c2c_peer_ping(llid, tid, name);
    }
  else
    send_c2c_peer_ping(llid, itid, iname);
  count_peer_ping++;
}
/*--------------------------------------------------------------------------*/




/***************************************************************************/
void recv_c2c_peer_delete(int llid, int itid, char *iname)
{
  static int tid;
  static char name[MAX_NAME_LEN];
  if (i_am_client)
    {
    if (count_peer_delete)
      {
      if (strcmp(iname, name))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    random_choice_str(name, MAX_NAME_LEN);
    send_c2c_peer_delete(llid, tid, name);
    }
  else
    send_c2c_peer_delete(llid, itid, iname);
  count_peer_delete++;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void send_first_burst(int llid)
{
  recv_c2c_peer_create(llid, 0, NULL, NULL, 0, 0, 0);
  recv_c2c_ack_peer_create(llid, 0, NULL, NULL, NULL, 0, 0, 0);
  recv_c2c_peer_delete(llid, 0, NULL);
  recv_c2c_peer_ping(llid, 0, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void usage(char *name)
{
  printf("\n");
  printf("\n%s -s", name);
  printf("\n%s -c", name);
  printf("\n");
  exit (0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void err_cb (void *ptr, int llid, int err, int from)
{
  (void) ptr;
  printf("ERROR %d  %d %d\n", llid, err, from);
  KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_cb(int llid, int len, char *buf)
{
  if (doors_io_c2c_decoder(llid, len, buf))
    KOUT(" ");
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void connect_from_client(void *ptr, int llid, int llid_new)
{
  (void) ptr;
  printf("%d connect_from_client\n", llid);
  msg_mngt_set_callbacks (llid_new, err_cb, rx_cb);
}
/*--------------------------------------------------------------------------*/



/****************************************************************************/
int main (int argc, char *argv[])
{
  int llid, ip, port = PORT;
  if (ip_string_to_int(&ip, IP)) 
    KOUT("%s", IP);
  doors_io_c2c_init(string_tx);
  msg_mngt_init("tst", IO_MAX_BUF_LEN);
  if (argc == 2)
    {
    if (!strcmp(argv[1], "-s"))
      {
      msg_mngt_heartbeat_init(heartbeat);
      string_server_inet(port, connect_from_client, "test");
      printf("\n\n\nSERVER\n\n");
      }
    else if (!strcmp(argv[1], "-c"))
      {
      i_am_client = 1;
      msg_mngt_heartbeat_init(heartbeat);
      llid = string_client_inet(ip, port, err_cb, rx_cb, "test");
      send_first_burst(llid);
      }
    else
      usage(argv[0]);
    }
  else
    usage(argv[0]);
  msg_mngt_loop();
  return 0;
}
/*--------------------------------------------------------------------------*/



