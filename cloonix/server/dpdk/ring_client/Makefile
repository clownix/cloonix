all: ring
PKG_CONFIG_PATH = $(RTE_SDK)/lib/pkgconfig

CFLAGS += -O3 -Wall -Wdeprecated -mavx -I$(RTE_SDK)/include

LDFLAGS=  -Wl,-rpath,/usr/local/bin/cloonix/server/dpdk/lib \
          $(shell export PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) ; \
                  pkg-config --libs --static libdpdk)

ring: ring_client.c
	$(CC) $(CFLAGS) ring_client.c -o $@ $(LDFLAGS)
