CLOONIX_COMMON=../../../cloonix/common

IO_CLOWNIX=${CLOONIX_COMMON}/lib_io_clownix
GLOB_INCLUDE=${CLOONIX_COMMON}/glob_include
RPC_CLOWNIX=${CLOONIX_COMMON}/lib_rpc_clownix/
RPC_DOORS=${CLOONIX_COMMON}/lib_rpc_doors/
UTILS=${CLOONIX_COMMON}/lib_utils/
BLKD=${CLOONIX_COMMON}/lib_blkd
RPCT=${CLOONIX_COMMON}/lib_rpct
AGENT_INCLUDE=${CLOONIX_COMMON}/agent_dropbear/cloonix_agent/src


OBJ_DIR=obj/
EXE = cloonix_doorways
PID_CLONE=../uml_cloonix_switch/src
VPATH=./src:${PID_CLONE}

SRC = doorways.c llid_backdoor.c llid_traffic.c sock.c \
      llid_x11.c stats.c dispach.c local_dropbear.c \
      stub_doorways.c c2c.c pid_clone.c openssh_traf.c

WARNINGS=-Wall -Wstrict-prototypes -Wdeclaration-after-statement

INCLUDES= -I./src \
          -I$(GLOB_INCLUDE) \
          -I${AGENT_INCLUDE} \
          -I${RPC_DOORS}/include \
          -I${IO_CLOWNIX}/include \
          -I${RPC_CLOWNIX}/include \
          -I${PID_CLONE} \
          -I${UTILS}/include

LIBS = -L${RPC_DOORS} -lrpc_doors \
       -L${UTILS} -lutils \
       -L${IO_CLOWNIX} -lio_clownix \
       -L${BLKD} -lblkd \
       -L${RPCT} -lrpct \
       -lssl -lcrypto -lrt -lm

CFLAGS+= -g -DIS_DOORWAYS $(WARNINGS) $(INCLUDES)
CC=gcc

all: $(shell mkdir -p ./obj) $(EXE)

OBJ = $(SRC:.c=.o)
OBJS = $(addprefix $(OBJ_DIR), $(OBJ))

$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(EXE): $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $(EXE) 

clean:
	@-rm -vf $(OBJ_DIR)*.o *~ $(VPATH)/*~ $(EXE) core*
