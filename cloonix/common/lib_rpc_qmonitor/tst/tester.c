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
#include "rpc_qmonitor.h"

#define IP "127.0.0.1"
#define PORT 0xA658
static int i_am_client = 0;
static int count_qmonitor=0;
static int count_sub2qmonitor=0;



/*****************************************************************************/
static void print_all_count(void)
{
  printf("\n\n");
  printf("START COUNT\n");
  printf("%d\n", count_qmonitor);
  printf("%d\n", count_sub2qmonitor);
  printf("END COUNT\n");
}
/*---------------------------------------------------------------------------*/
static int my_rand(int max)
{
  unsigned int result = rand();
  result %= max;
  return (int) result; 
}
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


/*****************************************************************************/
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
/*---------------------------------------------------------------------------*/

#define MAX_PRINT_LEN 5000

/*****************************************************************************/
void recv_sub2qmonitor(int llid, int itid, char *iname, int ion_off)
{
  static int tid, on_off;
  static char name[MAX_NAME_LEN];
  if (i_am_client)
    {
    if (count_sub2qmonitor)
      {
      if (strcmp(iname, name))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      if (ion_off != on_off)
        KOUT(" ");
      }
    tid = rand();
    on_off = rand();
    random_choice_str(name, MAX_NAME_LEN);
    send_sub2qmonitor(llid, tid, name, on_off);
    }
  else
    send_sub2qmonitor(llid, itid, iname, ion_off);
  count_sub2qmonitor++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_qmonitor(int llid, int itid, char *iname, char *idata)
{
  static int tid;
  static char name[MAX_NAME_LEN];
  static char data[MAX_PRINT_LEN];
  if (i_am_client)
    {
    if (count_qmonitor)
      {
      if (strcmp(iname, name))
        KOUT(" ");
      if (strcmp(idata, idata))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    random_choice_str(name, MAX_NAME_LEN);
    random_choice_str(data, MAX_PRINT_LEN);
    send_qmonitor(llid, tid, name, data);
    }
  else
    send_qmonitor(llid, itid, iname, idata);
  count_qmonitor++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void send_first_burst(int llid)
{
  recv_sub2qmonitor(llid, 0, NULL, 0);
  recv_qmonitor(llid, 0, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void usage(char *name)
{
  printf("\n");
  printf("\n%s -s", name);
  printf("\n%s -c", name);
  printf("\n");
  exit (0);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void err_cb (int llid, int err)
{
  printf("ERROR %d  %d\n", llid, err);
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void rx_cb(int llid, int len, char *buf)
{
  if (doors_io_qmonitor_decoder(llid, len, buf))
    KOUT(" ");
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void connect_from_client(int llid, int llid_new)
{
  printf("%d connect_from_client\n", llid);
  msg_mngt_set_callbacks (llid_new, err_cb, rx_cb);
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
int main (int argc, char *argv[])
{
  int llid, ip, port = PORT;
  if (ip_string_to_int(&ip, IP)) 
    KOUT("%s", IP);
  doors_io_qmonitor_xml_init(string_tx);
  msg_mngt_init(IO_MAX_BUF_LEN);
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



