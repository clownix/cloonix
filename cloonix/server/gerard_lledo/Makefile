OBJ_DIR=obj/
VPATH=./src
EXE = cloonix-ext4fuse

CFLAGS+=-mtune=generic -fno-stack-protector
CFLAGS+= -I.src -std=gnu99 -g3 -Wall -Wextra
CFLAGS+= $(shell pkg-config fuse3 --cflags) -DFUSE_USE_VERSION=30
LDFLAGS += $(shell pkg-config fuse3 --libs)

SRC = dcache.c fuse-main.c op_getattr.c op_read.c super.c \
      disk.c inode.c op_init.c op_readdir.c extents.c logging.c \
      op_open.c op_readlink.c

all: $(shell mkdir -p ./obj) $(EXE)

OBJ = $(SRC:.c=.o)
OBJS = $(addprefix $(OBJ_DIR), $(OBJ))

$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(EXE): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	@-rm -vf $(OBJ_DIR)*.o $(EXE)
