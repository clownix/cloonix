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

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_daemon.h"

#include "doors_rpc.h"
#include "cfg_store.h"
#include "automates.h"
#include "dpdk_ovs.h"
#include "dpdk_xyx.h"
#include "suid_power.h"
#include "hop_event.h"
#include "nat_dpdk_process.h"
#include "d2d_dpdk_process.h"
#include "a2b_dpdk_process.h"
#include "xyx_dpdk_process.h"


/*****************************************************************************/
void rpct_recv_poldiag_msg(int llid, int tid, char *line)
{
  char name[MAX_NAME_LEN];
  int ms, ptx, btx, prx, brx; 

  if (get_glob_req_self_destruction())
    return;

  if (suid_power_diag_llid(llid))
    {
    hop_event_hook(llid, FLAG_HOP_POLDIAG, line);
    suid_power_poldiag_resp(llid, tid, line);
    }
  else
    {
    if (sscanf(line, "endp_eventfull_tx_rx %s %d %d %d %d %d",
                     name, &ms, &ptx, &btx, &prx, &brx) == 6)
      {
      if (!dpdk_xyx_exists_fullname(name))
        KERR("ERROR DISPATCH %s", line);
      else
        dpdk_xyx_eventfull(name, ms, ptx, btx, prx, brx);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_sigdiag_msg(int llid, int tid, char *line)
{
  if (get_glob_req_self_destruction())
    return;

  if (suid_power_diag_llid(llid))
    {
    hop_event_hook(llid, FLAG_HOP_SIGDIAG, line);
    suid_power_sigdiag_resp(llid, tid, line);
    }
  else if (xyx_dpdk_diag_llid(llid))
    {
    hop_event_hook(llid, FLAG_HOP_SIGDIAG, line);
    xyx_dpdk_sigdiag_resp(llid, tid, line);
    }
  else if (nat_dpdk_diag_llid(llid))
    {
    hop_event_hook(llid, FLAG_HOP_SIGDIAG, line);
    nat_dpdk_sigdiag_resp(llid, tid, line);
    }
  else if (a2b_dpdk_diag_llid(llid))
    {
    hop_event_hook(llid, FLAG_HOP_SIGDIAG, line);
    a2b_dpdk_sigdiag_resp(llid, tid, line);
    }
  else if (d2d_dpdk_diag_llid(llid))
    {
    hop_event_hook(llid, FLAG_HOP_SIGDIAG, line);
    d2d_dpdk_sigdiag_resp(llid, tid, line);
    }
  else if (dpdk_ovs_find_with_llid(llid))
    {
    hop_event_hook(llid, FLAG_HOP_SIGDIAG, line);
    dpdk_ovs_rpct_recv_sigdiag_msg(llid, tid, line);
    }
  else
    {
    KERR("CANNOT DISPATCH %s", line);
    }
}
/*---------------------------------------------------------------------------*/






