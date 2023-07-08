CLOONIX_COMMON=../../../cloonix/common

IO_CLOWNIX=${CLOONIX_COMMON}/lib_io_clownix
GLOB_INCLUDE=${CLOONIX_COMMON}/glob_include
RPC_CLOWNIX=${CLOONIX_COMMON}/lib_rpc_clownix/
RPC_DOORS=${CLOONIX_COMMON}/lib_rpc_doors/
UTILS=${CLOONIX_COMMON}/lib_utils/


OBJ_DIR=obj/
EXE = cloonix-doorways
PID_CLONE=../uml_cloonix_switch/src
VPATH=./src:${PID_CLONE}

SRC = doorways.c llid_backdoor.c llid_traffic.c sock.c \
      llid_x11.c stats.c dispach.c llid_xwy.c \
      stub_doorways.c pid_clone.c openssh_traf.c

WARNINGS=-Wall -Wstrict-prototypes -Wdeclaration-after-statement

INCLUDES= -I./src \
          -I$(GLOB_INCLUDE) \
          -I${RPC_DOORS}/include \
          -I${IO_CLOWNIX}/include \
          -I${RPC_CLOWNIX}/include \
          -I${PID_CLONE} \
          -I${UTILS}/include

LIBS = -L${RPC_DOORS} -lrpc_doors \
       -L${UTILS} -lutils \
       -L${IO_CLOWNIX} -lio_clownix \
       -lssl -lcrypto -lrt -lm

CFLAGS+=-mtune=generic -fno-stack-protector
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
