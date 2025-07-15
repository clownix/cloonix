IO_CLOWNIX=../../../lib_io_clownix
RPC_CLOWNIX=../../../lib_rpc_clownix/
UTILS=../../../lib_utils/
RPC_LAYOUT=../../../lib_rpc_layout/
GLOB_INCLUDE=../../../glob_include
LIB_CLIENT=../../../lib_client
LIB_CANVAS=../../crcanvas-0.19/crcanvas
LIB_BANK=../bank/include
OBJ_DIR=obj/
LIB = libinterface.a
VPATH=./src
WARNINGS=-Wall

INCLUDES= -I. $(shell pkg-config --cflags gtk+-3.0) \
          -I${IO_CLOWNIX}/include \
          -I${LIB_BANK} \
          -I${UTILS}/include \
          -I${GLOB_INCLUDE} \
          -I${LIB_CANVAS} \
          -I${RPC_CLOWNIX}/include \
          -I${LIB_CLIENT}/include \
          -I${RPC_LAYOUT}/include \
          -I./include -I.. 


CFLAGS+=-mtune=generic -fno-stack-protector
CFLAGS+= -g -rdynamic $(WARNINGS) $(INCLUDES) -DWITH_GLIB
CFLAGS+= -DGTK_DISABLE_SINGLE_INCLUDES \
         -DGDK_DISABLE_DEPRECATED \
         -DGTK_DISABLE_DEPRECATED \
         -DGSEAL_ENABLE


AR = ar
RANLIB = ranlib
CC?=gcc

SRC = interface.c layout_x_y.c utils.c \
      timeout_start_req.c timeout_start_resp.c \
      timeout_end_req.c timeout_end_resp.c 

all: $(shell mkdir -p ./obj) $(LIB)

OBJ = $(SRC:.c=.o)
OBJS = $(addprefix $(OBJ_DIR), $(OBJ))
$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(LIB): $(OBJS)
	$(AR) rc $(LIB) $(OBJS)
	$(RANLIB) $(LIB)
clean:
	@-rm -vf libinterface.a $(OBJ_DIR)*.o $(OBJ_DIR)*.a *~ 
