/*****************************************************************************/
/*    Copyright (C) 2006-2018 cloonix@cloonix.net License AGPL-3             */
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
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "rpc_c2c.h"
#include "doorways_mngt.h"
#include "doors_rpc.h"
#include "doorways_sock.h"
#include "cfg_store.h"
#include "event_subscriber.h"
#include "lan_to_name.h"
#include "c2c_utils.h"
#include "llid_trace.h"
#include "utils_cmd_line_maker.h"
#include "commun_daemon.h"
#include "endp_mngt.h"
#include "endp_evt.h"


/****************************************************************************/
static void time_resp_conx_from_doors(t_sc2c *c2c)
{
  c2c_abort_error(c2c,(char *) __FUNCTION__, __LINE__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void time_resp_idx_from_doors(t_sc2c *c2c)
{
  c2c_abort_error(c2c,(char *) __FUNCTION__, __LINE__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void time_resp_from_create_peer(t_sc2c *c2c)
{
  c2c_abort_error(c2c,(char *) __FUNCTION__, __LINE__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void time_c2c_create_end(t_sc2c *c2c)
{
  c2c_abort_error(c2c,(char *) __FUNCTION__, __LINE__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int c2c_exists(char *name)
{
  int result = 0;
  if (c2c_find(name))
    result = 1;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void recv_c2c_peer_ping(int llid, int tid, char *name)
{
  t_sc2c *c2c = c2c_find(name);
  if (c2c)
    {
    c2c_globtopo_small_event(name, c2c->master_cloonix, c2c->slave_cloonix, 
                             c2c_evt_connection_ok);
    c2c_set_state(c2c, state_peered);
    c2c->ping_timer_count = 0;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void recv_c2c_peer_create(int llid, int tid, char *name,
                          char *master_cloonix, int master_idx,
                          int ip_slave, int port_slave)
{
  int type;
  char info[MAX_PATH_LEN];
  t_sc2c *c2c = c2c_find(name);

  event_print("slave create c2c %s from %s "
              "ip_slave:%X  port_slave:%d",
               name, master_cloonix, ip_slave, port_slave);

  memset(info, 0, MAX_PATH_LEN);
  if (c2c)
    {
    if (c2c->state == state_slave_waiting_reconnect)
      {
      c2c->peer_switch_llid = llid;
      c2c->master_idx = master_idx;
      c2c_set_state(c2c, state_slave_waiting_idx);
      strncpy(c2c->master_cloonix, master_cloonix, MAX_NAME_LEN-1);
      c2c_tx_req_idx_to_doors(name);
      c2c_arm_timer(c2c, 1500, time_resp_idx_from_doors);
      }
    else
      {
      c2c_abort_error(c2c, (char *)__FUNCTION__, __LINE__);
      send_c2c_ack_peer_create(llid,0,name,"none","none",0,0,1);
      }
    }
  else
    {
    if (endp_mngt_exists(name, 0, &type))
      {
      KERR("%s", name);
      send_c2c_ack_peer_create(llid,0,name,"none","none",0,0,1);
      }
    else
      {
      c2c = c2c_alloc(0, name, ip_slave, port_slave, NULL); 
      c2c->peer_switch_llid = llid;
      c2c->master_idx = master_idx;
      strncpy(c2c->master_cloonix, master_cloonix, MAX_NAME_LEN-1);
      strncpy(c2c->slave_cloonix, cfg_get_cloonix_name(), MAX_NAME_LEN-1);
      if (endp_mngt_start(0, 0, name, 0, endp_type_c2c))
        {
        KERR("Bad start of %s", name);
        send_c2c_ack_peer_create(llid,0,name,"none","none",0,0,1);
        c2c_free_ctx(name);
        }
      else
        {
        c2c_set_state(c2c, state_slave_waiting_idx);
        c2c_tx_req_idx_to_doors(name);
        c2c_arm_timer(c2c, 500, time_c2c_create_end);
        }
      if (c2c_globtopo_add(c2c))
        KERR("%s", name);
      event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void recv_c2c_ack_peer_create(int llid, int tid, char *name,
                              char *master_cloonix, char *slave_cloonix,
                              int master_idx, int slave_idx, int status)
{
  t_sc2c *c2c = c2c_find(name);
  if (c2c)
    {
    c2c_del_timer(c2c);
    if (status)
      c2c_abort_error(c2c, (char *) __FUNCTION__, __LINE__);
    else if (!c2c->local_is_master)
      c2c_abort_error(c2c, (char *) __FUNCTION__, __LINE__);
    else if (strcmp(master_cloonix, cfg_get_cloonix_name()))
      c2c_abort_error(c2c, (char *) __FUNCTION__, __LINE__);
    else if (llid != c2c->peer_switch_llid)
      c2c_abort_error(c2c, (char *) __FUNCTION__, __LINE__);
    else if (c2c->master_idx != master_idx)
      c2c_abort_error(c2c, (char *) __FUNCTION__, __LINE__);
    else
      {
      strncpy(c2c->slave_cloonix, slave_cloonix, MAX_NAME_LEN-1);
      c2c->slave_idx = slave_idx;
      c2c_tx_req_conx_to_doors(c2c->name, c2c->slave_idx, 
                               c2c->ip_slave, 
                               c2c->port_slave, c2c->slave_passwd);
      c2c_arm_timer(c2c, 1500, time_resp_conx_from_doors);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void recv_c2c_peer_delete(int llid, int tid, char *name)
{
  t_sc2c *c2c = c2c_find(name);
  if (c2c)
    {
    c2c_free_ctx(name);
    }
}
/*-------------------------------------------------------------------------*/

/****************************************************************************/
void rx_c2c_resp_idx_from_doors(char *name, int local_idx)
{
  t_sc2c *c2c = c2c_find(name);
  if (!c2c)
    KERR("%s", name);
  else
    { 
    c2c_del_timer(c2c);
    if (local_idx == 0)
      {
      c2c_abort_error(c2c, (char *)__FUNCTION__, __LINE__);
      }
    else
      {
      if (!msg_exist_channel(c2c->peer_switch_llid))
        {
        c2c_abort_error(c2c, (char *)__FUNCTION__, __LINE__);
        }
      else
        {
        if (c2c->state == state_master_waiting_idx)
          {
          if (!c2c->local_is_master)
            {
            KERR(" ");
            c2c_abort_error(c2c, (char *)__FUNCTION__, __LINE__);
            }
          else
            {
            strncpy(c2c->master_cloonix,cfg_get_cloonix_name(),MAX_NAME_LEN-1);
            doors_io_c2c_tx_set(peer_doorways_client_tx);
            send_c2c_peer_create(c2c->peer_switch_llid, 0, c2c->name,
                                 c2c->master_cloonix, local_idx,
                                 c2c->ip_slave, c2c->port_slave);
            doors_io_c2c_tx_set(string_tx);
            c2c->master_idx = local_idx;
            c2c_arm_timer(c2c, 1500, time_resp_from_create_peer);
            }
          }
        else if (c2c->state == state_slave_waiting_idx)
          {
          if (c2c->local_is_master)
            {
            KERR(" ");
            c2c_abort_error(c2c, (char *)__FUNCTION__, __LINE__);
            }
          else if (!c2c->master_idx)
            {
            KERR(" ");
            c2c_abort_error(c2c, (char *)__FUNCTION__, __LINE__);
            }
          else if (!c2c->peer_switch_llid)
            {
            KERR(" ");
            c2c_abort_error(c2c, (char *)__FUNCTION__, __LINE__);
            }
          else
            {
            c2c->slave_idx = local_idx;
            strncpy(c2c->slave_cloonix, cfg_get_cloonix_name(), MAX_NAME_LEN-1);
            send_c2c_ack_peer_create(c2c->peer_switch_llid, 0, c2c->name, 
                                     c2c->master_cloonix, c2c->slave_cloonix, 
                                     c2c->master_idx, c2c->slave_idx, 0);
            c2c_tx_req_conx_to_doors(c2c->name, c2c->master_idx, 0, 0, NULL);
            c2c_arm_timer(c2c, 1500, time_resp_conx_from_doors);
            }
          }
        else
          c2c_abort_error(c2c, (char *)__FUNCTION__, __LINE__);
        }
      }
    }
} 
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rx_c2c_event_conx_from_doors(char *name, int fd,  int status) 
{
  t_sc2c *c2c = c2c_find(name);
  if (c2c)
    {
    if (!status)
      {
      c2c_del_timer(c2c);
      c2c_arm_ping_timer(name);
      if (c2c->local_is_master)
        {
        if (c2c_globtopo_add(c2c))
          KERR("%s", name);
        c2c_globtopo_small_event(name, c2c->master_cloonix, 
                                 c2c->slave_cloonix, c2c_evt_mod_master_slave);
        fd_ready_doors_clone_has_arrived(name, fd);
        }
      else
        fd_ready_doors_clone_has_arrived(name, fd);
      }
    else
      KERR("%s", name);
    }
  else
    KERR("%s", name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int c2c_create_master_begin(char *name, 
                            int ip_slave, int port_slave, char *passwd_slave)
{
  int result = -1;
  char info[MAX_PATH_LEN];
  t_sc2c *c2c;
  memset(info, 0, MAX_PATH_LEN);
  event_print("master create c2c %s "
              "ip_slave:%X  port_slave:%d",
               name,  ip_slave, port_slave);
  c2c = c2c_alloc(1, name, ip_slave, port_slave, passwd_slave);
  c2c_arm_timer(c2c, 10, time_try_connect_peer);
  if (c2c_globtopo_add(c2c))
    KERR("%s", name);
  result = 0;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void init_c2c(void)
{
  c2c_utils_init();
}
/*--------------------------------------------------------------------------*/




