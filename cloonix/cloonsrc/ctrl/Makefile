IO_CLOWNIX=../lib_io_clownix
GLOB_INCLUDE=../glob_include
RPC_CLOWNIX=../lib_rpc_clownix/
UTILS=../lib_utils/
RPC_LAYOUT=../lib_rpc_layout/
LIB_CLIENT=../lib_client
OBJ_DIR=./obj/
WARNINGS=-Wall 
VPATH=src

EXE = cloonix-ctrl
SRC = cloonix_ctrl.c cnt_cli.c cmd_ctrl.c help.c vm_cli.c \
      struct_2_ascii.c  cmd_layout.c



INCLUDES= -I./src \
          -I${IO_CLOWNIX}/include \
          -I${GLOB_INCLUDE} \
          -I${RPC_CLOWNIX}/include \
          -I${UTILS}/include \
          -I${RPC_LAYOUT}/include \
          -I${LIB_CLIENT}/include

LIBS = -L${LIB_CLIENT}/ -lclient \
       -L${IO_CLOWNIX} -lio_clownix \
       -L${RPC_LAYOUT}/ -lrpc_layout \
       -L${RPC_CLOWNIX}/ -lrpc_clownix \
       -L${UTILS}/ -lutils \
       -L${LIB_CLIENT}/ -lclient \
       -lssl -lcrypto -lrt -lm 


CFLAGS+=-mtune=generic -fno-stack-protector
CFLAGS+= -g $(WARNINGS) $(INCLUDES)
CC=gcc

all: $(shell mkdir -p ./obj) $(EXE)

OBJ = $(SRC:.c=.o)
OBJS = $(addprefix $(OBJ_DIR), $(OBJ))

$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(EXE): $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $(EXE) 

clean:
	@-rm -vf $(OBJ_DIR)*.o *~ src/*~ $(EXE)

