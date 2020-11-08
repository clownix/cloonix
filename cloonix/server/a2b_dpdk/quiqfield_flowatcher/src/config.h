#ifndef __CONFIG_H__
#define __CONFIG_H__

/* ports */
#define RX_RING_SIZE 2048
#define TX_RING_SIZE 2048
#define RX_BURST_SIZE 64
#define TX_BURST_SIZE 32

/* mempool */
#define POOL_MBUF_SIZE 2048
#define POOL_SIZE (1 << 20)
#define POOL_CACHE_SIZE 512

/* flow table */
#define FLOW_TABLE_SIZE (1 << 20) /* 1M entries */

/* for export */
#define FLOWEXP_PORT 7681
#define RESOURCE_PATH "client"

#endif /* __CONFIG_H__ */
