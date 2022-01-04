/*****************************************************************************/
/*    Copyright (C) 2006-2022 clownix@clownix.net License AGPL-3             */
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
#include "dpdk_msg.h"
#include "fmt_diag.h"
#include "dpdk_tap.h"
#include "dpdk_kvm.h"
#include "dpdk_phy.h"
#include "dpdk_nat.h"


/****************************************************************************/
int fmt_tx_add_ethd(int tid, char *name, int num)
{
  int result;
  char cmd[2*MAX_PATH_LEN];
  memset(cmd, 0, 2*MAX_PATH_LEN);
  sprintf(cmd,
  "cloonixovs_add_ethd name=%s num=%d", name, num);
  result = dpdk_ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_add_eths1(int tid, char *name, int num)
{
  int result;
  char cmd[2*MAX_PATH_LEN];
  memset(cmd, 0, 2*MAX_PATH_LEN);
  sprintf(cmd,
  "cloonixovs_add_eths1 name=%s num=%d", name, num);
  result = dpdk_ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
int fmt_tx_add_eths2(int tid, char *name, int num)
{
  int result;
  char cmd[2*MAX_PATH_LEN];
  memset(cmd, 0, 2*MAX_PATH_LEN);
  sprintf(cmd,
  "cloonixovs_add_eths2 name=%s num=%d", name, num);
  result = dpdk_ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_del_ethd(int tid, char *name, int num)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  sprintf(cmd,
  "cloonixovs_del_ethd name=%s num=%d", name, num);
  result = dpdk_ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_del_eths(int tid, char *name, int num)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  sprintf(cmd,
  "cloonixovs_del_eths name=%s num=%d", name, num);
  result = dpdk_ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_add_phy(int tid, char *name)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_add_phy name=%s", name);
  result = dpdk_ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_del_phy(int tid, char *name)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_del_phy name=%s", name);
  result = dpdk_ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_add_tap(int tid, char *name)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_add_tap name=%s", name);
  result = dpdk_ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_del_tap(int tid, char *name)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_del_tap name=%s", name);
  result = dpdk_ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_add_lan(int tid, char *lan)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, 
  "cloonixovs_add_lan lan=%s", lan);
  result = dpdk_ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_del_lan(int tid, char *lan)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_del_lan lan=%s", lan);
  result = dpdk_ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_add_lan_phy(int tid, char *lan, char *name)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_add_lan_phy lan=%s name=%s", lan, name);
  result = dpdk_ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_del_lan_phy(int tid, char *lan, char *name)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_del_lan_phy lan=%s name=%s", lan, name);
  result = dpdk_ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_add_lan_tap(int tid, char *lan, char *name)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_add_lan_tap lan=%s name=%s", lan, name);
  result = dpdk_ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_del_lan_tap(int tid, char *lan, char *name)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_del_lan_tap lan=%s name=%s", lan, name);
  result = dpdk_ovs_try_send_sigdiag_msg(tid, cmd);
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
  result = dpdk_ovs_try_send_sigdiag_msg(tid, cmd);
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
  result = dpdk_ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_add_lan_a2b(int tid, char *lan, char *name, int num)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_add_lan_a2b lan=%s name=%s num=%d", lan, name, num);
  result = dpdk_ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_del_lan_a2b(int tid, char *lan, char *name, int num)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_del_lan_a2b lan=%s name=%s num=%d", lan, name, num);
  result = dpdk_ovs_try_send_sigdiag_msg(tid, cmd);
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
  result = dpdk_ovs_try_send_sigdiag_msg(tid, cmd);
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
  result = dpdk_ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_add_lan_ethd(int tid, char *lan, char *name, int num)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_add_lan_ethd lan=%s name=%s num=%d", lan, name, num);
  result = dpdk_ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}

/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_del_lan_ethd(int tid, char *lan, char *name, int num)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_del_lan_ethd lan=%s name=%s num=%d", lan, name, num);
  result = dpdk_ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_add_lan_ethv(int tid, char *lan, char *name, int num, char *vhost)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_add_lan_ethv lan=%s name=%s num=%d vhost=%s",
  lan, name, num, vhost);
  result = dpdk_ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}

/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_del_lan_ethv(int tid, char *lan, char *name, int num, char *vhost)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_del_lan_ethv lan=%s name=%s num=%d vhost=%s",
  lan, name, num, vhost);
  result = dpdk_ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_add_lan_eths(int tid, char *lan, char *name, int num)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_add_lan_eths lan=%s name=%s num=%d", lan, name, num);
  result = dpdk_ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}

/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_del_lan_eths(int tid, char *lan, char *name, int num)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "cloonixovs_del_lan_eths lan=%s name=%s num=%d", lan, name, num);
  result = dpdk_ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void fmt_rx_rpct_recv_sigdiag_msg(int llid, int tid, char *line)
{
  int num;
  char lan[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  char vhost[MAX_NAME_LEN];

  if (sscanf(line, "KO cloonixovs_add_lan lan=%s", lan) == 1)
    dpdk_msg_ack_lan(tid, lan, 1, 1, "KO");
  else if (sscanf(line, "OK cloonixovs_add_lan lan=%s", lan) == 1)
    dpdk_msg_ack_lan(tid, lan, 1, 0, "OK");
  else if (sscanf(line, "KO cloonixovs_del_lan lan=%s", lan) == 1)
    dpdk_msg_ack_lan(tid, lan, 0, 1, "KO");
  else if (sscanf(line, "OK cloonixovs_del_lan lan=%s", lan) == 1)
    dpdk_msg_ack_lan(tid, lan, 0, 0, "OK");

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

  else if (sscanf(line,"KO cloonixovs_add_lan_a2b lan=%s name=%s num=%d",
                        lan, name, &num) == 3)
    dpdk_msg_ack_lan_endp(tid, lan, name, num, 1, 1, "KO");
  else if (sscanf(line,"OK cloonixovs_add_lan_a2b lan=%s name=%s num=%d",
                        lan, name, &num) == 3)
    dpdk_msg_ack_lan_endp(tid, lan, name, num, 1, 0, "OK");
  else if (sscanf(line,"KO cloonixovs_del_lan_a2b lan=%s name=%s num=%d",
                        lan, name, &num) == 3)
    dpdk_msg_ack_lan_endp(tid, lan, name, num, 0, 1, "KO");
  else if (sscanf(line,"OK cloonixovs_del_lan_a2b lan=%s name=%s num=%d",
                        lan, name, &num) == 3)
    dpdk_msg_ack_lan_endp(tid, lan, name, num, 0, 0, "OK");

  else if (sscanf(line, "OK cloonixovs_add_ethd name=%s num=%d",
                        name, &num) == 2)
    dpdk_kvm_resp_add(0, name, num);
  else if (sscanf(line, "KO cloonixovs_add_ethd name=%s num=%d",
                        name, &num) == 2)
    dpdk_kvm_resp_add(1, name, num);

  else if (sscanf(line, "OK cloonixovs_add_eths1 name=%s num=%d",
                        name, &num) == 2)
    dpdk_kvm_resp_add(0, name, num);
  else if (sscanf(line, "KO cloonixovs_add_eths1 name=%s num=%d",
                        name, &num) == 2)
    dpdk_kvm_resp_add(1, name, num);


  else if (sscanf(line, "OK cloonixovs_add_eths2 name=%s num=%d",
                        name, &num) == 2)
    dpdk_kvm_resp_add_eths2(0, name, num);
  else if (sscanf(line, "KO cloonixovs_add_eths2 name=%s num=%d",
                        name, &num) == 2)
    dpdk_kvm_resp_add_eths2(1, name, num);

  else if (sscanf(line, "OK cloonixovs_del_ethd name=%s num=%d",
                        name, &num) == 2)
    dpdk_kvm_resp_del(0, name, num);
  else if (sscanf(line, "KO cloonixovs_del_ethd name=%s num=%d",
                        name, &num) == 2)
    dpdk_kvm_resp_del(1, name, num);

  else if (sscanf(line, "OK cloonixovs_del_eths name=%s num=%d",
                        name, &num) == 2)
    dpdk_kvm_resp_del(0, name, num);
  else if (sscanf(line, "KO cloonixovs_del_eths name=%s num=%d",
                        name, &num) == 2)
    dpdk_kvm_resp_del(1, name, num);

  else if (sscanf(line, "OK cloonixovs_add_lan_ethd lan=%s name=%s num=%d",
                        lan, name, &num) == 3)
    dpdk_msg_ack_lan_endp(tid, lan, name, num, 1, 0, "OK");
  else if (sscanf(line, "KO cloonixovs_add_lan_ethd lan=%s name=%s num=%d",
                        lan, name, &num) == 3)
    dpdk_msg_ack_lan_endp(tid, lan, name, num, 1, 1, "KO");
  else if (sscanf(line, "OK cloonixovs_del_lan_ethd lan=%s name=%s num=%d",
                        lan, name, &num) == 3)
    dpdk_msg_ack_lan_endp(tid, lan, name, num, 0, 0, "OK");
  else if (sscanf(line, "KO cloonixovs_del_lan_ethd lan=%s name=%s num=%d",
                        lan, name, &num) == 3)
    dpdk_msg_ack_lan_endp(tid, lan, name, num, 0, 1, "KO");


  else if (sscanf(line, "OK cloonixovs_add_lan_eths lan=%s name=%s num=%d",
                        lan, name, &num) == 3)
    dpdk_msg_ack_lan_endp(tid, lan, name, num, 1, 0, "OK");
  else if (sscanf(line, "KO cloonixovs_add_lan_eths lan=%s name=%s num=%d",
                        lan, name, &num) == 3)
    dpdk_msg_ack_lan_endp(tid, lan, name, num, 1, 1, "KO");
  else if (sscanf(line, "OK cloonixovs_del_lan_eths lan=%s name=%s num=%d",
                        lan, name, &num) == 3)
    dpdk_msg_ack_lan_endp(tid, lan, name, num, 0, 0, "OK");
  else if (sscanf(line, "KO cloonixovs_del_lan_eths lan=%s name=%s num=%d",
                        lan, name, &num) == 3)
    dpdk_msg_ack_lan_endp(tid, lan, name, num, 0, 1, "KO");


  else if (sscanf(line, "OK cloonixovs_add_lan_ethv lan=%s name=%s num=%d vhost=%s",
                        lan, name, &num, vhost) == 4)
    dpdk_msg_ack_lan_endp(tid, lan, name, num, 1, 0, "OK");
  else if (sscanf(line, "KO cloonixovs_add_lan_ethv lan=%s name=%s num=%d vhost=%s",
                        lan, name, &num, vhost) == 4)
    dpdk_msg_ack_lan_endp(tid, lan, name, num, 1, 1, "KO");
  else if (sscanf(line, "OK cloonixovs_del_lan_ethv lan=%s name=%s num=%d vhost=%s",
                        lan, name, &num, vhost) == 4)
    dpdk_msg_ack_lan_endp(tid, lan, name, num, 0, 0, "OK");
  else if (sscanf(line, "KO cloonixovs_del_lan_ethv lan=%s name=%s num=%d vhost=%s",
                        lan, name, &num, vhost) == 4)
    dpdk_msg_ack_lan_endp(tid, lan, name, num, 0, 1, "KO");


  else if (sscanf(line,"OK cloonixovs_add_tap name=%s", name) == 1)
    dpdk_tap_resp_add(0, name);
  else if (sscanf(line, "KO cloonixovs_add_tap name=%s", name) == 1)
    dpdk_tap_resp_add(1, name);
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

  else if (sscanf(line,"OK cloonixovs_add_phy name=%s", name) == 1)
    dpdk_phy_resp_add(0, name);
  else if (sscanf(line, "KO cloonixovs_add_phy name=%s", name) == 1)
    dpdk_phy_resp_add(1, name);
  else if (sscanf(line, "OK cloonixovs_del_phy name=%s", name) == 1)
    dpdk_phy_resp_del(0, name);
  else if (sscanf(line, "KO cloonixovs_del_phy name=%s", name) == 1)
    dpdk_phy_resp_del(1, name);

  else if (sscanf(line,"OK cloonixovs_add_lan_phy lan=%s name=%s",lan,name)==2)
    dpdk_msg_ack_lan_endp(tid, lan, name, 0, 1, 0, "OK");
  else if (sscanf(line,"KO cloonixovs_add_lan_phy lan=%s name=%s",lan,name)==2)
    dpdk_msg_ack_lan_endp(tid, lan, name, 0, 1, 1, "KO");
  else if (sscanf(line,"OK cloonixovs_del_lan_phy lan=%s name=%s",lan,name)==2)
    dpdk_msg_ack_lan_endp(tid, lan, name, 0, 0, 0, "OK");
  else if (sscanf(line,"KO cloonixovs_del_lan_phy lan=%s name=%s",lan,name)==2)
    dpdk_msg_ack_lan_endp(tid, lan, name, 0, 0, 1, "KO");

  else
    KERR("ERROR: %s", line);
}
/*--------------------------------------------------------------------------*/

