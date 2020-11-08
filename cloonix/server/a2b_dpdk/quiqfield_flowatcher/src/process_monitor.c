#include "flowtable.h"
#include "export_format.h"
#include "config.h"

#define FLOW_TIMEOUT 30

static struct export_table_v4 *exp_table_v4;
static struct export_table_v6 *exp_table_v6;
static uint32_t *exp_idx_v4, *exp_idx_v6;

static uint8_t export_lock = 0;

static void update_export_data_v4(void)
{
	const uint64_t threshold_tsc = rte_get_tsc_hz() * FLOW_TIMEOUT;
	const uint64_t curr_tsc = rte_rdtsc();
	uint32_t  j, pps, num = 0;
	uint64_t pkt_cnt, byte_cnt;
	struct flow_table *table;
	struct flow_entry *entry;
	struct export_entry_v4 *exp_entry;

	table = app.flow_table_v4;

	for (j = 0; j < FLOW_TABLE_SIZE; j++) {
		entry = &table->entries[j];
		exp_entry = &exp_table_v4->entries[j];

		if (entry->start_tsc != 0) {
			pkt_cnt = entry->pkt_cnt;
			byte_cnt = entry->byte_cnt;

			if (exp_entry->start_tsc != entry->start_tsc) {
				exp_entry->start_tsc = entry->start_tsc;
				exp_entry->prev_pkt_cnt = 0;
				exp_entry->prev_byte_cnt = 0;
				exp_entry->fdata.key.src_ip = entry->key.v4.src_ip;
				exp_entry->fdata.key.dst_ip = entry->key.v4.dst_ip;
				exp_entry->fdata.key.src_port = entry->key.v4.src_port;
				exp_entry->fdata.key.dst_port = entry->key.v4.dst_port;
				exp_entry->fdata.key.proto = entry->key.v4.proto;
			}

			pps = (uint32_t) (pkt_cnt - exp_entry->prev_pkt_cnt);
			if (pps == 0) {
				/* expire check */
				if (curr_tsc - entry->last_tsc > threshold_tsc) {
					remove_flow_entry(table, &entry->key);
					memset(exp_entry, 0, sizeof(struct export_entry_v4));
				}
				continue;
			}

			exp_entry->fdata.pps = pps;
			exp_entry->fdata.bps = (uint32_t) (byte_cnt - exp_entry->prev_byte_cnt);

			exp_idx_v4[num++] = j;
			exp_entry->prev_pkt_cnt = pkt_cnt;
			exp_entry->prev_byte_cnt = byte_cnt;
		}
	}

	exp_table_v4->num = num;
}

static void update_export_data_v6(void)
{
	const uint64_t threshold_tsc = rte_get_tsc_hz() * FLOW_TIMEOUT;
	const uint64_t curr_tsc = rte_rdtsc();
	uint32_t j, pps, num = 0;
	uint64_t pkt_cnt, byte_cnt;
	struct flow_table *table;
	struct flow_entry *entry;
	struct export_entry_v6 *exp_entry;

	table = app.flow_table_v6;

	for (j = 0; j < FLOW_TABLE_SIZE; j++) {
		entry = &table->entries[j];
		exp_entry = &exp_table_v6->entries[j];

		if (entry->start_tsc != 0) {
			pkt_cnt = entry->pkt_cnt;
			byte_cnt = entry->byte_cnt;

			if (exp_entry->start_tsc != entry->start_tsc) {
				exp_entry->start_tsc = entry->start_tsc;
				exp_entry->prev_pkt_cnt = 0;
				exp_entry->prev_byte_cnt = 0;
				rte_memcpy(exp_entry->fdata.key.src_ip, entry->key.v6.src_ip, 16);
				rte_memcpy(exp_entry->fdata.key.dst_ip, entry->key.v6.dst_ip, 16);
				exp_entry->fdata.key.src_port = entry->key.v6.src_port;
				exp_entry->fdata.key.dst_port = entry->key.v6.dst_port;
				exp_entry->fdata.key.proto = entry->key.v6.proto;
			}

			pps = (uint32_t) (pkt_cnt - exp_entry->prev_pkt_cnt);
			if (pps == 0) {
				/* expire check */
				if (curr_tsc - entry->last_tsc > threshold_tsc) {
					remove_flow_entry(table, &entry->key);
					memset(exp_entry, 0, sizeof(struct export_entry_v6));
				}
				continue;
			}

			exp_entry->fdata.pps = pps;
			exp_entry->fdata.bps = (uint32_t) (byte_cnt - exp_entry->prev_byte_cnt);

			exp_idx_v6[num++] = j;
			exp_entry->prev_pkt_cnt = pkt_cnt;
			exp_entry->prev_byte_cnt = byte_cnt;
		}
	}

	exp_table_v6->num = num;
}

static int closing_flag = 0;
int callback_flowexp(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + 1280];
	struct per_session_data__flowexp *psd = (struct per_session_data__flowexp *)user;
	struct export_format_v4 *p4 = (struct export_format_v4 *)&buf[LWS_SEND_BUFFER_PRE_PADDING];
	struct export_format_v6 *p6 = (struct export_format_v6 *)&buf[LWS_SEND_BUFFER_PRE_PADDING];
	char seeya[6];
	int n, m;
	int write_mode = LWS_WRITE_CONTINUATION;
	uint32_t i;

	switch (reason) {

		case LWS_CALLBACK_ESTABLISHED:

			RTE_LOG(INFO, WEBSOCKETS, "Server sees client connect\n");
			psd->serial = 0;
			psd->state = EXPORT_START_V4;
			break;

		case LWS_CALLBACK_SERVER_WRITEABLE:

			switch (psd->state) {

				case EXPORT_START_V4:

					export_lock = 1;
					psd->left = exp_table_v4->num;
					write_mode = LWS_WRITE_BINARY;

					p4->serial = ++psd->serial;
					p4->flow_cnt = exp_stat.flow_cnt;
					p4->data_type = 0;
					p4->kpps = exp_stat.kpps;
					p4->mbps = exp_stat.mbps;

					if (psd->left <= EXPORT_FLOW_MAX_V4) { /* NO FRAGMENT */

						n = sizeof(struct export_format_v4) +
							sizeof(struct flowexp_data_v4) * psd->left;
						for (i = 0; i < psd->left; i++) {
							rte_memcpy(&p4->fdata[i], &exp_table_v4->entries[exp_idx_v4[i]].fdata,
									sizeof(struct flowexp_data_v4));
						}

						m = lws_write(wsi, (unsigned char *)p4, n, write_mode);
						if (m < 0) {
							RTE_LOG(WARNING, WEBSOCKETS, "Failed to write (%d)\n", m);
							export_lock = 0;
							return -1;
						}
						if (m < n) {
							RTE_LOG(WARNING, WEBSOCKETS, "Partial write (%d/%d)\n", m, n);
							export_lock = 0;
							return -1;
						}

						psd->state = EXPORT_START_V6;
						lws_callback_on_writable(wsi);

						break;

					} else { /* FIRST FRAGMENT */

						write_mode |= LWS_WRITE_NO_FIN;

						n = sizeof(struct export_format_v4) +
							sizeof(struct flowexp_data_v4) * EXPORT_FLOW_MAX_V4;
						for (i = 0; i < EXPORT_FLOW_MAX_V4; i++) {
							rte_memcpy(&p4->fdata[i], &exp_table_v4->entries[exp_idx_v4[i]].fdata,
									sizeof(struct flowexp_data_v4));
						}

						m = lws_write(wsi, (unsigned char *)p4, n, write_mode);
						if (m < 0) {
							RTE_LOG(WARNING, WEBSOCKETS, "Failed to write (%d)\n", m);
							export_lock = 0;
							return -1;
						}
						if (m < n) {
							RTE_LOG(WARNING, WEBSOCKETS, "Partial write (%d/%d)\n", m, n);
							export_lock = 0;
							return -1;
						}

						psd->left -= EXPORT_FLOW_MAX_V4;
						psd->state = EXPORT_PAYLOAD_V4;
						lws_callback_on_writable(wsi);

						break;
					}

				case EXPORT_PAYLOAD_V4:

					if (psd->left <= EXPORT_FLOW_MAX_V4) {
						psd->state = EXPORT_PAYLOAD_V4_FIN;
						lws_callback_on_writable(wsi);
						break;
					}

					write_mode |= LWS_WRITE_NO_FIN;

					n = sizeof(struct flowexp_data_v4) * EXPORT_FLOW_MAX_V4;
					for (i = 0; i < EXPORT_FLOW_MAX_V4; i++) {
						rte_memcpy(&p4->fdata[i],
								&exp_table_v4->entries[exp_idx_v4[exp_table_v4->num - psd->left + i]].fdata,
								sizeof(struct flowexp_data_v4));
					}

					m = lws_write(wsi, (unsigned char *)p4->fdata, n, write_mode);
					if (m < 0) {
						RTE_LOG(WARNING, WEBSOCKETS, "Failed to write (%d)\n", m);
						export_lock = 0;
						return -1;
					}
					if (m < n) {
						RTE_LOG(WARNING, WEBSOCKETS, "Partial write (%d/%d)\n", m, n);
						export_lock = 0;
						return -1;
					}

					psd->left -= EXPORT_FLOW_MAX_V4;
					lws_callback_on_writable(wsi);

					break;

				case EXPORT_PAYLOAD_V4_FIN:

					n = sizeof(struct flowexp_data_v4) * psd->left;
					for (i = 0; i < psd->left; i++) {
						rte_memcpy(&p4->fdata[i],
								&exp_table_v4->entries[exp_idx_v4[exp_table_v4->num - psd->left + i]].fdata,
								sizeof(struct flowexp_data_v4));
					}

					m = lws_write(wsi, (unsigned char *)p4->fdata, n, write_mode);
					if (m < 0) {
						RTE_LOG(WARNING, WEBSOCKETS, "Failed to write (%d)\n", m);
						export_lock = 0;
						return -1;
					}
					if (m < n) {
						RTE_LOG(WARNING, WEBSOCKETS, "Partial write (%d/%d)\n", m, n);
						export_lock = 0;
						return -1;
					}

					psd->state = EXPORT_START_V6;
					lws_callback_on_writable(wsi);

					break;

				case EXPORT_START_V6:

					psd->left = exp_table_v6->num;
					write_mode = LWS_WRITE_BINARY;

					p6->serial = psd->serial;
					p6->flow_cnt = exp_stat.flow_cnt;
					p6->data_type = 1;
					p6->kpps = exp_stat.kpps;
					p6->mbps = exp_stat.mbps;

					if (psd->left <= EXPORT_FLOW_MAX_V6) { /* NO FRAGMENT */

						n = sizeof(struct export_format_v6) +
							sizeof(struct flowexp_data_v6) * psd->left;
						for (i = 0; i < psd->left; i++) {
							rte_memcpy(&p6->fdata[i], &exp_table_v6->entries[exp_idx_v6[i]].fdata,
									sizeof(struct flowexp_data_v6));
						}

						m = lws_write(wsi, (unsigned char *)p6, n, write_mode);
						if (m < 0) {
							RTE_LOG(WARNING, WEBSOCKETS, "Failed to write (%d)\n", m);
							export_lock = 0;
							return -1;
						}
						if (m < n) {
							RTE_LOG(WARNING, WEBSOCKETS, "Partial write (%d/%d)\n", m, n);
							export_lock = 0;
							return -1;
						}

						psd->state = EXPORT_START_V4;

						export_lock = 0;
						break;

					} else { /* FIRST FRAGMENT */

						write_mode |= LWS_WRITE_NO_FIN;

						n = sizeof(struct export_format_v6) +
							sizeof(struct flowexp_data_v6) * EXPORT_FLOW_MAX_V6;
						for (i = 0; i < EXPORT_FLOW_MAX_V6; i++) {
							rte_memcpy(&p6->fdata[i], &exp_table_v6->entries[exp_idx_v6[i]].fdata,
									sizeof(struct flowexp_data_v6));
						}

						m = lws_write(wsi, (unsigned char *)p6, n, write_mode);
						if (m < 0) {
							RTE_LOG(WARNING, WEBSOCKETS, "Failed to write (%d)\n", m);
							export_lock = 0;
							return -1;
						}
						if (m < n) {
							RTE_LOG(WARNING, WEBSOCKETS, "Partial write (%d/%d)\n", m, n);
							export_lock = 0;
							return -1;
						}

						psd->left -= EXPORT_FLOW_MAX_V6;
						psd->state = EXPORT_PAYLOAD_V6;
						lws_callback_on_writable(wsi);

						break;
					}

				case EXPORT_PAYLOAD_V6:

					if (psd->left <= EXPORT_FLOW_MAX_V6) {
						psd->state = EXPORT_PAYLOAD_V6_FIN;
						lws_callback_on_writable(wsi);
						break;
					}

					write_mode |= LWS_WRITE_NO_FIN;

					n = sizeof(struct flowexp_data_v6) * EXPORT_FLOW_MAX_V6;
					for (i = 0; i < EXPORT_FLOW_MAX_V6; i++) {
						rte_memcpy(&p6->fdata[i],
								&exp_table_v6->entries[exp_idx_v6[exp_table_v6->num - psd->left + i]].fdata,
								sizeof(struct flowexp_data_v6));
					}

					m = lws_write(wsi, (unsigned char *)p6->fdata, n, write_mode);
					if (m < 0) {
						RTE_LOG(WARNING, WEBSOCKETS, "Failed to write (%d)\n", m);
						export_lock = 0;
						return -1;
					}
					if (m < n) {
						RTE_LOG(WARNING, WEBSOCKETS, "Partial write (%d/%d)\n", m, n);
						export_lock = 0;
						return -1;
					}

					psd->left -= EXPORT_FLOW_MAX_V6;
					lws_callback_on_writable(wsi);

					break;

				case EXPORT_PAYLOAD_V6_FIN:

					n = sizeof(struct flowexp_data_v6) * psd->left;
					for (i = 0; i < psd->left; i++) {
						rte_memcpy(&p6->fdata[i],
								&exp_table_v6->entries[exp_idx_v6[exp_table_v6->num - psd->left + i]].fdata,
								sizeof(struct flowexp_data_v6));
					}

					m = lws_write(wsi, (unsigned char *)p6->fdata, n, write_mode);
					if (m < 0) {
						RTE_LOG(WARNING, WEBSOCKETS, "Failed to write (%d)\n", m);
						export_lock = 0;
						return -1;
					}
					if (m < n) {
						RTE_LOG(WARNING, WEBSOCKETS, "Partial write (%d/%d)\n", m, n);
						export_lock = 0;
						return -1;
					}

					psd->state = EXPORT_START_V4;

					export_lock = 0;
					break;
			}

			if (closing_flag && psd->serial == 50) {
				RTE_LOG(INFO, WEBSOCKETS, "Close testing countup\n");
				export_lock = 0;
				return -1;
			}

			break;

		case LWS_CALLBACK_RECEIVE:

			if (len < 6)
				break;

			if (strcmp((const char *)in, "reset\n") == 0)
				psd->serial = 0;

			if (strcmp((const char *)in, "closeme\n") == 0) {
				RTE_LOG(INFO, WEBSOCKETS, "Closing as requested\n");
				strcpy(seeya, "seeya");
				lws_close_reason(wsi, LWS_CLOSE_STATUS_GOINGAWAY, (unsigned char *)seeya, 5);
				export_lock = 0;
				return -1;
			}

			break;

		default:
			break;
	}

	return 0;
}

void app_main_loop_monitor(void)
{
	const unsigned lcore_id = rte_lcore_id();
	const uint64_t threshold_tsc = rte_get_tsc_hz();
	uint64_t curr_tsc, prev_tsc;
	/* uint64_t tsc1, tsc2; */
	int32_t n;

	/* init exp_table */
	exp_table_v4 = (struct export_table_v4 *) rte_zmalloc("exp_table_v4",
			sizeof(struct export_table_v4) + sizeof(struct flowexp_data_v4), 0);
	if (exp_table_v4 == NULL)
		rte_exit(EXIT_FAILURE, "Cannot allocate exp_table_v4\n");

	exp_table_v6 = (struct export_table_v6 *) rte_zmalloc("exp_table_v6",
			sizeof(struct export_table_v6) + sizeof(struct flowexp_data_v6), 0);
	if (exp_table_v6 == NULL)
		rte_exit(EXIT_FAILURE, "Cannot allocate exp_table_v6\n");

	exp_idx_v4 = (uint32_t *) rte_zmalloc("exp_idx_v4", sizeof(uint32_t), 0);
	if (exp_idx_v4 == NULL)
		rte_exit(EXIT_FAILURE, "Cannot allocate exp_idx_v4\n");

	exp_idx_v6 = (uint32_t *) rte_zmalloc("exp_idx_v6", sizeof(uint32_t), 0);
	if (exp_idx_v6 == NULL)
		rte_exit(EXIT_FAILURE, "Cannot allocate exp_idx_v6\n");

	RTE_LOG(INFO, FLOWATCHER, "[core %u] monitor flow_table & export data Ready\n", lcore_id);

	curr_tsc = prev_tsc = rte_rdtsc();
	while (n >= 0 && !app_quit_signal) {
		curr_tsc = rte_rdtsc();
		if (curr_tsc - prev_tsc > threshold_tsc) {
			prev_tsc = curr_tsc;

			/* print app status */
			get_app_status_1sec();

			/* update export_table */
			if (!export_lock) {
				/* tsc1 = rte_rdtsc(); */
				update_export_data_v4();
				update_export_data_v6();
				/* tsc2 = rte_rdtsc(); */
				/* printf("UPDATE_EXPORT_DATA tsc = %"PRIu64"\n", (tsc2 - tsc1)); */
			} else {
				RTE_LOG(INFO, FLOWATCHER, "Locked update_export_data\n");
			}

			/* export */
			lws_callback_on_writable_all_protocol(wss_context, &wss_protocols[PROTOCOL_FLOWEXP]);
		}

		n = lws_service(wss_context, 0);
	}

	RTE_LOG(INFO, FLOWATCHER, "[core %u] monitor flow_table & export data finished\n", lcore_id);
}
