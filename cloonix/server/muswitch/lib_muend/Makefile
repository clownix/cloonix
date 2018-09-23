CLOONIX_COMMON=../../../../cloonix/common

GLOB_INCLUDE=${CLOONIX_COMMON}/glob_include
BLKD=${CLOONIX_COMMON}/lib_blkd
RPCT=${CLOONIX_COMMON}/lib_rpct

IOC=../lib_ioc

OBJ_DIR=obj/
LIB = libmuend.a
VPATH = ./src/
WARNINGS=-Wall

INCLUDES= -I./src \
          -I./include \
          -I${GLOB_INCLUDE} \
          -I${IOC}/include \
          -I${BLKD}/include \
          -I${RPCT}/include


CFLAGS+= -fPIC -g $(WARNINGS) $(INCLUDES) 

AR = ar
RANLIB = ranlib

CC=gcc

SRC = diag.c \
      sock_fd.c \
      mueth.c \
      stub.c

all: $(shell mkdir -p ./obj) $(LIB)
OBJ = $(SRC:.c=.o)
OBJS = $(addprefix $(OBJ_DIR), $(OBJ))
$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@
$(LIB): $(OBJS)
	$(AR) rc $(LIB) $(OBJS)
	$(RANLIB) $(LIB)
clean:
	@-rm -vf $(LIB) $(OBJ_DIR)*.o $(OBJ_DIR)*.a 




