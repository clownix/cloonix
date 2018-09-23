CLOONIX_COMMON=../../../../cloonix/common

GLOB_INCLUDE=${CLOONIX_COMMON}/glob_include
BLKD=${CLOONIX_COMMON}/lib_blkd
RPCT=${CLOONIX_COMMON}/lib_rpct

IOC=../lib_ioc
LIB_MUEND=../lib_muend
CLO_TCP=./clo_tcp


OBJ_DIR=obj/
EXE = cloonix_munat
VPATH=src

SRC = main.c \
      unix2inet.c \
      utils.c \
      machine.c \
      bootp_input.c \
      packets_io.c \
      packet_ip_input.c \
      packet_icmp_input.c \
      packet_udp_input.c \
      llid_slirptux.c \
      tcp_tux.c \
      tcp_connect_wait_mngt.c

WARNINGS=-Wall -Wstrict-prototypes -Wdeclaration-after-statement


INCLUDES= -I./src \
          -I./include \
          -I${CLO_TCP}/include \
          -I${GLOB_INCLUDE} \
          -I${IOC}/include \
          -I${LIB_MUEND}/include \
          -I${RPCT}/include 


LIBS = -L${CLO_TCP} -lclo \
       -L${BLKD} -lblkd \
       -L${IOC} -lioc \
       -L${RPCT} -lrpct \
       -L${LIB_MUEND} -lmuend \
       -lrt


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
	@-rm -vf $(OBJ_DIR)*.o $(EXE) 


