CLOONIX_COMMON=../../../cloonix/common

IO_CLOWNIX=${CLOONIX_COMMON}/lib_io_clownix
GLOB_INCLUDE=${CLOONIX_COMMON}/glob_include
LIBMDL=${CLOONIX_COMMON}/xwy/libmdl
LIBCLI=${CLOONIX_COMMON}/xwy/libcli



OBJ_DIR=./obj/
WARNINGS=-Wall 
VPATH=src

EXE = cloonix-xwycli
SRC = main.c stub.c

INCLUDES= -I./src \
          -I${IO_CLOWNIX}/include \
          -I${LIBMDL} \
          -I${LIBCLI} \
          -I${GLOB_INCLUDE}

LIBS = -L${IO_CLOWNIX} -lio_clownix \
       -L${LIBCLI} -lcli \
       -L${LIBMDL} -lmdl \
       -lssl -lcrypto -lrt -lm -lutil -lpthread 


CFLAGS+=-mtune=generic -fno-stack-protector
CFLAGS+= -g $(WARNINGS) $(INCLUDES)
CC=gcc

all: $(shell mkdir -p ./obj) $(EXE)

OBJ = $(SRC:.c=.o)
OBJS = $(addprefix $(OBJ_DIR), $(OBJ))

$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(EXE): $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $(EXE) 

clean:
	@-rm -vf $(OBJ_DIR)*.o *~ src/*~ $(EXE)

