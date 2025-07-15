IO_CLOWNIX=../lib_io_clownix
RPC_CLOWNIX=../lib_rpc_clownix
GLOB_INCLUDE=../glob_include
UTILS=../lib_utils
OBJ_DIR=obj/
WARNINGS=-Wall -Wextra -Wstrict-prototypes
VPATH = src
LIB = librpc_layout.a
SRC = xml_layout.c

INCLUDES= -I./src \
          -I./include \
          -I${GLOB_INCLUDE} \
          -I${IO_CLOWNIX}/include \
          -I${RPC_CLOWNIX}/include \
          -I${UTILS}/include 

CFLAGS+=-mtune=generic -fno-stack-protector
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
