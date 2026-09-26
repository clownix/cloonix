IO_CLOWNIX=../lib_io_clownix
GLOB_INCLUDE=../glob_include
RPC_CLOWNIX=../lib_rpc_clownix/
OBJ_DIR=obj/
EXE = cloonix-doorways
VPATH=./src

SRC = doorways.c llid_backdoor.c llid_traffic.c sock.c \
      dispach.c kvmwifi.c

# Wmissing-prototypes: a global with no header declaration is either meant to
# be static, or is dead.
WARNINGS=-Wall -Wstrict-prototypes -Wdeclaration-after-statement \
         -Wmissing-prototypes

INCLUDES= -I./src \
          -I$(GLOB_INCLUDE) \
          -I${IO_CLOWNIX}/include \
          -I${RPC_CLOWNIX}/include

LIBS =   \
       -L${RPC_CLOWNIX} -lrpc_clownix \
       -L${IO_CLOWNIX} -lio_clownix \
       -lssl -lcrypto -lrt -lm

CFLAGS+=-mtune=generic -fno-stack-protector
CFLAGS+= -g -rdynamic $(WARNINGS) $(INCLUDES)

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
	@-rm -vf $(OBJ_DIR)*.o *~ $(VPATH)/*~ $(EXE) core*
