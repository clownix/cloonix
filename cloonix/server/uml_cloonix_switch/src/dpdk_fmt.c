/*****************************************************************************/
/*    Copyright (C) 2006-2019 cloonix@cloonix.net License AGPL-3             */
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
#include <errno.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "commun_daemon.h"
#include "pid_clone.h"
#include "dpdk_ovs.h"
#include "dpdk_dyn.h"
#include "dpdk_msg.h"
#include "dpdk_fmt.h"

/****************************************************************************/
int dpdk_fmt_tx_add_lan(int tid, char *lan, int spy)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "cloonixovs_add_lan lan_name=%s spy=%d",
                                lan, spy);
  result = dpdk_ovs_try_send_diag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_fmt_tx_del_lan(int tid, char *lan, int spy)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "cloonixovs_del_lan lan_name=%s spy=%d",
                                lan, spy);
  result = dpdk_ovs_try_send_diag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_fmt_tx_add_eth(int tid, char *name, int num)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  sprintf(cmd, "cloonixovs_add_eth name=%s num=%d", name, num);
  result = dpdk_ovs_try_send_diag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_fmt_tx_del_eth(int tid, char *name, int num)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  sprintf(cmd, "cloonixovs_del_eth name=%s num=%d", name, num);
  result = dpdk_ovs_try_send_diag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_fmt_tx_add_lan_eth(int tid, char *lan_name, char *name, int num)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_add_lan_eth lan_name=%s name=%s num=%d", lan_name, name, num);
  result = dpdk_ovs_try_send_diag_msg(tid, cmd);
  return result;
}

/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_fmt_tx_del_lan_eth(int tid, char *lan_name, char *name, int num)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_del_lan_eth lan_name=%s name=%s num=%d", lan_name, name, num);
  result = dpdk_ovs_try_send_diag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_fmt_rx_rpct_recv_diag_msg(int llid, int tid, char *line)
{
  int num, spy;
  char lan_name[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  if (sscanf(line,
      "KO cloonixovs_add_eth name=%s num=%d", name, &num) == 2)
    {
    KERR("%s", line);
    dpdk_msg_ack_eth(tid, name, num, 1, 1, "cloonixovs");
    }
  else if (sscanf(line,
      "OK cloonixovs_add_eth name=%s num=%d", name, &num) == 2)
    {
    dpdk_msg_ack_eth(tid, name, num, 1, 0, "cloonixovs");
    }
  else if (sscanf(line,
      "KO cloonixovs_del_eth name=%s num=%d", name, &num) == 2)
    {
    KERR("%s", line);
    dpdk_msg_ack_eth(tid, name, num, 0, 1, "cloonixovs");
    }
  else if (sscanf(line,
      "OK cloonixovs_del_eth name=%s num=%d", name, &num) == 2)
    {
    dpdk_msg_ack_eth(tid, name, num, 0, 0, "cloonixovs");
    }
  else if (sscanf(line,
      "KO cloonixovs_add_lan lan_name=%s spy=%d", lan_name, &spy) == 2)
    {
    KERR("%s", line);
    dpdk_msg_ack_lan(tid, lan_name, 1, 1, "cloonixovs");
    }
  else if (sscanf(line,
      "OK cloonixovs_add_lan lan_name=%s spy=%d", lan_name, &spy) == 2)
    {
    dpdk_msg_ack_lan(tid, lan_name, 1, 0, "cloonixovs");
    }
  else if (sscanf(line,
      "KO cloonixovs_del_lan lan_name=%s spy=%d", lan_name, &spy) == 2)
    {
    KERR("%s", line);
    dpdk_msg_ack_lan(tid, lan_name, 0, 1, "cloonixovs");
    }
  else if (sscanf(line,
      "OK cloonixovs_del_lan lan_name=%s spy=%d", lan_name, &spy) == 2)
    {
    dpdk_msg_ack_lan(tid, lan_name, 0, 0, "cloonixovs");
    }
  else if (sscanf(line,
      "KO cloonixovs_add_lan_eth lan_name=%s name=%s num=%d",
      lan_name, name, &num) == 3)
    dpdk_msg_ack_lan_eth(tid, lan_name, name, num, 1, 1, "cloonixovs");
  else if (sscanf(line,
      "OK cloonixovs_add_lan_eth lan_name=%s name=%s num=%d",
      lan_name, name, &num) == 3)
    dpdk_msg_ack_lan_eth(tid, lan_name, name, num, 1, 0, "cloonixovs");
  else if (sscanf(line,
      "KO cloonixovs_del_lan_eth lan_name=%s name=%s num=%d",
      lan_name, name, &num) == 3)
    dpdk_msg_ack_lan_eth(tid, lan_name, name, num, 0, 1, "cloonixovs");
  else if (sscanf(line,
       "OK cloonixovs_del_lan_eth lan_name=%s name=%s num=%d",
      lan_name, name, &num) == 3)
    dpdk_msg_ack_lan_eth(tid, lan_name, name, num, 0, 0, "cloonixovs");
  else
    KERR("%s", line);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_fmt_rx_rpct_recv_evt_msg(int llid, int tid, char *line)
{
  int tidx, ptx, btx, prx, brx;
  unsigned int ms;
    if (sscanf(line,"endp_eventfull_tx_rx %u %d %d %d %d %d",
                         &ms, &tidx, &ptx, &btx, &prx, &brx) == 6)
      {
      }
    else
      KERR("%s", line);
}
/*--------------------------------------------------------------------------*/

