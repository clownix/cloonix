OBJ_DIR=.
LIB = libcrcanvas_gtk.a
SRC = \
     cr-item.c \
     cr-types.c \
     cr-marshal.c \
     cr-text.c \
     cr-panner.c \
     cr-canvas.c \

INCLUDES= -I. $(shell pkg-config --cflags gtk+-3.0) \
          -I. $(shell pkg-config --cflags pangocairo) 

LIBS = $(shell pkg-config --libs gtk+-3.0) \
       $(shell pkg-config --libs pangocairo) 


CFLAGS+= -g -Wall $(INCLUDES)

CFLAGS+= -DGTK_DISABLE_SINGLE_INCLUDES \
         -DGDK_DISABLE_DEPRECATED \
         -DGTK_DISABLE_DEPRECATED \
         -DGSEAL_ENABLE


CC=gcc
all: $(LIB)
OBJ = $(SRC:.c=.o)
OBJS = $(addprefix $(OBJ_DIR), $(OBJ))

$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

AR = ar

RANLIB = ranlib

$(LIB): $(OBJS)
	$(AR) rc $(LIB) $(OBJS)
	$(RANLIB) $(LIB)


clean:
	@-rm -vf $(OBJ_DIR)*.o $(LIB) 

