IO_CLOWNIX=../../../lib_io_clownix
GLOB_INCLUDE=../../../glob_include

OBJ_DIR=obj/
LIB = libsubsets.a
VPATH=./src
WARNINGS=-Wall
INCLUDES= -I${IO_CLOWNIX}/include \
          -I$(GLOB_INCLUDE) \
          -I./include  

CFLAGS+=-mtune=generic -fno-stack-protector
CFLAGS+= -g -rdynamic $(WARNINGS) $(INCLUDES)
AR = ar
RANLIB = ranlib
CC?=gcc
SRC = dijkstra.c subsets.c 

all: $(shell mkdir -p ./obj) $(LIB)

OBJ = $(SRC:.c=.o)
OBJS = $(addprefix $(OBJ_DIR), $(OBJ))
$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@
$(LIB): $(OBJS)
	$(AR) rc $(LIB) $(OBJS)
	$(RANLIB) $(LIB)
clean:
	@-rm -vf libsubsets.a $(OBJ_DIR)*.o $(OBJ_DIR)*.a *~ 
