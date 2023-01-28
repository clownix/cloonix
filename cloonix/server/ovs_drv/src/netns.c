/*****************************************************************************/
/*    Copyright (C) 2006-2023 clownix@clownix.net License AGPL-3             */
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
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/if_tun.h>
#include <linux/if_arp.h>
#include <netinet/in.h>
#include <sys/queue.h>
#include <time.h>
#include <sched.h>
#include <sys/mount.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "action.h"


static char g_ovs_bin[MAX_PATH_LEN];
static char g_ovs_dir[MAX_PATH_LEN];
static char g_netns_namespace[MAX_PATH_LEN];
static char g_net_name[MAX_NAME_LEN];
static uint8_t g_buf_rx[TOT_NETNS_BUF_LEN];
static uint8_t g_buf_tx[TOT_NETNS_BUF_LEN];
static uint8_t g_head_msg[HEADER_NETNS_MSG];
static int g_fd_tx_to_parent;
static int g_fd_rx_from_parent;


/*****************************************************************************/
static int mycmp(char *req, char *targ)
{
  int result = strncmp(req, targ, strlen(targ));
  return result;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void process_msg_from_parent(char *line, char *respb)
{
  char *bin = g_ovs_bin;
  char *db = g_ovs_dir;
  int num;
  char lan[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  char vhost[MAX_NAME_LEN];
  memset(respb, 0, MAX_PATH_LEN);

  if (!mycmp(line,

"ovs_req_suidroot"))
    action_req_suidroot(respb);

  else if (!mycmp(line,

"ovs_req_ovsdb"))
    action_req_ovsdb(bin, db, g_net_name, respb);

  else if (!mycmp(line,

"ovs_req_ovs"))
    action_req_ovs_switch(bin, db, g_net_name, respb);

  else if (sscanf(line,

"ovs_vhost_up name=%s num=%d vhost=%s", name, &num, vhost) == 3)
    action_vhost_up(bin, db, respb, name, num, vhost);

  else if (sscanf(line,

"ovs_add_snf_lan name=%s num=%d vhost=%s lan=%s", name, &num, vhost, lan) == 4)
    action_add_snf_lan(bin, db, respb, name, num, vhost, lan);

  else if (sscanf(line,

"ovs_del_snf_lan name=%s num=%d vhost=%s lan=%s", name, &num, vhost, lan) == 4)
    action_del_snf_lan(bin, db, respb, name, num, vhost, lan);

  else if (sscanf(line,

"ovs_add_lan lan=%s", name) == 1)
    action_add_lan(bin, db, respb, name);

  else if (sscanf(line,

"ovs_del_lan lan=%s", name) == 1)
    action_del_lan(bin, db, respb, name);

  else if (sscanf(line,

"ovs_add_lan_endp name=%s num=%d vhost=%s lan=%s", name, &num, vhost, lan) == 4)
    action_add_lan_endp(bin, db, respb, lan, name, num, vhost);

  else if (sscanf(line,

"ovs_del_lan_endp name=%s num=%d vhost=%s lan=%s", name, &num, vhost, lan) == 4)
    action_del_lan_endp(bin, db, respb, lan, name, num, vhost);

  else
    KOUT("%s", line);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void process_msg(void)
{
  static char respb[MAX_PATH_LEN];
  int tid, len, tx, rx;
  len = read(g_fd_rx_from_parent, g_head_msg, HEADER_NETNS_MSG);
  if ((len != 0 ) && (len != HEADER_NETNS_MSG))
    KOUT("ERROR %d %d", len, errno);
  else if (len == HEADER_NETNS_MSG)
    {
    if ((g_head_msg[0] != 0xCA) || (g_head_msg[1] != 0xFE)) 
      KOUT("ERROR %d %d %hhx %hhx",len,errno,g_head_msg[0],g_head_msg[1]);
    len = ((g_head_msg[2] & 0xFF) << 8) + (g_head_msg[3] & 0xFF);
    tid = ((g_head_msg[4] & 0xFF) << 8) + (g_head_msg[5] & 0xFF);
    if ((len == 0) || (len > MAX_NETNS_BUF_LEN))
      KOUT("ERROR %d %hhx %hhx", len, g_head_msg[2], g_head_msg[3]);
    rx = read(g_fd_rx_from_parent, g_buf_rx, len);
    if (len != rx)
      KOUT("ERROR NETNS %d %d %d", len, rx, errno);
    if (!strlen((char *)g_buf_rx))
      KERR("ERROR %d", len);
    else
      {
      process_msg_from_parent((char *) g_buf_rx, respb);
      if (strlen(respb) > 0)
        {
        len = strlen(respb) + 1;
        g_buf_tx[0] = 0xCA;
        g_buf_tx[1] = 0xFE;
        g_buf_tx[2] = (len & 0xFF00) >> 8;
        g_buf_tx[3] = len & 0xFF;
        g_buf_tx[4] = (tid & 0xFF00) >> 8;
        g_buf_tx[5] = tid & 0xFF;
        memcpy(&(g_buf_tx[HEADER_NETNS_MSG]), respb, len);
        tx = write(g_fd_tx_to_parent, g_buf_tx, len + HEADER_NETNS_MSG);
        if (tx != len + HEADER_NETNS_MSG)
          KOUT("ERROR WRITE %d %d %d", tx, len, errno);
        }
    }
  }
}
/*-------------------------------------------------------------------------*/

/*****************************************************************************/
static void forked_process_netns(int fd_rx_from_parent)
{
  int fd, fd_max, result; 
  fd_set infd;
  int llid, nb_chan, i;
  clownix_timeout_del_all();
  nb_chan = get_clownix_channels_nb();
  for (i=0; i<=nb_chan; i++)
    {
    llid = channel_get_llid_with_cidx(i);
    if (llid)
      {
      if (msg_exist_channel(llid))
        msg_delete_channel(llid);
      }
    }
  fd = open(g_netns_namespace, O_RDONLY|O_CLOEXEC);
  if (fd < 0)
    KOUT("ERROR %s", g_netns_namespace);
  if (setns(fd, CLONE_NEWNET) != 0)
    KOUT("ERROR %s", g_netns_namespace);
  close(fd);

  fd_max = fd_rx_from_parent;

  while(1)
    {
    FD_ZERO(&infd);
    FD_SET(fd_rx_from_parent, &infd);
    result = select(fd_max + 1, &infd, NULL, NULL, NULL);
    if (result < 0)
      KOUT("ERROR %s", g_netns_namespace);
    if (FD_ISSET(fd_rx_from_parent, &infd))
      process_msg();
    }

}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int netns_open(char *net_name, int *fd_rx, int *fd_tx,
               char *ovs_bin, char *ovs_dir)
{
  int pid;
  int fd1[2];
  int fd2[2];
  
  memset(g_ovs_bin, 0, MAX_PATH_LEN);
  memset(g_ovs_dir, 0, MAX_PATH_LEN);
  memset(g_netns_namespace, 0, MAX_PATH_LEN);
  memset(g_net_name, 0, MAX_NAME_LEN);
  strncpy(g_ovs_bin, ovs_bin, MAX_PATH_LEN-1);
  strncpy(g_ovs_dir, ovs_dir, MAX_PATH_LEN-1);
  strncpy(g_net_name, net_name, MAX_NAME_LEN-1);
  snprintf(g_netns_namespace, MAX_PATH_LEN-1, "%s%s_%s",
           PATH_NAMESPACE, BASE_NAMESPACE, net_name);

  if (pipe(fd1) == -1)
    KOUT("ERROR %s", g_netns_namespace);
  if (pipe(fd2) == -1)
    KOUT("ERROR %s", g_netns_namespace);
  if ((pid = fork()) < 0)
    KOUT("ERROR %s", g_netns_namespace);
  if (pid == 0)
    {
    close(fd1[0]);
    close(fd2[1]);
    g_fd_tx_to_parent = fd1[1];
    g_fd_rx_from_parent = fd2[0];
    forked_process_netns(fd2[0]);
    }
  else
    {
    close(fd1[1]);
    close(fd2[0]);
    *fd_rx = fd1[0];
    *fd_tx = fd2[1]; 
    }
  return pid;
}
/*---------------------------------------------------------------------------*/

