IO_CLOWNIX=../lib_io_clownix
GLOB_INCLUDE=../glob_include
RPC_CLOWNIX=../lib_rpc_clownix/
OBJ_DIR=obj/
EXE = cloonix-suid-power

SRC = suid_power.c stub.c crun.c \
      crun_utils.c tar_img.c net_phy.c

VPATH=./src

WARNINGS=-Wall -Wstrict-prototypes -Wdeclaration-after-statement

INCLUDES= -I./src \
          -I$(GLOB_INCLUDE) \
          -I${IO_CLOWNIX}/include \
          -I${RPC_CLOWNIX}/include

LIBS = -L${IO_CLOWNIX} -lio_clownix \
       -lcrypto

CFLAGS+=-mtune=generic -fno-stack-protector -Wno-format-truncation
CFLAGS+=-Wno-unused-variable -g -rdynamic -DIS_DOORWAYS $(WARNINGS) $(INCLUDES)

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
	@-rm -vf $(OBJ_DIR)*.o *~ $(VPATH)/*~ $(EXE) core*
