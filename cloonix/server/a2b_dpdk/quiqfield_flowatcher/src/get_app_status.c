#include "flowtable.h"


void get_app_status_init(void)
{
	app_stat.rx_count = 0;
	app_stat.updated_tbl_v4_count = 0;
	app_stat.miss_updated_tbl_v4_count = 0;
	app_stat.updated_tbl_v6_count = 0;
	app_stat.miss_updated_tbl_v6_count = 0;
	app_stat.unknown_pkt_count = 0;
	exp_stat.flow_cnt = 0;
	exp_stat.kpps = 0;
	exp_stat.mbps = 0;
}

void get_app_status(void)
{
	struct app_rx_flow_stats _app_stat;
	struct app_rx_flow_stats total;
	uint32_t cnt_v4, cnt_v6;

	/* get status */
	cnt_v4 = cnt_v6 = 0;
	cnt_v4 += app.flow_table_v4->flow_cnt;
	cnt_v6 += app.flow_table_v6->flow_cnt;

	rte_memcpy(&_app_stat, &app_stat, sizeof(struct app_rx_flow_stats));

	memset(&total, 0, sizeof(struct app_rx_flow_stats));
	total.rx_count += _app_stat.rx_count;
	total.updated_tbl_v4_count += _app_stat.updated_tbl_v4_count;
	total.miss_updated_tbl_v4_count += _app_stat.miss_updated_tbl_v4_count;
	total.updated_tbl_v6_count += _app_stat.updated_tbl_v6_count;
	total.miss_updated_tbl_v6_count += _app_stat.miss_updated_tbl_v6_count;
	total.unknown_pkt_count += _app_stat.unknown_pkt_count;


	printf("=====  APP status  =====\n");
	printf(" RX: %"PRIu64" pkts\n", total.rx_count);
	printf(" tbl_v4: %"PRIu64" pkts\n", total.updated_tbl_v4_count);
	printf(" miss_tbl_v4: %"PRIu64" pkts\n", total.miss_updated_tbl_v4_count);
	printf(" tbl_v6: %"PRIu64" pkts\n", total.updated_tbl_v6_count);
	printf(" miss_tbl_v6: %"PRIu64" pkts\n", total.miss_updated_tbl_v6_count);
	printf(" unknown_pkt: %"PRIu64" pkts\n", total.unknown_pkt_count);

	printf("===== Flow count =====\n");
	printf(" cnt_v4: %"PRIu32", cnt_v6: %"PRIu32"\n", cnt_v4, cnt_v6);
}

void get_app_status_1sec(void)
{
	struct app_rx_flow_stats _app_stat;
	struct app_rx_flow_stats total;
	uint32_t cnt_v4, cnt_v6;

	cnt_v4 = cnt_v6 = 0;
	cnt_v4 += app.flow_table_v4->flow_cnt;
	cnt_v6 += app.flow_table_v6->flow_cnt;

	rte_memcpy(&_app_stat, &app_stat, sizeof(struct app_rx_flow_stats));

	memset(&total, 0, sizeof(struct app_rx_flow_stats));
	total.rx_count += _app_stat.rx_count;
	total.updated_tbl_v4_count += _app_stat.updated_tbl_v4_count;
	total.miss_updated_tbl_v4_count += _app_stat.miss_updated_tbl_v4_count;
	total.updated_tbl_v6_count += _app_stat.updated_tbl_v6_count;
	total.miss_updated_tbl_v6_count += _app_stat.miss_updated_tbl_v6_count;
	total.unknown_pkt_count += _app_stat.unknown_pkt_count;

	printf("[APP] RX: %"PRIu64" pkts, tbl_v4: %"PRIu64" pkts, miss_tbl_v4: %"PRIu64" pkts"
			" tbl_v6: %"PRIu64" pkts, miss_tbl_v6: %"PRIu64" pkts, unknown: %"PRIu64" pkts\n",
			total.rx_count, total.updated_tbl_v4_count, total.miss_updated_tbl_v4_count,
			total.updated_tbl_v6_count, total.miss_updated_tbl_v6_count, total.unknown_pkt_count);

	printf("[Flowtable] cnt_v4: %"PRIu32", cnt_v6: %"PRIu32"\n", cnt_v4, cnt_v6);
	exp_stat.flow_cnt = cnt_v4 + cnt_v6;

}
