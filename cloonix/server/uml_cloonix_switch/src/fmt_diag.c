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
#include "fmt_diag.h"
#include "dpdk_tap.h"
#include "dpdk_snf.h"
#include "dpdk_nat.h"
#include "vhost_eth.h"
#include "edp_vhost.h"
#include "pci_dpdk.h"


/****************************************************************************/
int fmt_tx_add_lan_pci_dpdk(int tid, char *lan, char *pci)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_add_lan_pci_dpdk lan=%s pci=%s", lan, pci);
  result = dpdk_ovs_try_send_diag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_del_lan_pci_dpdk(int tid, char *lan, char *pci)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_del_lan_pci_dpdk lan=%s pci=%s", lan, pci);
  result = dpdk_ovs_try_send_diag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_add_phy_vhost_port(int tid, char *name, char *lan)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_add_lan_phy_port lan=%s name=%s", lan, name);
  result = dpdk_ovs_try_send_diag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_del_phy_vhost_port(int tid, char *name, char *lan)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_del_lan_phy_port lan=%s name=%s", lan, name);
  result = dpdk_ovs_try_send_diag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_add_vhost_lan(int tid, char *name, int num, char *lan)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_add_lan_vhost lan=%s name=%s num=%d", lan, name, num);
  result = dpdk_ovs_try_send_diag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_add_vhost_port(int tid,char *name,int num,char *vhost,char *lan)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_add_lan_vhost_port lan=%s name=%s num=%d vhost=%s",
  lan, name, num, vhost);
  result = dpdk_ovs_try_send_diag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_del_vhost_lan(int tid, char *name, int num, char *lan)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_del_lan_vhost lan=%s name=%s num=%d", lan, name, num);
  result = dpdk_ovs_try_send_diag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_del_vhost_port(int tid,char *name,int num,char *vhost,char *lan)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_del_lan_vhost_port lan=%s name=%s num=%d vhost=%s",
  lan, name, num, vhost);
  result = dpdk_ovs_try_send_diag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_add_tap(int tid, char *name)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "cloonixovs_add_tap name=%s", name);
  result = dpdk_ovs_try_send_diag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_del_tap(int tid, char *name)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "cloonixovs_del_tap name=%s", name);
  result = dpdk_ovs_try_send_diag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_add_lan_tap(int tid, char *lan, char *name)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "cloonixovs_add_lan_tap lan=%s name=%s",
                                lan, name);
  result = dpdk_ovs_try_send_diag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_del_lan_tap(int tid, char *lan, char *name)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "cloonixovs_del_lan_tap lan=%s name=%s",
                                lan, name);
  result = dpdk_ovs_try_send_diag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_add_lan_br(int tid, char *lan)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "cloonixovs_add_lan_br lan=%s", lan);
  result = dpdk_ovs_try_send_diag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_del_lan_br(int tid, char *lan)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "cloonixovs_del_lan_br lan=%s", lan);
  result = dpdk_ovs_try_send_diag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_add_lan_snf(int tid, char *lan, char *name)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_add_lan_snf lan=%s name=%s", lan, name);
  result = dpdk_ovs_try_send_diag_msg(tid, cmd);
  return result;
}

/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_del_lan_snf(int tid, char *lan, char *name)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_del_lan_snf lan=%s name=%s", lan, name);
  result = dpdk_ovs_try_send_diag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_add_lan_nat(int tid, char *lan, char *name)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_add_lan_nat lan=%s name=%s", lan, name);
  result = dpdk_ovs_try_send_diag_msg(tid, cmd);
  return result;
}

/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_del_lan_nat(int tid, char *lan, char *name)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_del_lan_nat lan=%s name=%s", lan, name);
  result = dpdk_ovs_try_send_diag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_add_lan_d2d(int tid, char *lan, char *name)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_add_lan_d2d lan=%s name=%s", lan, name);
  result = dpdk_ovs_try_send_diag_msg(tid, cmd);
  return result;
}

/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_del_lan_d2d(int tid, char *lan, char *name)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_del_lan_d2d lan=%s name=%s", lan, name);
  result = dpdk_ovs_try_send_diag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_add_eth(int tid, char *name, int num, char *strmac)
{
  int result;
  char cmd[2*MAX_PATH_LEN];
  memset(cmd, 0, 2*MAX_PATH_LEN);
  sprintf(cmd, "cloonixovs_add_eth name=%s num=%d", name, num);
  if ((strlen(cmd) + strlen(strmac)) > 2*MAX_PATH_LEN)
    KOUT("%d %d", (int)strlen(cmd), (int)strlen(strmac));
  strcat(cmd, strmac);
  result = dpdk_ovs_try_send_diag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_del_eth(int tid, char *name, int num)
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
int fmt_tx_add_lan_eth(int tid, char *lan, char *name, int num)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_add_lan_eth lan=%s name=%s num=%d", lan, name, num);
  result = dpdk_ovs_try_send_diag_msg(tid, cmd);
  return result;
}

/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_del_lan_eth(int tid, char *lan, char *name, int num)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_del_lan_eth lan=%s name=%s num=%d", lan, name, num);
  result = dpdk_ovs_try_send_diag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void fmt_rx_rpct_recv_diag_msg(int llid, int tid, char *line)
{
  int num;
  char lan[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  char strmac[MAX_NAME_LEN];
  char pci[MAX_NAME_LEN];
  char vhost[MAX_NAME_LEN];

  if (sscanf(line,
      "OK cloonixovs_add_lan_pci_dpdk lan=%s pci=%s", lan, pci) == 2)
      pci_dpdk_ack_add(tid, 1, lan, pci);
  else if (sscanf(line,
      "KO cloonixovs_add_lan_pci_dpdk lan=%s pci=%s", lan, pci) == 2)
      pci_dpdk_ack_add(tid, 0, lan, pci);
  else if (sscanf(line,
      "OK cloonixovs_del_lan_pci_dpdk lan=%s pci=%s", lan, pci) == 2)
      pci_dpdk_ack_del(tid, 1, lan, pci);
  else if (sscanf(line,
      "KO cloonixovs_del_lan_pci_dpdk lan=%s pci=%s", lan, pci) == 2)
      pci_dpdk_ack_del(tid, 0, lan, pci);

  else if (sscanf(line,
      "OK cloonixovs_add_lan_phy_port lan=%s name=%s", lan, name) == 2)
      edp_vhost_ack_add_port(tid, 1, lan, name);
  else if (sscanf(line,
      "OK cloonixovs_del_lan_phy_port lan=%s name=%s", lan, name) == 2)
      edp_vhost_ack_del_port(tid, 1, lan, name);
  else if (sscanf(line,
      "KO cloonixovs_add_lan_phy_port lan=%s name=%s", lan, name) == 2)
      edp_vhost_ack_add_port(tid, 0, lan, name);
  else if (sscanf(line,
      "KO cloonixovs_del_lan_phy_port lan=%s name=%s", lan, name) == 2)
      edp_vhost_ack_del_port(tid, 0, lan, name);

  else if (sscanf(line, 
          "OK cloonixovs_add_lan_vhost lan=%s name=%s num=%d",
          lan, name, &num) == 3)
    vhost_eth_add_ack_lan(tid, 1, lan, name, num);
  else if (sscanf(line, 
          "OK cloonixovs_add_lan_vhost_port lan=%s name=%s num=%d vhost=%s",
          lan, name, &num, vhost) == 4)
    vhost_eth_add_ack_port(tid, 1, lan, name, num);
  else if (sscanf(line, 
          "OK cloonixovs_del_lan_vhost lan=%s name=%s num=%d",
          lan, name, &num) == 3)
    vhost_eth_del_ack_lan(tid, 1, lan, name, num);
  else if (sscanf(line, 
          "OK cloonixovs_del_lan_vhost_port lan=%s name=%s num=%d vhost=%s",
          lan, name, &num, vhost) == 4)
    vhost_eth_del_ack_port(tid, 1, lan, name, num);
  else if (sscanf(line, 
          "KO cloonixovs_add_lan_vhost lan=%s name=%s num=%d",
          lan, name, &num) == 3)
    vhost_eth_add_ack_lan(tid, 0, lan, name, num);
  else if (sscanf(line,
          "KO cloonixovs_add_lan_vhost_port lan=%s name=%s num=%d vhost=%s",
          lan, name, &num, vhost) == 4)
    vhost_eth_add_ack_port(tid, 0, lan, name, num);
  else if (sscanf(line,
          "KO cloonixovs_del_lan_vhost lan=%s name=%s num=%d",
          lan, name, &num) == 3)
    vhost_eth_del_ack_lan(tid, 0, lan, name, num);
  else if (sscanf(line,
          "KO cloonixovs_del_lan_vhost_port lan=%s name=%s num=%d vhost=%s",
          lan, name, &num, vhost) == 4)
    vhost_eth_del_ack_port(tid, 0, lan, name, num);

  else if (sscanf(line, "KO cloonixovs_add_eth name=%s num=%d",name,&num)==2)
    dpdk_msg_ack_eth(tid, name, num, 1, 1, "KO");
  else if (sscanf(line, "OK cloonixovs_add_eth name=%s num=%d",name,&num)==2)
    dpdk_msg_ack_eth(tid, name, num, 1, 0, "OK");
  else if (sscanf(line, "KO cloonixovs_del_eth name=%s num=%d",name,&num)==2)
    dpdk_msg_ack_eth(tid, name, num, 0, 1, "KO");
  else if (sscanf(line, "OK cloonixovs_del_eth name=%s num=%d",name,&num)==2)
    dpdk_msg_ack_eth(tid, name, num, 0, 0, "OK");

  else if (sscanf(line, "KO cloonixovs_add_lan_br lan=%s", lan) == 1)
    dpdk_msg_ack_lan(tid, lan, 1, 1, "KO");
  else if (sscanf(line, "OK cloonixovs_add_lan_br lan=%s", lan) == 1)
    dpdk_msg_ack_lan(tid, lan, 1, 0, "OK");
  else if (sscanf(line, "KO cloonixovs_del_lan_br lan=%s", lan) == 1)
    dpdk_msg_ack_lan(tid, lan, 0, 1, "KO");
  else if (sscanf(line, "OK cloonixovs_del_lan_br lan=%s", lan) == 1)
    dpdk_msg_ack_lan(tid, lan, 0, 0, "OK");

  else if (sscanf(line,"KO cloonixovs_add_lan_snf lan=%s name=%s",lan,name)==2)
    dpdk_msg_ack_lan_endp(tid, lan, name, 0, 1, 1, "KO");
  else if (sscanf(line,"OK cloonixovs_add_lan_snf lan=%s name=%s",lan,name)==2)
    dpdk_msg_ack_lan_endp(tid, lan, name, 0, 1, 0, "OK");
  else if (sscanf(line,"KO cloonixovs_del_lan_snf lan=%s name=%s",lan,name)==2)
    dpdk_msg_ack_lan_endp(tid, lan, name, 0, 0, 1, "KO");
  else if (sscanf(line,"OK cloonixovs_del_lan_snf lan=%s name=%s",lan,name)==2)
    dpdk_msg_ack_lan_endp(tid, lan, name, 0, 0, 0, "OK");

  else if (sscanf(line,"KO cloonixovs_add_lan_nat lan=%s name=%s",lan,name)==2)
    dpdk_msg_ack_lan_endp(tid, lan, name, 0, 1, 1, "KO");
  else if (sscanf(line,"OK cloonixovs_add_lan_nat lan=%s name=%s",lan,name)==2)
    dpdk_msg_ack_lan_endp(tid, lan, name, 0, 1, 0, "OK");
  else if (sscanf(line,"KO cloonixovs_del_lan_nat lan=%s name=%s",lan,name)==2)
    dpdk_msg_ack_lan_endp(tid, lan, name, 0, 0, 1, "KO");
  else if (sscanf(line,"OK cloonixovs_del_lan_nat lan=%s name=%s",lan,name)==2)
    dpdk_msg_ack_lan_endp(tid, lan, name, 0, 0, 0, "OK");

  else if (sscanf(line,"KO cloonixovs_add_lan_d2d lan=%s name=%s",lan,name)==2)
    dpdk_msg_ack_lan_endp(tid, lan, name, 0, 1, 1, "KO");
  else if (sscanf(line,"OK cloonixovs_add_lan_d2d lan=%s name=%s",lan,name)==2)
    dpdk_msg_ack_lan_endp(tid, lan, name, 0, 1, 0, "OK");
  else if (sscanf(line,"KO cloonixovs_del_lan_d2d lan=%s name=%s",lan,name)==2)
    dpdk_msg_ack_lan_endp(tid, lan, name, 0, 0, 1, "KO");
  else if (sscanf(line,"OK cloonixovs_del_lan_d2d lan=%s name=%s",lan,name)==2)
    dpdk_msg_ack_lan_endp(tid, lan, name, 0, 0, 0, "OK");

  else if (sscanf(line, "KO cloonixovs_add_lan_eth lan=%s name=%s num=%d",
                        lan, name, &num) == 3)
    dpdk_msg_ack_lan_endp(tid, lan, name, num, 1, 1, "KO");
  else if (sscanf(line, "OK cloonixovs_add_lan_eth lan=%s name=%s num=%d",
                        lan, name, &num) == 3)
    dpdk_msg_ack_lan_endp(tid, lan, name, num, 1, 0, "OK");
  else if (sscanf(line, "KO cloonixovs_del_lan_eth lan=%s name=%s num=%d",
                         lan, name, &num)==3)
    dpdk_msg_ack_lan_endp(tid, lan, name, num, 0, 1, "KO");
  else if (sscanf(line, "OK cloonixovs_del_lan_eth lan=%s name=%s num=%d",
                         lan,name,&num)==3)
    dpdk_msg_ack_lan_endp(tid, lan, name, num, 0, 0, "OK");


  else if (sscanf(line,"OK cloonixovs_add_tap name=%s mac=%s",name,strmac)==2)
    dpdk_tap_resp_add(0, name, strmac);
  else if (sscanf(line, "KO cloonixovs_add_tap name=%s", name) == 1)
    dpdk_tap_resp_add(1, name, NULL);
  else if (sscanf(line, "OK cloonixovs_del_tap name=%s", name) == 1)
    dpdk_tap_resp_del(0, name);
  else if (sscanf(line, "KO cloonixovs_del_tap name=%s", name) == 1)
    dpdk_tap_resp_del(1, name);
  else if (sscanf(line,"OK cloonixovs_add_lan_tap lan=%s name=%s",lan,name)==2)
    dpdk_msg_ack_lan_endp(tid, lan, name, 0, 1, 0, "OK");
  else if (sscanf(line,"KO cloonixovs_add_lan_tap lan=%s name=%s",lan,name)==2)
    dpdk_msg_ack_lan_endp(tid, lan, name, 0, 1, 1, "KO");
  else if (sscanf(line,"OK cloonixovs_del_lan_tap lan=%s name=%s",lan,name)==2)
    dpdk_msg_ack_lan_endp(tid, lan, name, 0, 0, 0, "OK");
  else if (sscanf(line,"KO cloonixovs_del_lan_tap lan=%s name=%s",lan,name)==2)
    dpdk_msg_ack_lan_endp(tid, lan, name, 0, 0, 1, "KO");
  else
    KERR("ERR: %s", line);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void fmt_rx_rpct_recv_evt_msg(int llid, int tid, char *line)
{
  KERR("%d %d %s", llid, tid, line);
}
/*--------------------------------------------------------------------------*/

