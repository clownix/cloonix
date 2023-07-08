CLOONIX_COMMON=../../../cloonix/common
IO_CLOWNIX=${CLOONIX_COMMON}/lib_io_clownix
RPC_CLOWNIX=${CLOONIX_COMMON}/lib_rpc_clownix
RPC_LAYOUT=${CLOONIX_COMMON}/lib_rpc_layout
UTILS=${CLOONIX_COMMON}/lib_utils/
GLOB_INCLUDE=${CLOONIX_COMMON}/glob_include

WARNINGS=-Wall -Wstrict-prototypes
OBJ_DIR=obj/
OBJ_DIR_GLIB=obj_glib/
VPATH=src

SRC = \
        client_clownix.c \
        client_stub.c \
        pid_clone.c \
        glib_client_clownix.c 


LIB=libclient.a


INCLUDES= -I./include \
          -I${GLOB_INCLUDE} \
          -I${IO_CLOWNIX}/include \
          -I${RPC_CLOWNIX}/include \
          -I${RPC_LAYOUT}/include \
          -I${UTILS}/include 

CFLAGS+=-mtune=generic -fno-stack-protector
CFLAGS += -g $(WARNINGS) $(INCLUDES)
CFGLIB= -I. $(shell pkg-config --cflags gtk+-3.0) \
        -DWITH_GLIB -DGTK_DISABLE_SINGLE_INCLUDES \
        -DGDK_DISABLE_DEPRECATED \
        -DGTK_DISABLE_DEPRECATED \
        -DGSEAL_ENABLE



AR = ar
RANLIB = ranlib
CC=gcc
all: $(shell mkdir -p ./obj) $(shell mkdir -p ./obj_glib) $(LIB)
OBJ = $(SRC:.c=.o)
OBJS = $(addprefix $(OBJ_DIR), $(OBJ))
OBJS_GLIB = $(addprefix $(OBJ_DIR_GLIB), $(OBJ))

$(OBJ_DIR_GLIB)%.o: %.c
	$(CC) $(CFLAGS) $(CFGLIB) -c $^ -o $@

$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(LIB): $(OBJS)
	$(AR) rc $(LIB) $(OBJS)
	$(RANLIB) $(LIB)

libglibclient.a: $(OBJS_GLIB)
	$(AR) rc libglibclient.a $(OBJS_GLIB)
	$(RANLIB) libglibclient.a


clean:
	@-rm -vf $(OBJ_DIR)*.o $(OBJ_DIR_GLIB)*.o $(LIB) libglibclient.a

