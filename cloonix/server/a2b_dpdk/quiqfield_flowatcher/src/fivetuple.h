#ifndef __FIVETUPLE_H__
#define __FIVETUPLE_H__

#include <stdint.h>

struct fivetuple_v4 {
	uint32_t src_ip;
	uint32_t dst_ip;
	uint16_t src_port;
	uint16_t dst_port;
	uint8_t proto;
} __attribute__((packed));

struct fivetuple_v6 {
	uint8_t src_ip[16];
	uint8_t dst_ip[16];
	uint16_t src_port;
	uint16_t dst_port;
	uint8_t proto;
} __attribute__((packed));

union fivetuple {
	struct fivetuple_v4 v4;
	struct fivetuple_v6 v6;
};

#endif /* __FIVETUPLE_H__ */
