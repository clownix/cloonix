IO_CLOWNIX=../../lib_io_clownix
GLOB_INCLUDE=../../glob_include
OBJ_DIR=obj/
WARNINGS=-Wall
VPATH = src
LIB = libdoorways.a
SRC = doorways_access.c \
      util_tools.c \
      stub.c

INCLUDES= -I./src \
          -I./include \
          -I${IO_CLOWNIX}/include \
          -I${GLOB_INCLUDE}

CFLAGS+=-mtune=generic -fno-stack-protector -Wno-format-truncation
CFLAGS+=-fPIC -g $(WARNINGS) $(INCLUDES)
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
