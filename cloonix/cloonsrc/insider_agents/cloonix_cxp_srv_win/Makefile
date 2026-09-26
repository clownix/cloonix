# The windows pty server, cross compiled like the agent beside it.
OBJ_DIR=./obj/
WARNINGS=-Wall
EXE = cloonix-cxp-srv.exe

CC = x86_64-w64-mingw32-gcc

# cxp_wire.c is shared with the other cxp server.
SRC = cloonix_cxp_serv_win.c \
      cxp_wire.c

VPATH=src:../../glob_include
INCLUDES= -I./src -I../../glob_include
LIBS = -lws2_32 -lwtsapi32 -luserenv
CFLAGS+= -g -D__USE_MINGW_ANSI_STDIO=1 -D_WIN32_WINNT=0x0A00 \
         $(WARNINGS) $(INCLUDES)
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
