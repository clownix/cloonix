#ifndef __EXPORT_FORMAT_H__
#define __EXPORT_FORMAT_H__

#include <stdint.h>
#include "fivetuple.h"

#define EXPORT_FLOW_MAX_V4 50
#define EXPORT_FLOW_MAX_V6 25

/* flowexp_data */
struct flowexp_data_v4 {
	uint32_t pps;		/* offset = 0 */
	uint32_t bps;		/* 4 */
	struct fivetuple_v4 key;
	/* uint32_t src_ip; */		/* 8 */
	/* uint32_t dst_ip; */		/* 12 */
	/* uint16_t src_port; */	/* 16 */
	/* uint16_t dst_port; */	/* 18 */
	/* uint8_t proto; */		/* 20 */
	uint8_t __unused[3];
} __attribute__((packed)); /* 24 bytes */

struct flowexp_data_v6 {
	uint32_t pps;		/* offset = 0 */
	uint32_t bps;		/* 4 */
	struct fivetuple_v6 key;
	/* uint8_t src_ip[16]; */	/* 8 */
	/* uint8_t dst_ip[16]; */	/* 24 */
	/* uint16_t src_port; */	/* 40 */
	/* uint16_t dst_port; */	/* 42 */
	/* uint8_t proto; */		/* 44 */
	uint8_t __unused[3];
} __attribute__((packed)); /* 48 bytes */

/* export_entry */
struct export_entry_v4 {
	struct flowexp_data_v4 fdata;
	uint64_t start_tsc;
	uint64_t prev_pkt_cnt;
	uint64_t prev_byte_cnt;
} __rte_cache_aligned;

struct export_entry_v6 {
	struct flowexp_data_v6 fdata;
	uint64_t start_tsc;
	uint64_t prev_pkt_cnt;
	uint64_t prev_byte_cnt;
} __rte_cache_aligned;

/* export_table */
struct export_table_v4 {
	uint32_t num;
	struct export_entry_v4 entries[0];
};

struct export_table_v6 {
	uint32_t num;
	struct export_entry_v6 entries[0];
};

/* export_format */
struct export_format_v4 {
	uint32_t serial;
	uint32_t flow_cnt;
	uint32_t data_type;
	float kpps;
	float mbps;
	struct flowexp_data_v4 fdata[0];
};

struct export_format_v6 {
	uint32_t serial;
	uint32_t flow_cnt;
	uint32_t data_type;
	float kpps;
	float mbps;
	struct flowexp_data_v6 fdata[0];
};

#endif /* __EXPORT_FORMAT_H__ */
