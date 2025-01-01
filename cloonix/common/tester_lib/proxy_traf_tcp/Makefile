IO_CLOWNIX=../../lib_io_clownix
RPC_CLOWNIX=../../lib_rpc_clownix
GLOB_INCLUDE=../../glob_include

OBJ_DIR=obj/
EXE = proxy_traf_tcp
WARNINGS =-Wall -Wno-format-truncation -Wno-format-overflow
VPATH = src
SRC = stub.c \
      main.c

INCLUDES = -I./src \
           -I${IO_CLOWNIX}/include \
           -I${RPC_CLOWNIX}/include \
           -I${GLOB_INCLUDE}


LIBS = -L${IO_CLOWNIX} -lio_clownix \
       -L${RPC_CLOWNIX} -lrpc_clownix \
       -lcrypto 

CFLAGS = $(WARNINGS) $(INCLUDES)

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
