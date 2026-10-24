OBJ_DIR=./obj/
# strip from the environment: the freebsd build needs a strip that reads a freebsd elf.
STRIP ?= strip
WARNINGS=-Wall 
EXE = cloonix-agent

# SIDE: main_kvm.c wifi.c (linux kvm), main_kvm.c wifi_none.c (freebsd kvm),
# main_cnt.c wifi_none.c (cnt).
SIDE ?= main_kvm.c wifi.c
SRC = action.c  \
      $(SIDE) \
      cloonix_agent.c \
      sock.c \
      nonblock.c \
      sock_header.c \
      rx_pktbuf.c

VPATH=src:../../glob_include
INCLUDES= -I./src -I../../glob_include
LIBS = 
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

