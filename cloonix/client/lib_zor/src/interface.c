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
#include <glib-2.0/glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/socket.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doorways_sock.h"
#include "cloonix_conf_info.h"
#include "connect_cloonix.h"
#include "interface.h"
#include "util_sock.h"



/*--------------------------------------------------------------------------*/
typedef struct t_cloonix
  {
  int doors_llid;
  int doors_fd;
  int connect_fd;
  int abnormal;
  char name[MAX_NAME_LEN];
  char path[MAX_PATH_LEN];
  char passwd[MSG_DIGEST_LEN];
  char work_dir[MAX_PATH_LEN];
  t_fd_event glibtx;
  t_fd_event glibrx;
  GIOChannel *g_io_connect_channel;
  int connect_tag;
  GIOChannel *g_io_channel;
  int tag;
  int tag_is_tx;
  struct t_cloonix *prev;
  struct t_cloonix *next;
  } t_cloonix;
/*--------------------------------------------------------------------------*/
static t_cloonix *g_head_cloonix;
/*--------------------------------------------------------------------------*/
static t_net_exists_cb      g_net_exists; 
/*--------------------------------------------------------------------------*/
static gboolean glib_cb (GIOChannel   *source,
                         GIOCondition  condition,
                         gpointer data);
/*--------------------------------------------------------------------------*/
static void connect_llid_del(t_cloonix *cloonix);
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_cloonix *find_cloonix_with_fd(int fd)
{
  t_cloonix *cur = g_head_cloonix;
  while(cur)
    {
    if ((cur->doors_fd == fd) || (cur->connect_fd == fd))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_cloonix *find_cloonix_with_doors_llid(int llid)
{
  t_cloonix *cur = g_head_cloonix;
  while(cur)
    {
    if (cur->doors_llid == llid)
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_cloonix *find_cloonix_with_name(char *net_name)
{
  t_cloonix *cur = g_head_cloonix;
  while(cur)
    {
    if (!strcmp(cur->name, net_name))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void glib_update_watch(t_cloonix *cloonix)
{
  if (cloonix->tag)
    {
    g_source_remove (cloonix->tag);
    cloonix->tag = 0;
    }
  if (msg_mngt_get_tx_queue_len(cloonix->doors_llid))
    {
    cloonix->tag_is_tx += 1;
    cloonix->tag = g_io_add_watch (cloonix->g_io_channel, 
                                   G_IO_OUT, glib_cb, cloonix);
    }
  else
    {
    cloonix->tag_is_tx = 0;
    cloonix->tag = g_io_add_watch (cloonix->g_io_channel, 
                                   G_IO_IN, glib_cb, cloonix);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void cloonix_alloc(char *name, char *path, char *passwd) 
{ 
  t_cloonix *cur;
  cur = (t_cloonix *)clownix_malloc(sizeof(t_cloonix), 5);
  memset(cur, 0, sizeof(t_cloonix));
  cur->connect_fd = -1;
  cur->doors_fd = -1;
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  strncpy(cur->path, path, MAX_PATH_LEN-1);
  strncpy(cur->passwd, passwd, MSG_DIGEST_LEN-1);
  if (g_head_cloonix)
    g_head_cloonix->prev = cur;
  cur->next = g_head_cloonix;
  g_head_cloonix = cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void doors_llid_attach(t_cloonix *cloonix, int llid,
                              t_fd_event rx_glib, t_fd_event tx_glib)
{
  if (cloonix->doors_fd != -1)
    KOUT(" ");
  cloonix->doors_fd = cloonix->connect_fd;
  cloonix->connect_fd = -1;
  cloonix->g_io_channel = g_io_channel_unix_new(cloonix->doors_fd);
  cloonix->doors_llid = llid;
  cloonix->glibtx = tx_glib;
  cloonix->glibrx = rx_glib;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void doors_llid_detach(t_cloonix *cloonix)
{
  if (cloonix->tag)
    g_source_remove(cloonix->tag);
  if (cloonix->g_io_channel)
    {
    g_io_channel_shutdown(cloonix->g_io_channel, FALSE, NULL);
    cloonix->g_io_channel = NULL;
    }
  cloonix->tag = 0;
  cloonix->glibtx = NULL;
  cloonix->glibrx = NULL;
  if (msg_exist_channel(cloonix->doors_llid))
    msg_delete_channel(cloonix->doors_llid);
  g_net_exists(cloonix->name, cloonix->doors_llid, 0); 
  cloonix->doors_llid = 0;
  cloonix->doors_fd = -1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void doorways_client_end(int llid)
{
  t_cloonix *cloonix = find_cloonix_with_doors_llid(llid);
  if (!cloonix)
    KERR("%d", llid);
  else
    {
    doors_llid_detach(cloonix);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void doorways_client_tx(int llid, int len, char *buf)
{
  t_cloonix *cloonix = find_cloonix_with_doors_llid(llid);
  if (!cloonix)
    KERR("%d %d", llid, len);
  else
    {
    doorways_tx(llid, 0, doors_type_switch, doors_val_none, len, buf);
    glib_update_watch(cloonix);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void try_repairing_abnormal(int llid)
{
  t_cloonix *cloonix;
  KERR(" REPAIR TOTALY ABNORMAL %d", llid);
  cloonix = find_cloonix_with_doors_llid(llid);
  if (!cloonix)
    {
    KERR(" TOTALY ABNORMAL NO RECORD %d", llid);
    if (msg_exist_channel(llid))
      {
      KERR(" TOTALY ABNORMAL DELETE LLID %d", llid);
      msg_delete_channel(llid);
      }
    }
  else
    {
    KERR(" TOTALY ABNORMAL RECORD PRESENT %d", llid);
    cloonix->abnormal = 1;;
    if (!msg_exist_channel(cloonix->doors_llid))
      {
      KERR("REPAIR ABNORMAL: %s %d", cloonix->name, cloonix->doors_llid);
      doors_llid_detach(cloonix);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void doorways_client_rx(int llid, int tid, int type, int val,
                               int len, char *buf)
{
  t_cloonix *cloonix;
  cloonix = find_cloonix_with_doors_llid(llid);
  if (!cloonix)
    KERR(" ");
  else
    {
    if (type == doors_type_switch)
      {
      if (val == doors_val_init_link)
        {
        KERR(" TOTALY ABNORMAL %d %d %s", llid, len, buf);
        try_repairing_abnormal(llid);
        }
      else if (val == doors_val_link_ok)
        {
        }
      else if (val == doors_val_link_ko)
        {
        KERR(" ");
        }
      else
        {
        if (cloonix->abnormal)
          KERR(" RE-TOTALY ABNORMAL %d %d %s", llid, len, buf);
        else
          {
          if (doors_io_basic_decoder(llid, len, buf))
            {
            if (rpct_decoder(NULL, llid, len, buf))
              KOUT("%s", buf);
            }
          }
        }
      }
    else
      KOUT("%d", type);
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void recv_work_dir_resp(int llid, int tid, t_topo_clc *conf)
{
  char *version = cloonix_conf_info_get_version();
  t_cloonix *cloonix = find_cloonix_with_doors_llid(llid);
  if (strcmp(conf->network, cloonix->name))
    KERR("%s %s", conf->network, cloonix->name);
  else if (strcmp(conf->version, version))
    KERR("%s %s", conf->version, version);
  else
    {
    send_event_topo_sub(llid, 0);
    g_net_exists(cloonix->name, cloonix->doors_llid, 1);
    strncpy(cloonix->work_dir, conf->work_dir, MAX_PATH_LEN-1);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *get_cloonix_work_dir(char *net_name)
{
  char *result = NULL;
  t_cloonix *cloonix = find_cloonix_with_name(net_name);
  if ((cloonix) && (strlen(cloonix->work_dir)))
    result = cloonix->work_dir;
  return result; 
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static gboolean connect_glib_cb (GIOChannel    *source,
                                 GIOCondition  condition,
                                 gpointer      data)
{
  t_cloonix *cur = (t_cloonix *) data;
  int glib_fd, err = 0;
  unsigned int len = sizeof(err);
  int doors_llid;
  char *ch;
  t_fd_event rx_glib;
  t_fd_event tx_glib;
  cur->connect_tag = 0;
  if (cur->connect_fd == -1)
    KOUT(" ");
  glib_fd = g_io_channel_unix_get_fd(source);
  if (glib_fd != cur->connect_fd)
    KOUT("%d %d", glib_fd, cur->connect_fd);
  if (condition != G_IO_OUT)
    connect_llid_del(cur);
  else if (cur->doors_llid > 0)
    KERR("TWO CONNECTS FOR ONE REQUEST");
  else
    {
    if (getsockopt(cur->connect_fd, SOL_SOCKET, SO_ERROR, &err, &len))
      connect_llid_del(cur);
    else
      {
      if (err)
        connect_llid_del(cur);
      else
        {
        if (read(cur->connect_fd, &ch, 0))
          {
          connect_llid_del(cur);
          KERR(" ");
          }
        else
          {
          doors_llid = doorways_sock_client_inet_end_glib(doors_type_switch,
                                                          cur->connect_fd,
                                                          cur->passwd,
                                                          doorways_client_end,
                                                          doorways_client_rx,
                                                          &(rx_glib),
                                                          &(tx_glib));
          if (doors_llid == 0)
            connect_llid_del(cur);
          else
            {
            doors_llid_attach(cur, doors_llid, rx_glib, tx_glib);
            doorways_tx(doors_llid, 0, doors_type_switch,
                        doors_val_init_link, strlen("OK") , "OK");
            send_work_dir_req(doors_llid, 0);
            glib_update_watch(cur);
            }
          }
        }
      }
    }
  return FALSE;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void connect_llid_add(t_cloonix *cloonix, int fd)
{
  t_cloonix *cloon;
  if (cloonix->connect_fd != -1)
    KOUT(" ");
  cloon  = find_cloonix_with_fd(fd);
  if (cloon)
    KOUT("%d %d %d %s", fd, cloon->doors_fd, cloon->connect_fd, cloon->name);
  cloonix->connect_fd = fd;
  cloonix->g_io_connect_channel = g_io_channel_unix_new(fd);
  cloonix->doors_llid = 0;
  cloonix->connect_tag = g_io_add_watch (cloonix->g_io_connect_channel, 
                                         G_IO_OUT, connect_glib_cb, cloonix);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void connect_llid_del(t_cloonix *cloonix)
{
  if (cloonix->connect_tag)
    g_source_remove(cloonix->connect_tag);
  if (cloonix->g_io_connect_channel)
    {
    g_io_channel_shutdown(cloonix->g_io_connect_channel, FALSE, NULL);
    cloonix->g_io_connect_channel = NULL;
    }
  if (cloonix->connect_fd != -1)
    close(cloonix->connect_fd);
  cloonix->connect_fd = -1;
  cloonix->connect_tag = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static gboolean glib_cb (GIOChannel   *source,
                         GIOCondition  condition,
                         gpointer data)
{
  t_cloonix *cloonix = (t_cloonix *) data;
  if (cloonix->doors_fd == -1)
    KOUT(" ");
  if (!msg_exist_channel(cloonix->doors_llid))
    {
    KERR("%s %d", cloonix->name, cloonix->doors_llid);
    doors_llid_detach(cloonix);
    return FALSE;
    }
  if (source != cloonix->g_io_channel)
    {
    KERR("%s %d", cloonix->name, cloonix->doors_llid);
    doors_llid_detach(cloonix);
    return FALSE;
    }
  cloonix->tag = 0;
  if (condition & G_IO_OUT)
    {
    cloonix->glibtx(NULL, cloonix->doors_llid, cloonix->doors_fd);
    glib_update_watch(cloonix);
    }
  else if (condition & G_IO_IN)
    {
    if (!cloonix->glibrx(NULL, cloonix->doors_llid, cloonix->doors_fd))
      {
      if (msg_exist_channel(cloonix->doors_llid))
        doors_llid_detach(cloonix);
      }
    else
      glib_update_watch(cloonix);
    }
  else
    {
    KERR("%s %d %X", cloonix->name, cloonix->doors_llid, condition);
    doors_llid_detach(cloonix);
    }
  return FALSE;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timeout_second(void *data)
{
  static int count = 0;
  uint32_t ip;
  int fd, port;
  t_cloonix *cur = g_head_cloonix;
  count += 1;
  if (count == 3)
    {
    count = 0;
    while (cur)
      {
      if (!cur->doors_llid)
        {
        if (cur->connect_fd != -1)
          connect_llid_del(cur);
        doorways_sock_address_detect(cur->path, &ip, &port);
        fd = util_nonblock_client_socket_inet(ip, port);
        if (fd != -1)
          {
          connect_llid_add(cur, fd);
          }
        }
      cur = cur->next;
      }
    }
  clownix_timeout_add(10, timeout_second, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void interface_init(char *conf_path, t_net_exists_cb net) 
{
  t_cloonix_conf_info *cnf;
  int i, nb_cloonix_servers;
  g_net_exists   = net; 
  g_head_cloonix = NULL;
  doorways_sock_init();
  msg_mngt_init("hyperzor", IO_MAX_BUF_LEN);
  doors_io_basic_xml_init(doorways_client_tx);
  if (cloonix_conf_info_init(conf_path))
    KOUT("%s", conf_path);
  cloonix_conf_info_get_all(&nb_cloonix_servers, &cnf);
  printf("\nVersion:%s\n", cloonix_conf_info_get_version());
  for (i=0; i<nb_cloonix_servers; i++)
    cloonix_alloc(cnf[i].name, cnf[i].doors, cnf[i].passwd);
  clownix_timeout_add(10, timeout_second, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/
