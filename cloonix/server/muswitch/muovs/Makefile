
RTE_SDK=$(shell if [ -d ../../../../tmpbuild/cloonix/server/dpdk/dpdk-19.02 ]; then \
                  echo "../../../../tmpbuild/cloonix/server/dpdk/dpdk-19.02"; \
                else \
                  echo "../../dpdk/dpdk-19.02"; \
                fi)
               
RTE_TARGET=x86_64-native-linuxapp-gcc
RUN_LIB=/usr/local/bin/cloonix/server/dpdk/lib
CLOONIX_COMMON=../../../../cloonix/common

GLOB_INCLUDE=${CLOONIX_COMMON}/glob_include
BLKD=${CLOONIX_COMMON}/lib_blkd
RPCT=${CLOONIX_COMMON}/lib_rpct
IOC=../lib_ioc

OBJ_DIR=obj/
EXE = cloonix_muovs
VPATH=src

SRC = main.c ovs_execv.c \
      my_popen.c ring_client.c \
      pcap_fifo.c

WARNINGS=-Wall -Wstrict-prototypes -Wdeclaration-after-statement

INCLUDES= -I./src \
          -I./include \
          -I${GLOB_INCLUDE} \
          -I${IOC}/include \
          -I${RPCT}/include 

LIBS = -L${BLKD} -lblkd \
       -L${IOC} -lioc \
       -L${RPCT} -lrpct \
       -L$(RTE_SDK)/$(RTE_TARGET)/lib \
       -lrte_ring -lrte_eal -lrte_mempool \
       -lrte_mempool_ring -lrte_kvargs -lrt -lpthread

ifneq ($(MAKECMDGOALS),clean)
    include $(RTE_SDK)/mk/rte.vars.mk
endif


CFLAGS+= -g $(WARNINGS) $(INCLUDES)
CFLAGS += -O3 -I$(RTE_SDK)/$(RTE_TARGET)/include
#  CFLAGS += $(WERROR_FLAGS)

LDFLAGS=  -Wl,-rpath -Wl,$(RUN_LIB)

CC=gcc
all: $(shell mkdir -p ./obj) $(EXE)
OBJ = $(SRC:.c=.o)
OBJS = $(addprefix $(OBJ_DIR), $(OBJ))

$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(EXE): $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $(EXE) 

clean:
	@-rm -vf $(OBJ_DIR)*.o $(EXE) 


