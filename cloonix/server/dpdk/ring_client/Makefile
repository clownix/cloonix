all: ring


include $(RTE_SDK)/mk/rte.vars.mk

CFLAGS += -g -ggdb -I$(RTE_SDK)/$(RTE_TARGET)/include
CFLAGS += $(WERROR_FLAGS)


LDFLAGS=  -Wl,-rpath -Wl,$(RUN_LIB) \
          -L$(RTE_SDK)/$(RTE_TARGET)/lib \
          -lrte_ring -lrte_eal -lrte_mempool -lrte_kvargs -lrte_mempool_ring

ring: ring_client.c
	$(CC) $(CFLAGS) ring_client.c -o $@ $(LDFLAGS) 
