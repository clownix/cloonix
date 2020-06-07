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
enum {
  state_min = 0,
  state_master_waiting_reconnect,
  state_slave_waiting_reconnect,
  state_master_waiting_idx,
  state_slave_waiting_idx,
  state_peered,
  state_deleting,
  state_max,
};

struct t_sc2c;

typedef void (*t_fct_timeout_cb)(struct t_sc2c *sc2c);

typedef struct t_timer_c2c
{
  char name[MAX_NAME_LEN];
  int token_id;
  t_fct_timeout_cb callback;
} t_timer_c2c;

typedef struct t_sc2c
{
  char name[MAX_NAME_LEN];
  int peer_switch_llid;
  int connect_llid;
  char master_cloonix[MAX_NAME_LEN];
  char slave_cloonix[MAX_NAME_LEN];
  char slave_passwd[MSG_DIGEST_LEN];
  int local_is_master;
  int master_idx;
  int slave_idx;
  int state;
  int timer_token_id;
  long long timer_abs_beat;
  int timer_ref;
  int ping_timer_count;
  uint32_t ip_slave; 
  int port_slave;
  struct t_sc2c *prev;
  struct t_sc2c *next;
} t_sc2c;

void c2c_del_timer(t_sc2c *c2c);
void c2c_arm_timer(t_sc2c *c2c, int val, t_fct_timeout_cb cb);
void c2c_globtopo_small_event(char *name, char *src, char *dst, int is_ok);
int c2c_globtopo_add(t_sc2c *c2c);
t_sc2c *c2c_find(char *name);
t_sc2c *c2c_find_with_llid(int llid);
t_sc2c *c2c_alloc(int local_is_master, char *name, 
                  int ip_slave, int port_slave, 
                  char *passwd_slave);
void c2c_free_ctx(char *name);
void c2c_arm_ping_timer(char *name);
void c2c_set_state(t_sc2c *c2c, int state);
void c2c_tx_req_idx_to_doors(char *name);
void c2c_tx_req_conx_to_doors(char *name, int peer_idx, 
                              int ip, int port, char *passwd);
void c2c_send_status_client(t_sc2c *c2c, int status, char *info);
void c2c_abort_error(t_sc2c *c2c, char *fct, int line);
void c2c_utils_init(void);
void time_try_connect_peer(t_sc2c *c2c);
void peer_doorways_client_tx(int llid, int len, char *buf);
/*--------------------------------------------------------------------------*/




