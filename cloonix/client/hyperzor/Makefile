CLOONIX_COMMON=../../../cloonix/common

IO_CLOWNIX=${CLOONIX_COMMON}/lib_io_clownix
GLOB_INCLUDE=${CLOONIX_COMMON}/glob_include
RPC_CLOWNIX=${CLOONIX_COMMON}/lib_rpc_clownix/
UTILS=${CLOONIX_COMMON}/lib_utils/
RPC_C2C=${CLOONIX_COMMON}/lib_rpc_c2c/
RPC_LAYOUT=${CLOONIX_COMMON}/lib_rpc_layout/
BLKD=${CLOONIX_COMMON}/lib_blkd
RPCT=${CLOONIX_COMMON}/lib_rpct
AGENT_INCLUDE=${CLOONIX_COMMON}/agent_dropbear/cloonix_agent/src

LIB_CLIENT=../lib_client
CONNECT=../lib_zor

EXE     = hyperzor
WARNINGS=-Wall
VPATH=src

INCLUDES= -I. $(shell pkg-config --cflags gtk+-3.0) \
          -I${IO_CLOWNIX}/include \
          -I$(GLOB_INCLUDE) \
          -I${RPC_CLOWNIX}/include \
          -I${UTILS}/include \
          -I${CONNECT}/include \
          -I${AGENT_INCLUDE} \
          -I$(VPATH)
LIBS = $(shell pkg-config --libs gtk+-3.0) \
       -L${IO_CLOWNIX} -lio_clownix \
       -L${BLKD} -lblkd \
       -L${RPCT} -lrpct \
       -L${CONNECT}/ -lzor \
       -L${RPC_CLOWNIX} -lrpc_clownix \
       -L${UTILS}/ -lutils \
       -lcrypto -lrt

SRCS     =  main.c store.c gui.c stats.c 

OBJ      = $(SRCS:.c=.o)
OBJ_DIR  = ./obj/
OBJS     = $(addprefix $(OBJ_DIR), $(OBJ))


CFLAGS   += -g $(WARNINGS) $(INCLUDES)

CC       = gcc 

all: $(shell mkdir -p ./obj) $(EXE)

$(EXE): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBS) $(LDFLAGS)
	@echo

$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

clean:	
	@-rm -vf $(EXE) $(OBJ_DIR)*.o


















