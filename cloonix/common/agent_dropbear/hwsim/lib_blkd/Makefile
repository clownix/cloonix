OBJ_DIR=obj/
LIB = libblkd.a
VPATH = ./src/
WARNINGS=-Wall
GLOB_INCLUDE=../glob_include


INCLUDES= -I./src \
          -I${GLOB_INCLUDE} 

CFLAGS+= -fPIC -g $(WARNINGS) $(INCLUDES) 

AR?=ar
RANLIB?=ranlib
CC?=gcc

SRC = sock_unix.c \
      common.c \
      blkd.c \
      blkd_tx.c \
      blkd_rx.c

all: $(shell mkdir -p ./obj) $(LIB)

OBJ = $(SRC:.c=.o)
OBJS = $(addprefix $(OBJ_DIR), $(OBJ))
$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@
$(LIB): $(OBJS)
	$(AR) rc $(LIB) $(OBJS)
	$(RANLIB) $(LIB)
clean:
	@-rm -vf libblkd.a $(OBJ_DIR)*.o $(OBJ_DIR)*.a 




