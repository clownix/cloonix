#include "flowtable.h"

struct flow_table *create_flow_table(const char *name, uint32_t _size, enum pkt_ip_type type)
{
	struct flow_table *table;
	uint32_t size = roundup32(_size);
	size = size > RTE_HASH_ENTRIES_MAX ? RTE_HASH_ENTRIES_MAX : size;
	uint32_t keylen;
	switch (type) {
		case PKT_IP_TYPE_IPV4:
			keylen = (uint32_t) sizeof(struct fivetuple_v4);
			break;
		case PKT_IP_TYPE_IPV6:
			keylen = (uint32_t) sizeof(struct fivetuple_v6);
			break;
		default:
			RTE_LOG(ERR, FLOWTABLE, "Invalid pkt_ip_type\n");
			return NULL;
	}

	struct rte_hash_parameters params = {
		.name = name,
		.entries = size,
		.key_len = keylen,
		.hash_func = rte_jhash,
		.hash_func_init_val = 0,
		.socket_id = (int) rte_socket_id(),
	};

	table = (struct flow_table *) rte_zmalloc("flow_table",
			sizeof(struct flow_table) + sizeof(struct flow_entry) * size, 0);
	if (table == NULL) {
		RTE_LOG(ERR, FLOWTABLE, "Cannot allocate memory for flow_table\n");
		return NULL;
	}

	table->handler = rte_hash_create(&params);
	if (table->handler == NULL) {
		RTE_LOG(ERR, FLOWTABLE, "Cannot create rte_hash: %s\n", rte_strerror(rte_errno));
		rte_free(table);
		return NULL;
	}

	return table;
}

void destroy_flow_table(struct flow_table *table)
{
	rte_hash_free(table->handler);
	rte_free(table);
}

int update_flow_entry(struct flow_table *table, struct pkt_info *info)
{
	int32_t key = rte_hash_add_key(table->handler, &info->key);
	if (key >= 0) {
		struct flow_entry *entry = &table->entries[key];
		if (entry->start_tsc == 0) {
			/* insert */
			table->flow_cnt++;
			entry->start_tsc = entry->last_tsc = info->timestamp;
			entry->pkt_cnt = 1;
			entry->byte_cnt = info->pktlen;
			rte_memcpy(&entry->key, &info->key, sizeof(union fivetuple));
		} else {
			/* update */
			entry->last_tsc = info->timestamp;
			entry->pkt_cnt++;
			entry->byte_cnt += info->pktlen;
		}
		return 0;
	}

	switch (-key) {
		case EINVAL:
			RTE_LOG(WARNING, FLOWTABLE, "[update] Invalid parameters\n");
			break;
		case ENOSPC:
			RTE_LOG(WARNING, FLOWTABLE, "[update] No space in the hash for this key\n");
			break;
	}
	return key;
}

int remove_flow_entry(struct flow_table *table, union fivetuple *_key)
{
	int32_t key = rte_hash_del_key(table->handler, _key);
	if (key >= 0) {
		struct flow_entry *entry = &table->entries[key];
		memset(entry, 0, sizeof(struct flow_entry));
		table->flow_cnt--;
		return 0;
	}

	switch (-key) {
		case EINVAL:
			RTE_LOG(WARNING, FLOWTABLE, "[remove] Invalid parameters\n");
			break;
		case ENOENT:
			RTE_LOG(WARNING, FLOWTABLE, "[remove] The key is not found\n");
			break;
	}
	return key;
}
