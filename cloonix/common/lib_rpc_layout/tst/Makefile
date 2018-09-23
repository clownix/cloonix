OBJ_DIR=.
EXE = tester
VPATH=.
SRC = tester.c 
WARNINGS=-Wall -Wstrict-prototypes -Wdeclaration-after-statement
IO_CLOWNIX=../../../lib_io_clownix
RPC_CLOWNIX=../../lib_rpc_clownix
GLOB_INCLUDE=../../glob_include
UTILS=../../lib_utils
BLKD=../../muswitch/lib_blkd
RPCT=../../muswitch/lib_rpct

INCLUDES= -I../src \
          -I../include \
          -I${GLOB_INCLUDE} \
          -I${IO_CLOWNIX}/include \
          -I${RPC_CLOWNIX}/include \
          -I../../lib_client \
          -I${UTILS}/include

LIBS = -L../ -lrpc_layout \
       -L${IO_CLOWNIX} -lio_clownix \
       -L${BLKD} -lblkd \
       -L${RPCT} -lrpct \
       -L${UTILS} -lutils -lssl -lcrypto -lrt -lm
CFLAGS+= -g $(WARNINGS) $(INCLUDES)
CC=gcc
all: $(EXE)
OBJ = $(SRC:.c=.o)
OBJS = $(addprefix $(OBJ_DIR), $(OBJ))

$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(EXE): $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $(EXE) 

clean:
	@-rm -vf $(OBJ_DIR)*.o *~ $(VPATH)/*~ $(EXE) core*
