CLOONIX_COMMON=../../../cloonix/common

IO_CLOWNIX=${CLOONIX_COMMON}/lib_io_clownix
GLOB_INCLUDE=${CLOONIX_COMMON}/glob_include
RPC_QMONITOR=${CLOONIX_COMMON}/lib_rpc_qmonitor/
UTILS=${CLOONIX_COMMON}/lib_utils/
RPCT=${CLOONIX_COMMON}/lib_rpct


OBJ_DIR=./obj/
VPATH=./src

EXE = qmonitor
SRC = qmonitor.c

INCLUDES= -I${IO_CLOWNIX}/include \
          -I${GLOB_INCLUDE} \
          -I${UTILS}/include \
          -I${RPC_QMONITOR}/include

LIBS = -L${IO_CLOWNIX} -lio_clownix \
       -L${RPCT} -lrpct \
       -L${RPC_QMONITOR} -lrpc_qmonitor \
       -L${UTILS} -lutils -lssl -lcrypto -lrt -lpthread -lreadline -lncurses

CFLAGS+= -g -Wall $(INCLUDES)
CC?=gcc
all: $(shell mkdir -p ./obj) $(EXE)

OBJ = $(SRC:.c=.o)
OBJS = $(addprefix $(OBJ_DIR), $(OBJ))

$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(EXE): $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $(EXE) 

clean:
	@-rm -vf $(OBJ_DIR)*.o *~ $(VPATH)/*~ $(EXE) core*
