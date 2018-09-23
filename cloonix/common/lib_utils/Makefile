IO_CLOWNIX=../lib_io_clownix
RPC_CLOWNIX=../lib_rpc_clownix
GLOB_INCLUDE=../glob_include
OBJ_DIR=obj/
WARNINGS=-Wall -Wextra -Wstrict-prototypes
VPATH = src
LIB = libutils.a
SRC = xml_utils.c file_read_write.c \
      diff_topo.c topo_items.c 

INCLUDES= -I./src \
          -I./include \
          -I${GLOB_INCLUDE} \
          -I${IO_CLOWNIX}/include \
          -I${RPC_CLOWNIX}/include

LIBS = 
CFLAGS+= -g $(WARNINGS) $(INCLUDES)
AR = ar
RANLIB = ranlib
CC=gcc

all: $(shell mkdir -p ./obj) $(LIB)

OBJ = $(SRC:.c=.o)
OBJS = $(addprefix $(OBJ_DIR), $(OBJ))

$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(LIB): $(OBJS)
	$(AR) rc $(LIB) $(OBJS)
	$(RANLIB) $(LIB)

clean:
	@-rm -vf $(OBJ_DIR)*.o *~ $(VPATH)/*~ $(LIB)
