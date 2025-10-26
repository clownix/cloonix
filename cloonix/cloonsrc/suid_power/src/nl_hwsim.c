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
#include <sys/types.h>
#include <errno.h>

#include <netlink/genl/ctrl.h>
#include <netlink/genl/genl.h>
#include <netlink/netlink.h>
#include <net/ethernet.h>

#include "io_clownix.h"

#define VERSION_NR 1

enum hwsim_commands {
        HWSIM_CMD_UNSPEC,
        HWSIM_CMD_REGISTER,
        HWSIM_CMD_FRAME,
        HWSIM_CMD_TX_INFO_FRAME,
        HWSIM_CMD_NEW_RADIO,
        HWSIM_CMD_DEL_RADIO,
        HWSIM_CMD_GET_RADIO,
        HWSIM_CMD_ADD_MAC_ADDR,
        HWSIM_CMD_DEL_MAC_ADDR,
        HWSIM_CMD_START_PMSR,
        HWSIM_CMD_ABORT_PMSR,
        HWSIM_CMD_REPORT_PMSR,
        __HWSIM_CMD_MAX,
};

enum hwsim_attrs {
        HWSIM_ATTR_UNSPEC,
        HWSIM_ATTR_ADDR_RECEIVER,
        HWSIM_ATTR_ADDR_TRANSMITTER,
        HWSIM_ATTR_FRAME,
        HWSIM_ATTR_FLAGS,
        HWSIM_ATTR_RX_RATE,
        HWSIM_ATTR_SIGNAL,
        HWSIM_ATTR_TX_INFO,
        HWSIM_ATTR_COOKIE,
        HWSIM_ATTR_CHANNELS,
        HWSIM_ATTR_RADIO_ID,
        HWSIM_ATTR_REG_HINT_ALPHA2,
        HWSIM_ATTR_REG_CUSTOM_REG,
        HWSIM_ATTR_REG_STRICT_REG,
        HWSIM_ATTR_SUPPORT_P2P_DEVICE,
        HWSIM_ATTR_USE_CHANCTX,
        HWSIM_ATTR_DESTROY_RADIO_ON_CLOSE,
        HWSIM_ATTR_RADIO_NAME,
        HWSIM_ATTR_NO_VIF,
        HWSIM_ATTR_FREQ,
        HWSIM_ATTR_PAD,
        HWSIM_ATTR_TX_INFO_FLAGS,
        HWSIM_ATTR_PERM_ADDR,
        HWSIM_ATTR_IFTYPE_SUPPORT,
        HWSIM_ATTR_CIPHER_SUPPORT,
        HWSIM_ATTR_MLO_SUPPORT,
        HWSIM_ATTR_PMSR_SUPPORT,
        HWSIM_ATTR_PMSR_REQUEST,
        HWSIM_ATTR_PMSR_RESULT,
        HWSIM_ATTR_MULTI_RADIO,
        __HWSIM_ATTR_MAX,
};


/****************************************************************************/
int nl_sock_listen_all_nsid(struct nl_sock *sock)
{
  int val = 1, result = -1;
  int fd = nl_socket_get_fd(sock);
  if (fd < 0)
    KERR("ERROR netlink listening to all nsid errno:%d", errno);
  else if (setsockopt(fd, SOL_NETLINK,NETLINK_LISTEN_ALL_NSID,
                      &val, sizeof val) < 0)
    KERR("ERROR netlink listening to all nsid errno:%d", errno);
  else
    result = 0;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static struct nl_msg *format_nlmsg(int fam, uint8_t *mac)
{
  struct nl_msg *nlmsg = nlmsg_alloc();
  void *user;

  if (nlmsg == NULL)
    KERR("ERROR nlmsg_alloc");
  else
    {
    user = genlmsg_put(nlmsg, NL_AUTO_PORT, NL_AUTO_SEQ, fam, 0,
                       NLM_F_REQUEST, HWSIM_CMD_NEW_RADIO, VERSION_NR);
    if (!user)
      {
      KERR("ERROR genlmsg_put");
      nlmsg_free(nlmsg);
      nlmsg = NULL;
      }
    else if (nla_put(nlmsg, HWSIM_ATTR_PERM_ADDR, ETH_ALEN, mac))
      {
      KERR("ERROR genlmsg_put");
      nlmsg_free(nlmsg);
      nlmsg = NULL;
      }
    else if (nla_put_flag(nlmsg, HWSIM_ATTR_SUPPORT_P2P_DEVICE))
      {
      KERR("ERROR genlmsg_put");
      nlmsg_free(nlmsg);
      nlmsg = NULL;
      }
    else if (nla_put_flag(nlmsg, HWSIM_ATTR_DESTROY_RADIO_ON_CLOSE))
      {
      KERR("ERROR genlmsg_put");
      nlmsg_free(nlmsg);
      nlmsg = NULL;
      }
    } 
  return (nlmsg);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void *nl_hwsim_add_phy(int nb, uint8_t *mac)
{
  struct nl_sock *nlsock = nl_socket_alloc();
  struct nl_msg *nlmsg;
  int i, fam;
  if (!nlsock)
    KERR("ERROR nl_socket_alloc");
  else if (genl_connect(nlsock))
    {
    KERR("ERROR genl_connectc");
    nl_socket_free(nlsock);
    nlsock = NULL;
    }
  else if (nl_sock_listen_all_nsid(nlsock))
    {
    KERR("ERROR listen to all namespace");
    nl_socket_free(nlsock);
    nlsock = NULL;
    }
  else
    {
    fam = genl_ctrl_resolve(nlsock, "MAC80211_HWSIM");
    if (fam < 0)
      {
      KERR("ERROR kernel module mac80211_hwsim not loaded");
      nl_socket_free(nlsock);
      nlsock = NULL;
      }
    else
      {
      for (i = 0; i < nb; ++i)
        {
        if( nb != 1 )
          mac[5] = i;
        nlmsg = format_nlmsg(fam, mac);
        if (nlmsg)
          {
          if (nl_send_auto(nlsock, nlmsg) < 0)
            {
            KERR("ERROR nl_send_auto");
            nl_socket_free(nlsock);
            nlmsg_free(nlmsg);
            nlsock = NULL;
            break;
            }
          nlmsg_free(nlmsg);
          }
        }
      }
    }
  return ((void *) nlsock);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void nl_hwsim_del_phy(void *pdata)
{
  struct nl_sock *nlsock = (struct nl_sock *) pdata;
  if (nlsock != NULL)
    nl_socket_free(nlsock);
}
/*--------------------------------------------------------------------------*/



