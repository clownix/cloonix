CLOONIX_COMMON=../../../cloonix/common

IO_CLOWNIX=${CLOONIX_COMMON}/lib_io_clownix
GLOB_INCLUDE=${CLOONIX_COMMON}/glob_include
RPC_CLOWNIX=${CLOONIX_COMMON}/lib_rpc_clownix/
UTILS=${CLOONIX_COMMON}/lib_utils/


WARNINGS=-Wall -Wstrict-prototypes
VPATH=src

LIB=libzor.a

INCLUDES= -I. $(shell pkg-config --cflags glib-2.0) \
          -I${IO_CLOWNIX}/include \
          -I${GLOB_INCLUDE} \
          -I${RPC_CLOWNIX}/include \
          -I${UTILS}/include \
          -I$(VPATH) -I./include

SRCS     = connect_cloonix.c interface.c recv_stub.c

OBJ      = $(SRCS:.c=.o)
OBJ_DIR  = ./obj/
OBJS     = $(addprefix $(OBJ_DIR), $(OBJ))


CFLAGS   += -g $(WARNINGS) $(INCLUDES)

AR = ar
RANLIB = ranlib
CC=gcc

all: $(shell mkdir -p ./obj) $(LIB)

$(EXE): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBS) $(LDFLAGS)
	@echo

$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(LIB): $(OBJS)
	$(AR) rc $(LIB) $(OBJS)
	$(RANLIB) $(LIB)

clean:	
	@-rm -vf $(LIB) $(OBJ_DIR)*.o


















