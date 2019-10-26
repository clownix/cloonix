BLKD=../lib_blkd

OBJ_DIR=obj/
LIB = libioc.a
VPATH = ./src/
WARNINGS=-Wall

INCLUDES= -I./src \
          -I${BLKD}/src

CFLAGS += $(WARNINGS) $(INCLUDES)

AR?=ar
RANLIB?=ranlib
CC?=gcc

SRC =  channel.c \
       msg_layer.c \
       clownix_timer.c \
       ioc_ctx.c

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




