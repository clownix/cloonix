IO_CLOWNIX=../lib_io_clownix
GLOB_INCLUDE=../glob_include
AGENT_INCLUDE=../agent_dropbear/cloonix_agent/src
UTILS=../lib_utils
OBJ_DIR=obj/
WARNINGS=-Wall -Wextra -Wstrict-prototypes
VPATH = src
LIB = librpc_clownix.a
SRC = xml_interface.c 

INCLUDES= -I./src \
          -I${GLOB_INCLUDE} \
          -I./include \
          -I${IO_CLOWNIX}/include \
          -I${AGENT_INCLUDE} \
          -I${UTILS}/include

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
	@-rm -rvf $(OBJ_DIR) *~ $(VPATH)/*~ $(LIB)
