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
void udp_tx_sig(int len, uint8_t *buf);
int  udp_get_traffic_mngt(void);
void udp_enter_traffic_mngt(void);
void udp_close(void);
void udp_fill_dist_addr(uint32_t ip, uint16_t udp_port);
int  udp_tx_burst(int nb, struct rte_mbuf **mbuf);
int  udp_rx_burst(int *nb, struct rte_mbuf **mbuf, struct rte_mempool *mpool);
int  udp_init(void);
/*--------------------------------------------------------------------------*/
