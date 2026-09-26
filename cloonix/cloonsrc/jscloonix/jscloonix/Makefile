# cloonix-gui-server; CLOONIX_ROOT = absolute path of cloonsrc/, passed by cmd.

CC  = gcc
EXE = cloonix-gui-server

CLOONIX_ROOT ?= ../..
IO_CLOWNIX   = $(CLOONIX_ROOT)/lib_io_clownix
RPC_CLOWNIX  = $(CLOONIX_ROOT)/lib_rpc_clownix
GLOB_INCLUDE = $(CLOONIX_ROOT)/glob_include

WARNINGS = -Wall -Wno-format-truncation -Wno-format-overflow \
           -Wno-stringop-truncation -Wno-unused-result
CFLAGS   = -O2 -mtune=generic -fno-stack-protector -g -rdynamic -D_GNU_SOURCE $(WARNINGS) \
           -I$(GLOB_INCLUDE)          \
           -I$(IO_CLOWNIX)/include    \
                    \
           -I$(RPC_CLOWNIX)/include

LIBS     = -Wl,--start-group \
             -L$(RPC_CLOWNIX) -lrpc_clownix \
                           \
             -L$(IO_CLOWNIX)  -lio_clownix  \
           -Wl,--end-group \
           -lcrypto -lrt -lutil -lm -lpthread

.PHONY: all clean

all:
	$(MAKE) -C $(IO_CLOWNIX)
	$(MAKE) -C $(RPC_CLOWNIX)
	$(CC) $(CFLAGS) -o $(EXE) cloonix_gui_server.c $(LIBS)

clean:
	rm -f $(EXE)
