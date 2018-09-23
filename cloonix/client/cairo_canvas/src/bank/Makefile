CLOONIX_COMMON=../../../../../cloonix/common

IO_CLOWNIX=${CLOONIX_COMMON}/lib_io_clownix
RPC_CLOWNIX=${CLOONIX_COMMON}/lib_rpc_clownix/
UTILS=${CLOONIX_COMMON}/lib_utils/
GLOB_INCLUDE=${CLOONIX_COMMON}/glob_include

LIB_SUBSETS=../subsets/include
LIB_INTERFACE=../interface/include
OBJ_DIR=obj/
LIB = libbank.a
VPATH=./src
WARNINGS=-Wall

INCLUDES= -I${IO_CLOWNIX}/include \
          -I./include \
          -I${RPC_CLOWNIX}/include \
          -I${GLOB_INCLUDE} \
          -I.. \
          -I${LIB_SUBSETS} \
          -I${LIB_INTERFACE} \
          -I${UTILS}/include 

CFLAGS+= -g -rdynamic $(WARNINGS) $(INCLUDES)


AR = ar
RANLIB = ranlib
CC?=gcc

SRC = bank_item.c add_new_item.c selection.c from_interface.c bijection.c \
      hidden_visible_edge.c

all: $(shell mkdir -p ./obj) $(LIB)
OBJ = $(SRC:.c=.o)
OBJS = $(addprefix $(OBJ_DIR), $(OBJ))
$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@
$(LIB): $(OBJS)
	$(AR) rc $(LIB) $(OBJS)
	$(RANLIB) $(LIB)
clean:
	@-rm -vf libbank.a $(OBJ_DIR)*.o $(OBJ_DIR)*.a *~ 
