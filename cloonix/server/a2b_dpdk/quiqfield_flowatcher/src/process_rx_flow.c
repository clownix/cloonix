#include "flowtable.h"
#include "config.h"

void app_main_loop_rx_flow(uint16_t n_rx, struct rte_mbuf **bufs)
{
	struct rte_mbuf *buf;
	struct rte_ether_hdr *eth_hdr;
	struct rte_ipv4_hdr *rte_ipv4_hdr;
	struct rte_ipv6_hdr *rte_ipv6_hdr;
	struct rte_tcp_hdr *rte_tcp_hdr;
	struct rte_udp_hdr *rte_udp_hdr;
	struct pkt_info pktinfo;
	int32_t ret;
	uint16_t i;

	app_stat.rx_count += n_rx;

	for (i = 0; i < n_rx; i++) {
		buf = bufs[i];

		pktinfo.timestamp = rte_rdtsc();
		pktinfo.pktlen = rte_pktmbuf_pkt_len(buf);

		eth_hdr = rte_pktmbuf_mtod(buf, struct rte_ether_hdr *);

		/* strip rte_vlan_hdr */
		if (eth_hdr->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN)) {
			/* struct rte_vlan_hdr *vh = (struct rte_vlan_hdr *) &eth_hdr[1]; */
			/* buf->ol_flags |= PKT_RX_VLAN_PKT; */
			/* buf->vlan_tci = rte_be_to_cpu_16(vh->vlan_tci); */
			/* memmove(rte_pktmbuf_adj(buf, sizeof(struct rte_vlan_hdr)), */
			/* 		eth_hdr, 2 * ETHER_ADDR_LEN); */
			/* eth_hdr = rte_pktmbuf_mtod(buf, struct rte_ether_hdr *); */
			eth_hdr = (struct rte_ether_hdr *) rte_pktmbuf_adj(buf, sizeof(struct rte_vlan_hdr));
		}

		if (eth_hdr->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)) {
			/* IPv4 */
			pktinfo.type = PKT_IP_TYPE_IPV4;
			rte_ipv4_hdr = (struct rte_ipv4_hdr *) &eth_hdr[1];

			pktinfo.key.v4.src_ip = rte_be_to_cpu_32(rte_ipv4_hdr->src_addr);
			pktinfo.key.v4.dst_ip = rte_be_to_cpu_32(rte_ipv4_hdr->dst_addr);
			pktinfo.key.v4.proto = rte_ipv4_hdr->next_proto_id;

			switch (rte_ipv4_hdr->next_proto_id) {
				case IPPROTO_TCP:
					rte_tcp_hdr = (struct rte_tcp_hdr *) &rte_ipv4_hdr[1];
					pktinfo.key.v4.src_port = rte_be_to_cpu_16(rte_tcp_hdr->src_port);
					pktinfo.key.v4.dst_port = rte_be_to_cpu_16(rte_tcp_hdr->dst_port);
					break;
				case IPPROTO_UDP:
					rte_udp_hdr = (struct rte_udp_hdr *) &rte_ipv4_hdr[1];
					pktinfo.key.v4.src_port = rte_be_to_cpu_16(rte_udp_hdr->src_port);
					pktinfo.key.v4.dst_port = rte_be_to_cpu_16(rte_udp_hdr->dst_port);
					break;
				default:
					pktinfo.key.v4.src_port = 0;
					pktinfo.key.v4.dst_port = 0;
					break;
			}

			ret = update_flow_entry(app.flow_table_v4, &pktinfo);
			if (ret == 0)
				app_stat.updated_tbl_v4_count++;
			else
				app_stat.miss_updated_tbl_v4_count++;

		} else if (eth_hdr->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6)) {
			/* IPv6 */
			pktinfo.type = PKT_IP_TYPE_IPV6;
			rte_ipv6_hdr = (struct rte_ipv6_hdr *) &eth_hdr[1];

			rte_memcpy(pktinfo.key.v6.src_ip, rte_ipv6_hdr->src_addr, 16);
			rte_memcpy(pktinfo.key.v6.dst_ip, rte_ipv6_hdr->dst_addr, 16);
			pktinfo.key.v6.proto = rte_ipv6_hdr->proto;

			switch (rte_ipv6_hdr->proto) {
				case IPPROTO_TCP:
					rte_tcp_hdr = (struct rte_tcp_hdr *) &rte_ipv6_hdr[1];
					pktinfo.key.v6.src_port = rte_be_to_cpu_16(rte_tcp_hdr->src_port);
					pktinfo.key.v6.dst_port = rte_be_to_cpu_16(rte_tcp_hdr->dst_port);
					break;
				case IPPROTO_UDP:
					rte_udp_hdr = (struct rte_udp_hdr *) &rte_ipv6_hdr[1];
					pktinfo.key.v6.src_port = rte_be_to_cpu_16(rte_udp_hdr->src_port);
					pktinfo.key.v6.dst_port = rte_be_to_cpu_16(rte_udp_hdr->dst_port);
					break;
				default:
					pktinfo.key.v6.src_port = 0;
					pktinfo.key.v6.dst_port = 0;
					break;
			}

			rte_pktmbuf_free(buf);

			/* update flow_table_v6 */
			ret = update_flow_entry(app.flow_table_v6, &pktinfo);
			if (ret == 0)
				app_stat.updated_tbl_v6_count++;
			else
				app_stat.miss_updated_tbl_v6_count++;

		} else {
			/* others */
			app_stat.unknown_pkt_count++;
			rte_pktmbuf_free(buf);
			continue;
		}
	}


}
