/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
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
#include "ovs_nat_main.h"
#include "ovs_c2c.h"
#include "ovs_a2b.h"

void rpct_recv_poldiag_msg(int llid, int tid, char *line)
{
  if (get_glob_req_self_destruction())
    return;

  if (suid_power_diag_llid(llid))
    {
    suid_power_poldiag_resp(llid, tid, line);
    }
  else if (ovs_snf_diag_llid(llid))
    {
    ovs_snf_poldiag_resp(llid, tid, line);
    }
  else if (ovs_nat_main_diag_llid(llid))
    {
    ovs_nat_main_poldiag_resp(llid, tid, line);
    }
  else if (ovs_c2c_diag_llid(llid))
    {
    ovs_c2c_poldiag_resp(llid, tid, line);
    }
  else
    {
    static unsigned long nb_undispatched;
    nb_undispatched += 1;
    if ((nb_undispatched % 100) == 1)
      KERR("ERROR DISPATCH on llid %d, nothing owns it (%lu such messages "
           "so far): %s", llid, nb_undispatched, line);
    }
}

void rpct_recv_sigdiag_msg(int llid, int tid, char *line)
{
  if (get_glob_req_self_destruction())
    return;

  if (suid_power_diag_llid(llid))
    {
    suid_power_sigdiag_resp(llid, tid, line);
    }
  else if (ovs_find_with_llid(llid))
    {
    ovs_rpct_recv_sigdiag_msg(llid, tid, line);
    }
  else if (ovs_snf_diag_llid(llid))
    {
    ovs_snf_sigdiag_resp(llid, tid, line);
    }
  else if (ovs_nat_main_diag_llid(llid))
    {
    ovs_nat_main_sigdiag_resp(llid, tid, line);
    }
  else if (ovs_c2c_diag_llid(llid))
    {
    ovs_c2c_sigdiag_resp(llid, tid, line);
    }
  else if (ovs_a2b_diag_llid(llid))
    {
    ovs_a2b_sigdiag_resp(llid, tid, line);
    }
  else
    {
    KERR("ERROR no handler for the diag message %s", line);
    }
}

