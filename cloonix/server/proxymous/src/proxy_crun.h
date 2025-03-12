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
void sig_process_heartbeat(void);
void sig_process_proxy_peer_data_to_crun(char *name, int len, char *buf);
void sig_process_tx_proxy_callback_connect(char *name, int pair_llid);
int get_llid_sig(void);
void ever_select_loop(int sock_sig_stream, char *proxyshare);
char *get_proxyshare(void);

void udp_proxy_dist_udp_ip_port(uint32_t ip, uint16_t udp_port);
void udp_proxy_end(void);
int udp_proxy_init(char *proxyshare, uint16_t *udp_port, uint16_t c2c_udp_port_low);
void sig_process_rx(char *proxyshare, char *sigbuf);
int set_config(char *net, uint16_t pmain, uint16_t pweb, char *passwd);
void X_init(char *proxyshare);  
void pweb_init(char *net, uint16_t pweb);
/*--------------------------------------------------------------------------*/
