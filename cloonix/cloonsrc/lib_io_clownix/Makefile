OBJ_DIR=obj/
LIB = libio_clownix.a
VPATH = ./src/:../glob_include
WARNINGS=-Wall

GLOB_INCLUDE=../glob_include
INCLUDES= -I./src \
          -I./include \
          -I${GLOB_INCLUDE} 

CFLAGS+=-mtune=generic -fno-stack-protector
CFLAGS+=-fPIC -g -rdynamic $(WARNINGS) $(INCLUDES) -lrt 

AR?=ar
RANLIB?=ranlib
CC?=gcc

SRC =  pthexec.c \
       channel.c \
       rpct.c \
       util_sock.c \
       msg_layer.c \
       clownix_timer.c \
       chunk.c \
       rx_process.c \
       tx_process.c  \
       job_for_select.c \
       hmac_cipher.c \
       cloonix_conf_info.c \
       doorways_sock.c

all: $(shell mkdir -p ./obj) $(LIB)

OBJ = $(SRC:.c=.o)
OBJS = $(addprefix $(OBJ_DIR), $(OBJ))
$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@
$(LIB): $(OBJS)
	$(AR) rc $(LIB) $(OBJS)
	$(RANLIB) $(LIB)
clean:
	@-rm -vf libio_clownix.a $(OBJ_DIR)*.o $(OBJ_DIR)*.a $(VPATH)*~ *~ ./include/*~
