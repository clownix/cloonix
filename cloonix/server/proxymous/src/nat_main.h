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
typedef struct t_ctx_nat t_ctx_nat;


/*--------------------------------------------------------------------------*/
typedef struct t_icmp_flow
{ 
  t_ctx_nat *ctx;
  uint32_t ipdst;
  uint16_t seq;
  uint16_t ident;
  uint32_t count;
  int llid; 
  struct sockaddr_in addr;
  struct t_icmp_flow *prev;
  struct t_icmp_flow *next;
} t_icmp_flow;
/*--------------------------------------------------------------------------*/
typedef struct t_llid_icmp
{
  t_icmp_flow *item;
  struct t_llid_icmp *prev;
  struct t_llid_icmp *next;
} t_llid_icmp;
/*--------------------------------------------------------------------------*/
typedef struct t_udp_flow
{
  t_ctx_nat *ctx;
  char dgram_rx[MAX_PATH_LEN];
  char dgram_tx[MAX_PATH_LEN];
  uint32_t sip;
  uint32_t dip;
  uint32_t dest_ip;
  uint16_t sport;
  uint16_t dport;
  int llidrx;
  int fdrx;
  int llidtx;
  int fdtx;
  int inactivity_count;
  struct t_udp_flow *prev;
  struct t_udp_flow *next;
} t_udp_flow;
/*--------------------------------------------------------------------------*/
typedef struct t_llid_udp
{
  t_udp_flow *item;
  struct t_llid_udp *prev;
  struct t_llid_udp *next;
} t_llid_udp;
/*--------------------------------------------------------------------------*/
typedef struct t_tcp_flow
{
  t_ctx_nat *ctx;
  char stream[MAX_PATH_LEN];
  uint32_t sip;
  uint32_t dip;
  uint16_t sport;
  uint16_t dport;
  int count;
  int stopped;
  int fd;
  int llid_inet;
  int llid_unix;
  int delayed_count;
  int inactivity_count;
  struct t_tcp_flow *prev;
  struct t_tcp_flow *next;
} t_tcp_flow;
/*--------------------------------------------------------------------------*/
typedef struct t_llid_tcp
{
  t_tcp_flow *item;
  struct t_llid_tcp *prev;
  struct t_llid_tcp *next;
} t_llid_tcp;
/*--------------------------------------------------------------------------*/
typedef struct t_ctx_nat
{
  int llid_listen;
  int llid_sig;
  char nat[MAX_NAME_LEN];
  char nat_prxy_path[MAX_PATH_LEN];
  t_icmp_flow *head_icmp;
  t_udp_flow *head_udp;
  t_tcp_flow *head_tcp;
  struct t_ctx_nat *prev;
  struct t_ctx_nat *next;
} t_ctx_nat;
/*--------------------------------------------------------------------------*/
char *get_proxymous_root_path(void);
char *get_net_name(void);
t_ctx_nat *get_ctx_nat(char *nat);
int  nat_main_request(char *name, int on);
void nat_main_kil_req(void);
void nat_main_init(void);
/*--------------------------------------------------------------------------*/
