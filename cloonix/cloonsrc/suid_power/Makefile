IO_CLOWNIX=../lib_io_clownix
GLOB_INCLUDE=../glob_include
RPC_CLOWNIX=../lib_rpc_clownix/
OBJ_DIR=obj/
EXE = cloonix-suid-power

SRC = suid_power.c crun.c console.c \
      crun_utils.c tar_img.c net_phy.c idmap.c \
      vwifi.c air_proc.c utils.c guard.c

VPATH=./src

WARNINGS=-Wall -Wstrict-prototypes -Wdeclaration-after-statement

INCLUDES= -I./src \
          -I/usr/include/libnl3 \
          -I$(GLOB_INCLUDE) \
          -I${IO_CLOWNIX}/include \
          -I${RPC_CLOWNIX}/include

LIBS = -L${IO_CLOWNIX} -lio_clownix -lnl-3 -lnl-genl-3 \
       -lcrypto

CFLAGS+=-mtune=generic -fno-stack-protector -Wno-format-truncation
CFLAGS+=-g -rdynamic -DIS_DOORWAYS $(WARNINGS) $(INCLUDES)

LDFLAGS+= -g -rdynamic

CC=gcc

all: $(shell mkdir -p ./obj) $(EXE)

OBJ = $(SRC:.c=.o)
OBJS = $(addprefix $(OBJ_DIR), $(OBJ))

$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(EXE): $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $(EXE) 
	# NO strip: it threw away the -g above, and the crash handler prints offsets.

clean:
	@-rm -vf $(OBJ_DIR)*.o *~ $(VPATH)/*~ $(EXE) core*
