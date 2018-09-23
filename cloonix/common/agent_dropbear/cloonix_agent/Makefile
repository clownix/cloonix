OBJ_DIR=./obj/
WARNINGS=-Wall 
EXE = cloonix_agent

SRC = action.c  \
      cloonix_agent.c \
      sock.c \
      x11_channels.c \
      nonblock.c

VPATH=src
INCLUDES= -I./src
LIBS = 
CFLAGS+= -g $(WARNINGS) $(INCLUDES)
all: $(shell mkdir -p ./obj) $(EXE)
OBJ = $(SRC:.c=.o)
OBJS = $(addprefix $(OBJ_DIR), $(OBJ))

$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(EXE): $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $(EXE) 

clean:
	@-rm -vrf $(OBJ_DIR) $(EXE)

