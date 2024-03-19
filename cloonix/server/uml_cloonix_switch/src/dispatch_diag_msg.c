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
#include "suid_power.h"
#include "hop_event.h"
#include "ovs.h"
#include "ovs_snf.h"
#include "ovs_nat.h"
#include "ovs_c2c.h"
#include "ovs_a2b.h"


/*****************************************************************************/
void rpct_recv_poldiag_msg(int llid, int tid, char *line)
{
  if (get_glob_req_self_destruction())
    return;

  if (suid_power_diag_llid(llid))
    {
    hop_event_hook(llid, FLAG_HOP_POLDIAG, line);
    suid_power_poldiag_resp(llid, tid, line);
    }
  else if (ovs_snf_diag_llid(llid))
    {
    hop_event_hook(llid, FLAG_HOP_POLDIAG, line);
    ovs_snf_poldiag_resp(llid, tid, line);
    }
  else if (ovs_nat_diag_llid(llid))
    {
    hop_event_hook(llid, FLAG_HOP_POLDIAG, line);
    ovs_nat_poldiag_resp(llid, tid, line);
    }
  else if (ovs_c2c_diag_llid(llid))
    {
    hop_event_hook(llid, FLAG_HOP_POLDIAG, line);
    ovs_c2c_poldiag_resp(llid, tid, line);
    }
  else
    {
    KERR("ERROR DISPATCH %s", line);
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
  else if (ovs_find_with_llid(llid))
    {
    hop_event_hook(llid, FLAG_HOP_SIGDIAG, line);
    ovs_rpct_recv_sigdiag_msg(llid, tid, line);
    }
  else if (ovs_snf_diag_llid(llid))
    {
    hop_event_hook(llid, FLAG_HOP_SIGDIAG, line);
    ovs_snf_sigdiag_resp(llid, tid, line);
    }
  else if (ovs_nat_diag_llid(llid))
    {
    hop_event_hook(llid, FLAG_HOP_SIGDIAG, line);
    ovs_nat_sigdiag_resp(llid, tid, line);
    }
  else if (ovs_c2c_diag_llid(llid))
    {
    hop_event_hook(llid, FLAG_HOP_SIGDIAG, line);
    ovs_c2c_sigdiag_resp(llid, tid, line);
    }
  else if (ovs_a2b_diag_llid(llid))
    {
    hop_event_hook(llid, FLAG_HOP_SIGDIAG, line);
    ovs_a2b_sigdiag_resp(llid, tid, line);
    }
  else
    {
    KERR("CANNOT DISPATCH %s", line);
    }
}
/*---------------------------------------------------------------------------*/






