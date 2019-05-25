OBJ_DIR=./obj/
WARNINGS=-Wall 
EXE = qmp_cnf
SRC = qmp_sock.c keystrokes.c unix_sock.c oper_auto.c
VPATH=src
INCLUDES= -I./src 
LIBS = 
CFLAGS+= -g $(WARNINGS) $(INCLUDES)
CC=gcc
all: $(shell mkdir -p ./obj) $(EXE)
OBJ = $(SRC:.c=.o)
OBJS = $(addprefix $(OBJ_DIR), $(OBJ))

$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(EXE): $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $(EXE) 


install:
	install -m 755 $(EXE) $(DESTDIR)/usr/bin

clean:
	@-rm -vrf $(OBJ_DIR) $(EXE)

