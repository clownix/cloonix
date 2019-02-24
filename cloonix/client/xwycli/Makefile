CLOONIX_COMMON=../../../cloonix/common

IO_CLOWNIX=${CLOONIX_COMMON}/lib_io_clownix
GLOB_INCLUDE=${CLOONIX_COMMON}/glob_include
BLKD=${CLOONIX_COMMON}/lib_blkd
RPCT=${CLOONIX_COMMON}/lib_rpct
LIBMDL=${CLOONIX_COMMON}/xwy/libmdl
LIBCLI=${CLOONIX_COMMON}/xwy/libcli



OBJ_DIR=./obj/
WARNINGS=-Wall 
VPATH=src

EXE = xwycli
SRC = main.c stub.c

INCLUDES= -I./src \
          -I${IO_CLOWNIX}/include \
          -I${LIBMDL} \
          -I${LIBCLI} \
          -I${GLOB_INCLUDE}

LIBS = -L${IO_CLOWNIX} -lio_clownix \
       -L${BLKD} -lblkd \
       -L${RPCT} -lrpct \
       -L${LIBCLI} -lcli \
       -L${LIBMDL} -lmdl \
       -lssl -lcrypto -lrt -lm -lutil -lpthread 


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

