/*****************************************************************************/
/*    Copyright (C) 2006-2021 clownix@clownix.net License AGPL-3             */
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
#include <math.h>
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doors_rpc.h"

#define SAMPLE_CLOWNIX_SERVER "/tmp/test_doors"

static int i_am_client        = 0;
static int count_info        = 0;
static int count_event        = 0;
static int count_command      = 0;
static int count_status       = 0;
static int count_add_vm       = 0;
static int count_del_vm       = 0;



/*****************************************************************************/
static void print_all_count(void)
{
  printf("\n\n");
  printf("START COUNT\n");
  printf("%d\n", count_info);
  printf("%d\n", count_event);
  printf("%d\n", count_command);
  printf("%d\n", count_status);
  printf("%d\n", count_add_vm);
  printf("%d\n", count_del_vm);
  printf("END COUNT\n");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void random_choice_str(char *name, int max_len)
{
  int i, len = rand() % (max_len-2);
  len += 1;
  memset (name, 0 , max_len);
  for (i=0; i<len; i++)
    {
    do 
      {
      name[i] = (char )(rand());
      } while((name[i] < 0x41) || (name[i] > 0x5A));
    }
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
    printf("MALLOCS: ");
    mallocs = get_clownix_malloc_nb();
    for (i=0; i<MAX_MALLOC_TYPES; i++)
      printf("%d:%02lu ", i, mallocs[i]);
    print_all_count();
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void doors_recv_info(int llid, int itid, char *itype,  char *iinfo)
{
  static char info[MAX_PRINT_LEN];
  static char type[MAX_NAME_LEN];
  static int tid;
  if (i_am_client)
    {
    if (count_info)
      {
      if (strcmp(itype, type))
        KOUT(" ");
      if (strcmp(iinfo, info))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    random_choice_str(info, MAX_PRINT_LEN);
    random_choice_str(type, MAX_NAME_LEN);
    doors_send_info(llid, tid, type, info);
    }
  else
    doors_send_info(llid, itid, itype, iinfo);
  count_info++;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void doors_recv_event(int llid, int itid, char *iname,  char *iline)
{
  static char line[MAX_PRINT_LEN];
  static char name[MAX_NAME_LEN];
  static int tid;
  if (i_am_client)
    {
    if (count_event)
      {
      if (strcmp(iline, line))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    random_choice_str(line, MAX_PRINT_LEN);
    random_choice_str(name, MAX_NAME_LEN);
    doors_send_event(llid, tid, name, line);
    }
  else
    doors_send_event(llid, itid, iname, iline);
  count_event++;
}
/*--------------------------------------------------------------------------*/



/****************************************************************************/
void doors_recv_command(int llid, int itid, char *iname, char *icmd)
{
  static char cmd[MAX_PRINT_LEN];
  static char name[MAX_NAME_LEN];
  static int tid;
  if (i_am_client)
    {
    if (count_command)
      {
      if (strcmp(cmd, icmd))
        KOUT(" ");
      if (strcmp(name, iname))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    random_choice_str(cmd, MAX_PRINT_LEN);
    random_choice_str(name, MAX_NAME_LEN);
    doors_send_command(llid, tid, name, cmd);
    }
  else
    doors_send_command(llid, itid, iname, icmd);
  count_command++;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void doors_recv_status(int llid, int itid, char *iname, char *icmd)
{
  static char cmd[MAX_PRINT_LEN];
  static char name[MAX_NAME_LEN];
  static int tid;
  if (i_am_client)
    {
    if (count_status)
      {
      if (strcmp(cmd, icmd))
        KOUT(" ");
      if (strcmp(name, iname))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    random_choice_str(cmd, MAX_PRINT_LEN);
    random_choice_str(name, MAX_NAME_LEN);
    doors_send_status(llid, tid, name, cmd);
    }
  else
    doors_send_status(llid, itid, iname, icmd);
  count_status++;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void doors_recv_add_vm(int llid, int itid, char *iname, char *iway2agent)
{
  static char way2agent[MAX_PATH_LEN];
  static char name[MAX_NAME_LEN];
  static int tid;
  if (i_am_client)
    {
    if (count_add_vm)
      {
      if (strcmp(way2agent, iway2agent))
        KOUT(" ");
      if (strcmp(name, iname))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    random_choice_str(way2agent, MAX_PATH_LEN);
    random_choice_str(name, MAX_NAME_LEN);
    doors_send_add_vm(llid, tid, name, way2agent);
    }
  else
    doors_send_add_vm(llid, itid, iname, iway2agent);
  count_add_vm++;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void doors_recv_del_vm(int llid, int itid, char *iname)
{
  static char name[MAX_NAME_LEN];
  static int tid;
  if (i_am_client)
    {
    if (count_del_vm)
      {
      if (strcmp(name, iname))
        KOUT(" ");
      if (itid != tid)
        KOUT(" ");
      }
    tid = rand();
    random_choice_str(name, MAX_NAME_LEN);
    doors_send_del_vm(llid, tid, name);
    }
  else
    doors_send_del_vm(llid, itid, iname);
  count_del_vm++;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void send_first_burst(int llid)
{
  doors_recv_info(llid, 0, NULL, NULL);
  doors_recv_event(llid, 0, NULL, NULL);
  doors_recv_command(llid, 0, NULL, NULL);
  doors_recv_status(llid, 0, NULL, NULL);
  doors_recv_add_vm(llid, 0, NULL, NULL);
  doors_recv_del_vm(llid, 0, NULL);
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
static void err_cb (void *ptr, int llid, int err, int from)
{
  KOUT("ERROR %d  %d\n", llid, err);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void rx_cb (int llid, int len, char *buf)
{
  if (doors_xml_decoder(llid, len, buf))
    KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void connect_from_client(void *ptr, int llid, int llid_new)
{
  msg_mngt_set_callbacks (llid_new, err_cb, rx_cb);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
int main (int argc, char *argv[])
{
  int llid;
  doors_xml_init();
  msg_mngt_init("doors", IO_MAX_BUF_LEN);
  if (argc == 2)
    {
    if (!strcmp(argv[1], "-s"))
      {
      unlink(SAMPLE_CLOWNIX_SERVER);
      msg_mngt_heartbeat_init(heartbeat);
      string_server_unix(SAMPLE_CLOWNIX_SERVER, connect_from_client, "o");
      printf("\n\n\nSERVER\n\n");
      }
    else if (!strcmp(argv[1], "-c"))
      {
      i_am_client = 1;
      msg_mngt_heartbeat_init(heartbeat);
      llid = string_client_unix(SAMPLE_CLOWNIX_SERVER, err_cb, rx_cb, "o");
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



