OBJ_DIR=obj/
EXE = cloonix-proxy-crun-access
WARNINGS =-Wall -Wno-format-truncation -Wno-format-overflow
VPATH = src
SRC = accept_incoming_connect.c \
      ever_select_loop.c \
      main.c

INCLUDES = -I./src
LIBS =
CFLAGS = $(WARNINGS) $(INCLUDES)
CC=gcc 
all: $(shell mkdir -p ./obj) $(EXE)
OBJ = $(SRC:.c=.o)
OBJS = $(addprefix $(OBJ_DIR), $(OBJ))
$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@
$(EXE): $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $(EXE) 
clean:
	@-rm -vf $(OBJ_DIR)*.o $(EXE)
