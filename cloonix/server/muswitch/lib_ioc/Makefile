CLOONIX_COMMON=../../../../cloonix/common

GLOB_INCLUDE=${CLOONIX_COMMON}/glob_include
RPCT=${CLOONIX_COMMON}/lib_rpct


OBJ_DIR=obj/
LIB = libioc.a
VPATH = ./src/
WARNINGS=-Wall

INCLUDES= -I./src \
          -I./include \
          -I${GLOB_INCLUDE} \
          -I${RPCT}/include

CFLAGS+= -fPIC -g $(WARNINGS) $(INCLUDES) -lrt 

AR = ar
RANLIB = ranlib
CC?=gcc
SRC =  channel.c \
       util_sock.c \
       msg_layer.c \
       chunk.c \
       rx_process.c \
       clownix_timer.c \
       out_of_epoll.c \
       ioc_ctx.c \
       clownix_real_timer.c

all: $(shell mkdir -p ./obj) $(LIB)
OBJ = $(SRC:.c=.o)
OBJS = $(addprefix $(OBJ_DIR), $(OBJ))
$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@
$(LIB): $(OBJS)
	$(AR) rc $(LIB) $(OBJS)
	$(RANLIB) $(LIB)
clean:
	@-rm -vf libioc.a $(OBJ_DIR)*.o $(OBJ_DIR)*.a 




