/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_daemon.h"
#include "cfg_store.h"
#include "hop_event.h"
#include "doorways_mngt.h"
#include "ovs.h"
#include "ovs_snf.h"
#include "ovs_nat_main.h"
#include "ovs_a2b.h"
#include "ovs_c2c.h"
#include "suid_power.h"
#include "gui_server_mngt.h"
#include "proxymous.h"

void rpct_recv_pid_resp(int llid, int tid, char *name, int num,
                        int toppid, int pid)
{
  if (tid == type_hop_suid_power)
    suid_power_pid_resp(llid, tid, name, pid);
  else if (tid == type_hop_gui_server)
    gui_server_pid_resp(llid, tid, name, num, toppid, pid);
  else if (tid == type_hop_doors)
    doors_pid_resp(llid, name, pid);
  else if (tid == type_hop_snf)
    ovs_snf_pid_resp(llid, name, pid);
  else if (tid == type_hop_nat_main)
    ovs_nat_main_pid_resp(llid, name, pid);
  else if (tid == type_hop_proxymous)
    proxymous_pid_resp(llid, name, pid);
  else if (tid == type_hop_a2b)
    ovs_a2b_pid_resp(llid, name, pid);
  else if (tid == type_hop_c2c)
    ovs_c2c_pid_resp(llid, name, pid);
  else if ((tid == type_hop_ovs) || (tid == type_hop_ovsdb)) 
    ovs_pid_resp(llid, name, toppid, pid);
  else
    KERR("%d %s %d", tid, name, pid);
}
