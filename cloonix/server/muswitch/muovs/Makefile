CLOONIX_COMMON=../../../../cloonix/common

GLOB_INCLUDE=${CLOONIX_COMMON}/glob_include
BLKD=${CLOONIX_COMMON}/lib_blkd
RPCT=${CLOONIX_COMMON}/lib_rpct
IOC=../lib_ioc

OBJ_DIR=obj/
EXE = cloonix_muovs
VPATH=src

SRC = main.c ovs_execv.c my_popen.c

WARNINGS=-Wall -Wdeprecated -Wstrict-prototypes -Wdeclaration-after-statement

INCLUDES= -I./src \
          -I./include \
          -I${GLOB_INCLUDE} \
          -I${IOC}/include \
          -I${RPCT}/include

LIBS = -L${BLKD} -lblkd \
       -L${IOC} -lioc \
       -L${RPCT} -lrpct

CFLAGS += -O3 -mavx $(INCLUDES) $(WARNINGS)

CC=gcc
all: $(shell mkdir -p ./obj) $(EXE)
OBJ = $(SRC:.c=.o)
OBJS = $(addprefix $(OBJ_DIR), $(OBJ))

$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(EXE): $(OBJS)
	$(CC) $^ $(LDFLAGS) $(LIBS) -o $(EXE) 

clean:
	@-rm -vf $(OBJ_DIR)*.o $(EXE) 

