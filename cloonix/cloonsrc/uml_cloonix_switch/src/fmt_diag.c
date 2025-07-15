/*****************************************************************************/
/*    Copyright (C) 2006-2025 clownix@clownix.net License AGPL-3             */
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
#include "ovs.h"
#include "msg.h"
#include "fmt_diag.h"
#include "kvm.h"


/****************************************************************************/
int fmt_tx_system_promisc(int tid)
{
  int result;
  char vhost[MAX_NAME_LEN];
  char cmd[MAX_PATH_LEN];
  memset(vhost, 0, MAX_NAME_LEN);
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(vhost, MAX_NAME_LEN-1, "%s-system", cfg_get_cloonix_name()); 
  snprintf(cmd, MAX_PATH_LEN-1, "ovs_system_promisc vhost=%s", vhost);
  result = ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int  fmt_tx_vhost_up(int tid, char *name, int num, char *vhost)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "ovs_vhost_up name=%s num=%d vhost=%s", name, num, vhost);
  result = ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int  fmt_tx_add_snf_lan(int tid, char *name, int num, char *vhost, char *lan)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "ovs_add_snf_lan name=%s num=%d vhost=%s lan=%s", name, num, vhost, lan);
  result = ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int  fmt_tx_del_snf_lan(int tid, char *name, int num, char *vhost, char *lan)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "ovs_del_snf_lan name=%s num=%d vhost=%s lan=%s", name, num, vhost, lan);
  result = ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_add_tap(int tid, char *name, char *vhost, char *mac)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "ovs_add_tap name=%s vhost=%s mac=%s", name, vhost, mac);
  result = ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_del_tap(int tid, char *name, char *vhost)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "ovs_del_tap name=%s vhost=%s", name, vhost);
  result = ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_add_phy(int tid, char *name, char *vhost, char *mac, int num_macvlan)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "ovs_add_phy name=%s vhost=%s mac=%s num_macvlan=%d",
  name, vhost, mac, num_macvlan);
  result = ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_del_phy(int tid, char *name, char *vhost, int num_macvlan)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "ovs_del_phy name=%s vhost=%s num_macvlan=%d",
  name, vhost, num_macvlan);
  result = ovs_try_send_sigdiag_msg(tid, cmd);
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
  "ovs_add_lan lan=%s", lan);
  result = ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_add_lan_rstp(int tid, char *lan)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "ovs_add_lan_rstp lan=%s", lan);
  result = ovs_try_send_sigdiag_msg(tid, cmd);
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
  "ovs_del_lan lan=%s", lan);
  result = ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_add_lan_endp(int tid, char *name, int num, char *vhost, char *lan)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "ovs_add_lan_endp name=%s num=%d vhost=%s lan=%s", name, num, vhost, lan);
  result = ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_macvlan_mac(int type, char *macv, char *nmac)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "ovs_change_macvlan_mac type=%d macv=%s nmac=%s", type, macv, nmac);
  result = ovs_try_send_sigdiag_msg(0, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int fmt_tx_del_lan_endp(int tid, char *name, int num, char *vhost, char *lan)
{
  int result;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "ovs_del_lan_endp name=%s num=%d vhost=%s lan=%s", name, num, vhost, lan);
  result = ovs_try_send_sigdiag_msg(tid, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void fmt_rx_rpct_recv_sigdiag_msg(int llid, int tid, char *line)
{
  int num;
  char mac[MAX_NAME_LEN];
  char lan[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  char vhost[MAX_NAME_LEN];

  if (sscanf(line, 
  "OK ovs_add_lan lan=%s", lan) == 1)
    msg_ack_lan(tid, 0, 1, lan);
  else if (sscanf(line,
  "KO ovs_add_lan lan=%s", lan) == 1)
    msg_ack_lan(tid, 1, 1, lan);
  else if (sscanf(line,
  "OK ovs_del_lan lan=%s", lan) == 1)
    msg_ack_lan(tid, 0, 0, lan);
  else if (sscanf(line,
  "KO ovs_del_lan lan=%s", lan) == 1)
    msg_ack_lan(tid, 1, 0, lan);

  else if (sscanf(line,
  "OK ovs_add_tap name=%s vhost=%s mac=%s", name, vhost, mac) == 3)
    msg_ack_tap(tid, 0, 1, name, vhost, mac);
  else if (sscanf(line,
  "KO ovs_add_tap name=%s vhost=%s mac=%s", name, vhost, mac) == 3)
    msg_ack_tap(tid, 1, 1, name, vhost, mac);
  else if (sscanf(line,
  "OK ovs_del_tap name=%s", name) == 1)
    msg_ack_tap(tid, 0, 0, name, NULL, NULL);
  else if (sscanf(line,
  "KO ovs_del_tap name=%s", name) == 1)
    msg_ack_tap(tid, 1, 0, name, NULL, NULL);

  else if (sscanf(line,
  "OK ovs_add_phy name=%s vhost=%s mac=%s num_macvlan=%d",
  name, vhost, mac, &num) == 4)
    msg_ack_phy(tid, 0, 1, name, num, vhost);
  else if (sscanf(line,
  "KO ovs_add_phy name=%s vhost=%s mac=%s num_macvlan=%d",
  name, vhost, mac, &num) == 4)
    msg_ack_phy(tid, 1, 1, name, num, vhost);
  else if (sscanf(line,
  "OK ovs_del_phy name=%s vhost=%s num_macvlan=%d",
  name, vhost, &num) == 3)
    msg_ack_phy(tid, 0, 0, name, num, vhost);
  else if (sscanf(line,
  "KO ovs_del_phy name=%s vhost=%s num_macvlan=%d",
  name, vhost, &num) == 3)
    msg_ack_phy(tid, 1, 0, name, num, vhost);

  else if (sscanf(line,
  "OK ovs_vhost_up name=%s num=%d vhost=%s", name, &num, vhost) == 3)
    msg_ack_vhost_up(tid, 0, name, num, vhost);
  else if (sscanf(line,
  "KO ovs_vhost_up name=%s num=%d vhost=%s", name, &num, vhost) == 3)
    msg_ack_vhost_up(tid, 1, name, num, vhost);

  else if (sscanf(line,
  "OK ovs_add_snf_lan name=%s num=%d vhost=%s lan=%s", 
    name, &num, vhost, lan) == 4)
    msg_ack_snf_ethv(tid, 0, 1, name, num, vhost, lan);
  else if (sscanf(line,
  "KO ovs_add_snf_lan name=%s num=%d vhost=%s lan=%s",
    name, &num, vhost, lan) == 4)
    msg_ack_snf_ethv(tid, 1, 1, name, num, vhost, lan);
  else if (sscanf(line,
  "OK ovs_del_snf_lan name=%s num=%d vhost=%s lan=%s",
    name, &num, vhost, lan) == 4)
    msg_ack_snf_ethv(tid, 0, 0, name, num, vhost, lan);
  else if (sscanf(line,
  "KO ovs_del_snf_lan name=%s num=%d vhost=%s lan=%s",
    name, &num, vhost, lan) == 4)
    msg_ack_snf_ethv(tid, 1, 0, name, num, vhost, lan);

  else if (sscanf(line, 
  "OK ovs_add_lan_endp name=%s num=%d vhost=%s lan=%s",
    name, &num, vhost, lan) == 4)
    msg_ack_lan_endp(tid, 0, 1, name, num, vhost, lan);
  else if (sscanf(line, 
  "KO ovs_add_lan_endp name=%s num=%d vhost=%s lan=%s",
    name, &num, vhost, lan) == 4)
    msg_ack_lan_endp(tid, 1, 1, name, num, vhost, lan);
  else if (sscanf(line,
  "OK ovs_del_lan_endp name=%s num=%d vhost=%s lan=%s",
    name, &num, vhost, lan) == 4)
    msg_ack_lan_endp(tid, 0, 0, name, num, vhost, lan);
  else if (sscanf(line,
  "KO ovs_del_lan_endp name=%s num=%d vhost=%s lan=%s",
    name, &num, vhost, lan) == 4)
    msg_ack_lan_endp(tid, 1, 0, name, num, vhost, lan);
  else if (sscanf(line,
  "OK ovs_system_promisc vhost=%s", vhost) == 1)
    msg_ack_system_promisc(tid, 0, vhost);
  else if (sscanf(line,
  "KO ovs_system_promisc vhost=%s", vhost) == 1)
    msg_ack_system_promisc(tid, 1, vhost);
  else
    KERR("ERROR: %s", line);
}
/*--------------------------------------------------------------------------*/

