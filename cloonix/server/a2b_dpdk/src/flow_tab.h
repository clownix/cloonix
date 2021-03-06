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
struct fivetuple_v4
{
  uint32_t src_ip;
  uint32_t dst_ip;
  uint16_t src_port;
  uint16_t dst_port;
  uint8_t proto;
  uint8_t dir;
} __attribute__((packed));
/*---------------------------------------------------------------------------*/
struct flowexp_data_v4
{
  uint32_t pps;
  uint32_t bps;
  struct fivetuple_v4 key;
  uint8_t unused[3];
} __attribute__((packed));
/*---------------------------------------------------------------------------*/
struct export_entry_v4
{
  struct flowexp_data_v4 fdata;
  uint64_t start_tsc;
  uint64_t prev_pkt_cnt;
  uint64_t prev_byte_cnt;
} __rte_cache_aligned;
/*---------------------------------------------------------------------------*/
struct export_table_v4
{
  uint32_t num;
  struct export_entry_v4 entries[0];
};
/*---------------------------------------------------------------------------*/
struct export_format_v4
{
  uint32_t serial;
  uint32_t flow_cnt;
  struct flowexp_data_v4 fdata[0];
};
/*---------------------------------------------------------------------------*/
struct flow_entry
{
  uint64_t start_tsc;
  uint64_t last_tsc;
  uint64_t pkt_cnt;
  uint64_t byte_cnt;
  struct fivetuple_v4 key;
} __rte_cache_aligned;
/*---------------------------------------------------------------------------*/
struct flow_table
{
  struct rte_hash *handler;
  uint64_t flow_cnt;
  struct flow_entry entries[0];
};
/*---------------------------------------------------------------------------*/
void flow_tab_init(char *name, int flow_mon_port, int flow_table_size);
void app_tx_flow(int direction, uint16_t n_rx, struct rte_mbuf **bufs);
int remove_flow_entry(struct flow_table *table, struct fivetuple_v4 *_key);
void flow_tab_periodic_dump(int flow_table_size);
/*---------------------------------------------------------------------------*/


