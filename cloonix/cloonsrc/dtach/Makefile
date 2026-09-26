OBJ_DIR=./obj/
EXE = cloonix-dtach
SRC = main.c master.c attach.c
VPATH=src
INCLUDES= -I./src
# Wno-unused-result: pty and client socket writes are best effort by design.
WARNINGS=-Wall -Wextra -Wno-unused-result
CFLAGS+= -O2 -mtune=generic -fno-stack-protector -g $(WARNINGS) $(INCLUDES)
LIBS = -lutil

all: $(shell mkdir -p ./obj) $(EXE)

OBJ = $(SRC:.c=.o)
OBJS = $(addprefix $(OBJ_DIR), $(OBJ))

$(OBJ_DIR)%.o: %.c src/dtach.h
	$(CC) $(CFLAGS) -c $< -o $@

$(EXE): $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $(EXE)
	strip $(EXE)

clean:
	@-rm -vrf $(OBJ_DIR) $(EXE) src/*~

.PHONY: all clean
