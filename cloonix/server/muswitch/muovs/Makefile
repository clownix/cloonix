RTE_SDK=$(shell if [ -d ../../../../tmpbuild/cloonix/server/dpdk ]; then \
                  echo "../../../../tmpbuild/cloonix/server/dpdk"; \
                else \
                  echo "../../dpdk"; \
                fi)
               
PKG_CONFIG_PATH=$(RTE_SDK)/lib/pkgconfig

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
      pcap_fifo.c cirspy.c eventfull.c

WARNINGS=-Wall -Wdeprecated -Wstrict-prototypes -Wdeclaration-after-statement

INCLUDES= -I./src \
          -I./include \
          -I${GLOB_INCLUDE} \
          -I${IOC}/include \
          -I${RPCT}/include  \
          -I$(RTE_SDK)/include

LIBS = -L${BLKD} -lblkd \
       -L${IOC} -lioc \
       -L${RPCT} -lrpct

CFLAGS += -O3 -mavx $(INCLUDES) $(WARNINGS)

LDFLAGS=  -Wl,-rpath,/usr/local/bin/cloonix/server/dpdk/lib \
          $(shell export PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) ; \
                  pkg-config --libs --static libdpdk)

CC=gcc
all: $(shell mkdir -p ./obj) $(EXE)
OBJ = $(SRC:.c=.o)
OBJS = $(addprefix $(OBJ_DIR), $(OBJ))

$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(EXE): $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LDFLAGS) $(LIBS) -o $(EXE) 

clean:
	@-rm -vf $(OBJ_DIR)*.o $(EXE) 

