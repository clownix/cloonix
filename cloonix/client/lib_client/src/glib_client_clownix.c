/*****************************************************************************/
/*    Copyright (C) 2006-2024 clownix@clownix.net License AGPL-3             */
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
#ifdef WITH_GLIB
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doorways_sock.h"
#include "client_clownix.h"
#include "pid_clone.h"


static GIOChannel* g_clone_io_channel;
static guint g_clone_tag;
static int g_clone_fd_request;
static int g_clone_fd_ack;


typedef gboolean (*t_glib_to_llid_cb)  (GIOChannel   *source,
                                        GIOCondition  condition,
                                        gpointer data);
struct t_glib_to_llid_list;

void pid_clone_harvest_death(void);

/*--------------------------------------------------------------------------*/
typedef struct t_glib_to_llid
  {
  GIOChannel *g_io_channel;
  t_fd_event glibtx;
  t_fd_event glibrx;
  int fd;
  int tag;
  int tag_is_tx;
  int llid;
  int fail_count;
  struct t_glib_to_llid_list *lst; 
  } t_glib_to_llid;
/*--------------------------------------------------------------------------*/
typedef struct t_glib_to_llid_list
{
  t_glib_to_llid gl;
  struct t_glib_to_llid_list *prev;
  struct t_glib_to_llid_list *next;
} t_glib_to_llid_list;
/*--------------------------------------------------------------------------*/
typedef struct t_glib_to_llid_listen
  {
  GIOChannel *g_io_channel;
  t_fd_event_glib gliblisten;
  t_fd_event glibtx;
  t_fd_event glibrx;
  t_msg_rx_cb client_cb;
  int fd;
  int tag;
  int llid;
  } t_glib_to_llid_listen;
/*--------------------------------------------------------------------------*/
static t_glib_to_llid g_glt;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void main_err_cb (int llid)
{
  pid_clone_kill_all();
  printf("\nCLOSED CONNECTION\n\n");
  exit(1);
}
/*--------------------------------------------------------------------------*/



/****************************************************************************/
static void clean_llid(t_glib_to_llid *gtl)
{
  if (gtl->tag)
    {
    g_source_remove(gtl->tag);
    gtl->tag = 0;
    g_io_channel_shutdown(gtl->g_io_channel, FALSE, NULL);
    }
  if (msg_exist_channel(gtl->llid))
    {
    msg_delete_channel(gtl->llid);
    }
  memset (gtl, 0, sizeof(t_glib_to_llid));
}
/*--------------------------------------------------------------------------*/



/****************************************************************************/
static gboolean glib_cb (GIOChannel   *source,
                         GIOCondition  condition,
                         gpointer data)
{
  t_glib_to_llid *gtl = (t_glib_to_llid *) data;
  if (!msg_exist_channel(gtl->llid))
    return FALSE;
  if (source != gtl->g_io_channel)
    return FALSE;
  gtl->tag = 0;
  if (condition & G_IO_OUT)
    gtl->glibtx(gtl->llid, gtl->fd);
  else if (condition & G_IO_IN)
    {
    if (!gtl->glibrx(gtl->llid, gtl->fd))
      {
      gtl->fail_count++;
      if (gtl->fail_count >= 5)
        {
        KERR("ERROR glib RX KILL ALL");
        pid_clone_kill_all();
        exit(-1);
        }
      }
    else
      gtl->fail_count = 0;
    }
  else
    KOUT("%X\n", condition);
  if (gtl->llid) 
    glib_prepare_rx_tx(gtl->llid);
  return FALSE;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_glib_to_llid *llid_to_gtl(int llid)
{
  t_glib_to_llid *gtl = NULL;
  if (llid == g_glt.llid)
    gtl = &g_glt;
  else 
    KOUT(" ");
  if (gtl == NULL)
    {
    if (msg_exist_channel(llid))
      msg_delete_channel(llid);
    }
  else if (!msg_exist_channel(gtl->llid))
    {
    clean_llid(gtl);
    gtl = NULL;
    }
  return gtl;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void glib_prepare_rx_tx(int llid)
{
  t_glib_to_llid *gtl = llid_to_gtl (llid);
  if (gtl)
    {
    if (gtl->tag)
      {
      g_source_remove (gtl->tag);
      gtl->tag = 0;
      }
    if (msg_mngt_get_tx_queue_len (gtl->llid))
      {
      gtl->tag_is_tx += 1;
      gtl->tag = g_io_add_watch (gtl->g_io_channel, G_IO_OUT, glib_cb, gtl);
      }
    else
      {
      gtl->tag_is_tx = 0;
      gtl->tag = g_io_add_watch (gtl->g_io_channel, G_IO_IN, glib_cb, gtl);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void glib_clean_llid(int llid)
{
  t_glib_to_llid *gtl = llid_to_gtl(llid);
  if (gtl)
    clean_llid(gtl);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int glib_connect_llid(int llid, int fd, t_doorways_rx cb, char *passwd)
{
  t_glib_to_llid *gtl = &g_glt;
  doorways_sock_client_inet_delete(llid);
  gtl->llid = doorways_sock_client_inet_end_glib(doors_type_switch, fd,
                                                 passwd, main_err_cb, cb,
                                                 &(gtl->glibrx), 
                                                 &(gtl->glibtx));
  if (!gtl->llid)
    {
    printf("FAILED TO CONNECT!\n\n");
    KOUT(" ");
    }
  gtl->fd = fd;
  gtl->g_io_channel = g_io_channel_unix_new(gtl->fd);
  glib_prepare_rx_tx(gtl->llid);
  return gtl->llid;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static gboolean glib_clone_cb (GIOChannel   *source,
                               GIOCondition  condition,
                               gpointer data)
{
  int rx_len;
  char buf[10];
  if (source != g_clone_io_channel)
    {
    KERR("ERROR glib");
    return FALSE;
    }
  if (condition & G_IO_IN)
    {
    rx_len = read (g_clone_fd_ack, buf, 10);
    if (rx_len < 0)
      {
      if ((errno != EAGAIN) && (errno != EINTR))
        KERR("ERROR %d", errno);
      }
    else if (rx_len == 0)
      KERR("ERROR CLOSE");
    else
      pid_clone_harvest_death();
    }
  else
    KOUT("ERROR %X\n", condition);
  g_source_remove (g_clone_tag);
  g_clone_tag = g_io_add_watch(g_clone_io_channel,G_IO_IN,glib_clone_cb,NULL);
  return FALSE;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void glib_pid_clone_init(int fd_request, int fd_ack)
{
  g_clone_fd_request = fd_request;
  g_clone_fd_ack = fd_ack;
  g_clone_io_channel = g_io_channel_unix_new(g_clone_fd_ack);
  g_clone_tag = g_io_add_watch(g_clone_io_channel, G_IO_IN, glib_clone_cb, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void glib_client_init(void)
{
  memset (&g_glt, 0, sizeof(t_glib_to_llid));
}
#endif
/*--------------------------------------------------------------------------*/
