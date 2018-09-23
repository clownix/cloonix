OBJ_DIR=.
EXE = tester
SRC = tester.c 
WARNINGS = -Wall
IO_CLOWNIX=../../../lib_io_clownix
GLOB_INCLUDE=../../glob_include
BLKD=../../muswitch/lib_blkd
RPCT=../../muswitch/lib_rpct/


INCLUDES= -I../src \
          -I../include \
          -I${GLOB_INCLUDE} \
          -I${IO_CLOWNIX}/include 
          
LIBS = -L../ -lrpc_c2c \
       -L${IO_CLOWNIX} -lio_clownix \
       -L${BLKD} -lblkd \
       -L${RPCT} -lrpct \
       -lrt

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
