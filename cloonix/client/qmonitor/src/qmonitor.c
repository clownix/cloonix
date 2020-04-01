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
#include <pty.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <pthread.h>
#include "io_clownix.h"
#include "util_sock.h"
#include "rpc_qmonitor.h"
#include "file_read_write.h"
#include "doorways_sock.h"

#define MAX_QM_LEN 10000


static pthread_t g_thread;
static pthread_mutex_t g_mutex;
static int g_connect_llid = 0;
static int g_doors_llid = 0;
static char g_buf[MAX_QM_LEN];
static int g_buf_to_send;
static char g_name[MAX_NAME_LEN];
static char g_password[MSG_DIGEST_LEN];
static char g_cloonix_doors[MAX_PATH_LEN];
/*****************************************************************************/

void rpct_recv_kil_req(void *ptr, int llid, int tid) {KOUT();}
void rpct_recv_pid_req(void *ptr, int llid, int tid, char *name, int num) {KOUT();}
void rpct_recv_pid_resp(void *ptr, int llid, int tid, char *name, int num, 
                       int toppid, int pid){KOUT();}
void rpct_recv_hop_sub(void *ptr, int llid, int tid, int flags_hop){KOUT();}
void rpct_recv_hop_unsub(void *ptr, int llid, int tid){KOUT();}
void rpct_recv_hop_msg(void *ptr, int llid, int tid, 
                      int flags_hop, char *txt){KOUT();}
void rpct_recv_app_msg(void *ptr, int llid, int tid, char *line) {KOUT();}
void rpct_recv_diag_msg(void *ptr, int llid, int tid, char *line) {KOUT();}
void rpct_recv_evt_msg(void *ptr, int llid, int tid, char *line) {KOUT();}
void rpct_recv_cli_req(void *ptr, int llid, int tid,
                    int cli_llid, int cli_tid, char *line) {KOUT();}
void rpct_recv_cli_resp(void *ptr, int llid, int tid,
                     int cli_llid, int cli_tid, char *line) {KOUT();}
void rpct_recv_report(void *ptr, int llid, t_blkd_item *item){KOUT(" ");}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void *thread_readline(void *arg)
{
  char *input;
  while (1)
    {
    input = readline("(qemu) ");
    if (!input)
      {
      printf("exit\n");
      exit(-1);
      }
    if (pthread_mutex_lock(&g_mutex) != 0)
      KOUT(" ");
    if (input)
      {
      if (strlen(input))
        add_history(input);
      memset(g_buf, 0, MAX_QM_LEN);
      strncpy(g_buf, input, MAX_QM_LEN-2);
      strcat(g_buf, " \n");
      g_buf_to_send = 1;
      free(input);
      }
    if (pthread_mutex_unlock(&g_mutex) != 0)
      KOUT(" ");
    usleep(20000);
    }
  return NULL;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void heartbeat (int delta)
{
  static int count = 0;
  count++;
  if (count == 500)
    {
    if (!g_doors_llid)
      {
      close(get_fd_with_llid(g_connect_llid));
      doorways_sock_client_inet_delete(g_connect_llid);
      printf("\nTimeout during connect: %s\n\n", g_cloonix_doors);
      exit(-1);
      }
    }
  if(g_buf_to_send)
    {
    if (pthread_mutex_lock(&g_mutex) != 0)
      KOUT(" ");
    if (!strlen(g_buf))
      KOUT(" ");
    send_qmonitor(g_doors_llid, 0, g_name, g_buf);
    g_buf_to_send = 0;
    memset(g_buf, 0, MAX_QM_LEN);
    if (pthread_mutex_unlock(&g_mutex) != 0)
      KOUT(" ");
    } 
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_sub2qmonitor(int llid, int tid, char *name, int on_off)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_qmonitor(int llid, int tid, char *name, char *data)
{
  printf("%s", data);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void err_cb (int llid)
{
  if (g_thread)
    pthread_kill(g_thread, SIGTERM);
  else
    KOUT(" ");
  sleep(1000);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void doorways_client_tx(int llid, int len, char *buf)
{
  doorways_tx(llid, 0, doors_type_switch, doors_val_none, len, buf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void rx_cb(int llid, int tid, int type, int val, int len, char *buf)
{
  if (type == doors_type_switch)
    {
    if (val == doors_val_link_ok)
      {
      }
    else if (val == doors_val_link_ko)
      {
      KERR(" ");
      }
    else
      {
      if (doors_io_qmonitor_decoder(llid, len, buf))
        {
        if (g_thread)
          {
          KERR("%d %s ", len, buf);
          pthread_kill(g_thread, SIGTERM);
          }
        else
          KOUT(" ");
        sleep(1000);
        }
      }
    }
  else
    KOUT("%d", type);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int callback_connect(void *ptr, int llid, int fd)
{
  if (g_doors_llid == 0)
    {
    g_doors_llid = doorways_sock_client_inet_end(doors_type_switch, llid, fd,
                                                 g_password, err_cb, rx_cb);
    if (!g_doors_llid)
      {
      printf("\nConnect not possible: %s\n\n", g_cloonix_doors);
      exit(-1);
      }
    if (doorways_tx(g_doors_llid, 0, doors_type_switch, doors_val_init_link,
                    strlen("OK") , "OK"))
      {
      printf("\nTransmit error: %s\n\n", g_cloonix_doors);
      exit(-1);
      }
    send_sub2qmonitor(g_doors_llid, 0, g_name, 1);
    printf("Requesting subscription to qmonitor for %s to switch.\n", g_name);
    if (pthread_mutex_init(&g_mutex, NULL) != 0)
      KOUT(" ");
    if (pthread_create(&g_thread, NULL, thread_readline, NULL) != 0)
      KOUT(" ");
    }
  else
    KERR("TWO CONNECTS FOR ONE REQUEST");
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int main (int argc, char *argv[])
{
  int switch_ip = 0;
  int switch_port = 0;

  if (argc != 4)
    {
    printf("\n%s 127.0.0.1:43211 clown nemo\n", argv[0]);
    KOUT(" ");
    }
  g_buf_to_send = 0;
  msg_mngt_init("qmonitor", IO_MAX_BUF_LEN);
  doorways_sock_init();
  doors_io_qmonitor_xml_init(doorways_client_tx);
  msg_mngt_heartbeat_init(heartbeat);
  memset(g_cloonix_doors, 0, MAX_PATH_LEN);
  memset(g_password, 0, MSG_DIGEST_LEN);
  memset(g_name, 0, MAX_NAME_LEN);
  strncpy(g_cloonix_doors, argv[1], MAX_PATH_LEN-1);
  strncpy(g_password, argv[2], MSG_DIGEST_LEN-1);
  strncpy(g_name,  argv[3], MAX_NAME_LEN-1);
  doorways_sock_address_detect(g_cloonix_doors, &switch_ip, &switch_port);                           
  g_doors_llid = 0;
  g_connect_llid = doorways_sock_client_inet_start(switch_ip, switch_port,
                                                   callback_connect);
  printf("LOOP\n");
  msg_mngt_loop();
  return 0;
}
/*--------------------------------------------------------------------------*/



