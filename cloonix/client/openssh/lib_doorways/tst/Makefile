OBJ_DIR=.
EXE = tst
SRC = doorways_tst.c 
WARNINGS=-Wall
IO_CLOWNIX=../../../../common/lib_io_clownix
BLKD=../../../../common/lib_blkd
RPCT=../../../../common/lib_rpct

INCLUDES= -I../include
          
LIBS = -L../ -ldoorways \
       -L${IO_CLOWNIX} -lio_clownix \
       -L${BLKD} -lblkd \
       -L${RPCT} -lrpct \
       -ldoorways -lssl -lcrypto -lrt

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
