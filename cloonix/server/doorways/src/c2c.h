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
typedef struct t_c2c
{
  char name[MAX_NAME_LEN];
  char tux_path[MAX_PATH_LEN];
  char passwd[MSG_DIGEST_LEN];
  int  peer_idx;
  uint32_t peer_ip;
  int  peer_port;
  int  connect_llid;
  long long connect_abs_beat_timer;
  int connect_ref_timer;
  int  dido_llid;
  int  tux_llid;
  int  local_idx;
  int  count_zero;
  struct t_c2c *prev;
  struct t_c2c *next;
} t_c2c;
/*--------------------------------------------------------------------------*/
void c2c_from_switch_req_idx(char *name);
void c2c_from_switch_req_free(char *name);
void c2c_from_switch_req_conx(char *name, int peer_idx, 
                              uint32_t peer_ip, int peer_port, char *passwd);
void c2c_server_dispach_rx_init(int dido_llid, int val, int len, char *buf);
void c2c_server_dispach_rx_traf(int dido_llid, int val, int len, char *buf);
t_c2c *c2c_get_llid_ctx(int llid);
void c2c_set_llid_ctx(int llid, t_c2c *ctx);
void c2c_init(void);
/*--------------------------------------------------------------------------*/

