# The windows insider agent, cross compiled.
OBJ_DIR=./obj/
WARNINGS=-Wall
EXE = cloonix-agent.exe

CC = x86_64-w64-mingw32-gcc

# sock_header.c is shared with the linux agent.
SRC = cloonix_agent_win.c \
      sock_header.c

VPATH=src:../../glob_include
INCLUDES= -I./src -I../../glob_include
# winsock: the legs are loopback sockets to the pty server
LIBS = -lws2_32
# mingw msvcrt printf has no %llu; the ansi stdio of mingw-w64 does
CFLAGS+= -g -D__USE_MINGW_ANSI_STDIO=1 $(WARNINGS) $(INCLUDES)
all: $(shell mkdir -p ./obj) $(EXE)
OBJ = $(SRC:.c=.o)
OBJS = $(addprefix $(OBJ_DIR), $(OBJ))

$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(EXE): $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $(EXE)
	x86_64-w64-mingw32-strip $(EXE)

clean:
	@-rm -vrf $(OBJ_DIR) $(EXE)
