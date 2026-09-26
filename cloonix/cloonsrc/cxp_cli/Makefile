OBJ_DIR=./obj/
WARNINGS=-Wall
EXE = cloonix-cxp-cli

SRC = cloonix_cxp_cli.c cxp_wire.c

VPATH=src:../glob_include
INCLUDES= -I./src -I../glob_include
LIBS = -lssl -lcrypto
CFLAGS+= -g $(WARNINGS) $(INCLUDES)
all: $(shell mkdir -p ./obj) $(EXE)
OBJ = $(SRC:.c=.o)
OBJS = $(addprefix $(OBJ_DIR), $(OBJ))

$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(EXE): $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $(EXE)
	strip $(EXE)

clean:
	@-rm -vrf $(OBJ_DIR) $(EXE)
