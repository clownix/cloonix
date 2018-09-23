CLOONIX_COMMON=../../../../common

GLOB_INCLUDE=${CLOONIX_COMMON}/glob_include
IOC=../../lib_ioc

OBJ_DIR=obj/
LIB = libclo.a
VPATH = ./src/
WARNINGS=-Wall
INCLUDES= -I${IOC}/include \
          -I./include \
          -I${GLOB_INCLUDE} \
          -I./src \

CFLAGS+= -g $(WARNINGS) $(INCLUDES)

AR = ar
RANLIB = ranlib
CC?=gcc
SRC =  clo_high.c  clo_low.c  clo_mngt.c  clo_utils.c 

all: $(shell mkdir -p ./obj) $(LIB)

OBJ = $(SRC:.c=.o)
OBJS = $(addprefix $(OBJ_DIR), $(OBJ))
$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@
$(LIB): $(OBJS)
	$(AR) rc $(LIB) $(OBJS)
	$(RANLIB) $(LIB)
clean:
	@-rm -vf libclo.a $(OBJ_DIR)*.o $(OBJ_DIR)*.a 
