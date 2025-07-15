IO_CLOWNIX=../lib_io_clownix
RPC_CLOWNIX=../lib_rpc_clownix
GLOB_INCLUDE=../glob_include

OBJ_DIR=obj/
EXE = cloonix-proxymous
WARNINGS = -g -rdynamic -Wall -Wno-format-truncation -Wno-format-overflow
VPATH = src
SRC = stub.c \
      c2c_peer.c \
      udp_proxy.c \
      sig_process.c \
      xprocess.c \
      set_config.c \
      pweb_process.c \
      nat_main.c \
      nat_icmp.c \
      nat_udp.c \
      nat_tcp.c \
      nat_utils.c \
      main.c

INCLUDES = -I./src \
           -I${IO_CLOWNIX}/include \
           -I${RPC_CLOWNIX}/include \
           -I${GLOB_INCLUDE}


LIBS = -L${IO_CLOWNIX} -lio_clownix \
       -L${RPC_CLOWNIX} -lrpc_clownix \
       -lcrypto 

CFLAGS = $(WARNINGS) $(INCLUDES)
LDFLAGS+= -g -rdynamic

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
