IO_CLOWNIX=../lib_io_clownix
RPC_CLOWNIX=../lib_rpc_clownix
GLOB_INCLUDE=../glob_include

OBJ_DIR=obj/
EXE = cloonix-proxymous
WARNINGS = -g -rdynamic -Wall -Wno-format-truncation -Wno-format-overflow
VPATH = src
SRC = c2c_peer.c \
      udp_proxy.c \
      sig_process.c \
      set_config.c \
      jsweb_relay.c \
      nat_main.c \
      passt_spawn.c \
      demo_exec.c \
      proxy_tls.c \
      dir_watch.c \
      main.c

INCLUDES = -I./src \
           -I${IO_CLOWNIX}/include \
           -I${RPC_CLOWNIX}/include \
           -I${GLOB_INCLUDE}

# rpc uses io, so rpc comes first for a static link
LIBS = -L${RPC_CLOWNIX} -lrpc_clownix \
       -L${IO_CLOWNIX} -lio_clownix \
       -lssl -lcrypto 

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
	strip $(EXE)
clean:
	@-rm -vf $(OBJ_DIR)*.o $(EXE)
