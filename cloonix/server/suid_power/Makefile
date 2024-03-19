CLOONIX_COMMON=../../common
IO_CLOWNIX=${CLOONIX_COMMON}/lib_io_clownix
GLOB_INCLUDE=${CLOONIX_COMMON}/glob_include
RPC_CLOWNIX=${CLOONIX_COMMON}/lib_rpc_clownix/

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
CFLAGS+=-Wno-unused-variable -g -DIS_DOORWAYS $(WARNINGS) $(INCLUDES)
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
