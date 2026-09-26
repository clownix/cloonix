OBJ_DIR=./obj/
# strip from the environment: the freebsd build needs a strip that reads a freebsd elf.
STRIP ?= strip
WARNINGS=-Wall
EXE = cloonix-cxp-srv

# cxp_wire.c is shared with the other cxp server.
SRC = cloonix_cxp_serv.c \
      cxp_wire.c

VPATH=src:../../glob_include
INCLUDES= -I./src -I../../glob_include
LIBS = -lutil
CFLAGS+= -g $(WARNINGS) $(INCLUDES)
all: $(shell mkdir -p ./obj) $(EXE)
OBJ = $(SRC:.c=.o)
OBJS = $(addprefix $(OBJ_DIR), $(OBJ))

$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(EXE): $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $(EXE)
	$(STRIP) $(EXE)

clean:
	@-rm -vrf $(OBJ_DIR) $(EXE)
