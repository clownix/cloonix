BLKD=./lib_blkd
IOC=./lib_ioc

SRC = blkd_addr.c hwsim.c main.c iface.c
OBJ_DIR = obj/
VPATH = src
EXE = cloonix_hwsim


WARNINGS=-Wall -Wstrict-prototypes -Wdeclaration-after-statement

INCLUDES= -I./src \
          -I$(BLKD)/src \
          -I$(IOC)/src

LIBS = -L$(BLKD) -lblkd \
       -L$(IOC) -lioc


#libnl-3-dev libnl-genl-3-dev 

CFLAGS += $(shell pkg-config --cflags libnl-3.0)
CFLAGS += $(WARNINGS) $(INCLUDES)

LDFLAGS += $(shell pkg-config --libs libnl-3.0)
LDFLAGS += -lnl-genl-3


OBJECTS=hwsim.o main.o

all: $(shell mkdir -p ./obj) $(EXE)

OBJ = $(SRC:.c=.o)

OBJS = $(addprefix $(OBJ_DIR), $(OBJ))

$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(EXE): $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $(EXE) 

clean:
	@-rm -vf $(OBJ_DIR)*.o $(EXE) 
