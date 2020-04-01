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
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doors_rpc.h"
#include "sock.h"
#include "llid_traffic.h"
#include "llid_x11.h"
#include "llid_backdoor.h"
#include "stats.h"

int get_doorways_llid(void);


/*****************************************************************************/
static void dump_tx_queue_sizes(void)
{
  int i, llid, pqsiz, qsiz;
  int nb_chan = get_clownix_channels_nb();
  for (i=0; i<=nb_chan; i++)
    {
    llid = channel_get_llid_with_cidx(i);
    if (llid > 0)
      {
      pqsiz = (int) msg_get_tx_peak_queue_len(llid);
      qsiz = (int) channel_get_tx_queue_len(llid);
      if (qsiz > pqsiz)
        pqsiz = qsiz;
      if (pqsiz > 10000000)
        {
        KERR("Big Tx Queue: %d", pqsiz);
        }
      }
    else if (llid < 0)
      KOUT("%d", llid);
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int get_sysinfo_vals(char *msg, unsigned long *uptime,
                            unsigned long *load1, unsigned long *load5, 
                            unsigned long *load15, unsigned long *totalram, 
                            unsigned long *freeram, unsigned long *cachedram, 
                            unsigned long *sharedram,
                            unsigned long *bufferram, unsigned long *totalswap,
                            unsigned long *freeswap, unsigned long *procs,
                            unsigned long *totalhigh, unsigned long *freehigh,
                            unsigned long *mem_unit)
{
  int result = -1;
  if (sscanf(msg, SYSINFOFORMAT,
                  uptime, load1, load5, load15, totalram, freeram, cachedram,
                  sharedram, bufferram, totalswap, freeswap, procs, 
                  totalhigh, freehigh, mem_unit) == 15)
    result = 0;
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void set_sysinfo_vals(char *msg, int max, unsigned long uptime,
                            unsigned long load1, unsigned long load5,
                            unsigned long load15, unsigned long totalram,
                            unsigned long freeram, unsigned long cachedram, 
                            unsigned long sharedram,
                            unsigned long bufferram, unsigned long totalswap,
                            unsigned long freeswap, unsigned long procs,
                            unsigned long totalhigh, unsigned long freehigh,
                            unsigned long mem_unit)
{
  msg[max] = 0;
  snprintf(msg, max-1, "%s " SYSINFOFORMAT,
                       AGENT_SYSINFO, uptime, load1, load5, load15, 
                       totalram, freeram, cachedram, sharedram,
                       bufferram, totalswap, freeswap, 
                       procs, totalhigh, freehigh, mem_unit);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void set_sysinfo_df(char *msg, int max, char * payload)
{
  msg[max] = 0;
  snprintf(msg, max-1, "%s \n%s", AGENT_SYSINFO_DF, payload); 
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void stats_from_agent_process(char *name, int header_val, 
                              int paylen,  char *payload)
{
  char msg[MAX_RPC_MSG_LEN];
  char *sys;
  unsigned long uptime, load1, load5, load15, totalram, freeram;
  unsigned long cachedram, sharedram, bufferram, totalswap, freeswap;
  unsigned long procs, totalhigh, freehigh, mem_unit; 

  if (header_val == header_val_sysinfo)
    {
    sys = (char *) payload;
    if (get_sysinfo_vals(sys, &uptime, &load1, &load5, &load15, &totalram,
                         &freeram, &cachedram, &sharedram, &bufferram, 
                         &totalswap, &freeswap, &procs, &totalhigh, &freehigh,
                         &mem_unit))
      {
      KERR("%s %s", name, sys);
      }
    else
      {
      set_sysinfo_vals(msg, MAX_RPC_MSG_LEN-1, uptime, 
                                           load1, load5, load15, totalram,
                       freeram, cachedram, sharedram, bufferram, totalswap,
                       freeswap, procs, totalhigh, freehigh, mem_unit);
      doors_send_event(get_doorways_llid(), 0, name, msg);
      }
    }
  else if (header_val == header_val_sysinfo_df)
    {
    set_sysinfo_df(msg, MAX_RPC_MSG_LEN-1, (char *) payload);
    doors_send_event(get_doorways_llid(), 0, name, msg);
    }
  else
    KERR("%d", header_val);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void stats_heartbeat(int count)
{
  char *buf = get_gbuf();
  int  headsize = sock_header_get_size();
  char *name;
  void *cur, *next;
  int backdoor_llid;
  memset(buf, 0, MAX_A2D_LEN);
  strcpy(buf+headsize, LASTATS);
  next = llid_backdoor_get_first(&name, &backdoor_llid); 
  while (next)
    {
    cur = next;
    llid_backdoor_tx(name, backdoor_llid, 0, strlen(LASTATS) + 1,
                     header_type_stats, header_val_sysinfo, buf);
    next = llid_backdoor_get_next(&name, &backdoor_llid, cur);
    }
  memset(buf, 0, MAX_A2D_LEN);
  strcpy(buf+headsize, LASTATSDF);
  next = llid_backdoor_get_first(&name, &backdoor_llid);
  while (next)
    {
    cur = next;
    llid_backdoor_tx(name, backdoor_llid, 0, strlen(LASTATSDF) + 1,
                     header_type_stats, header_val_sysinfo_df, buf);
    next = llid_backdoor_get_next(&name, &backdoor_llid, cur);
    }
  dump_tx_queue_sizes();
}
/*--------------------------------------------------------------------------*/

