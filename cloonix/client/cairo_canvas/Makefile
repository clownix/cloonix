CLOONIX_COMMON=../../../cloonix/common

IO_CLOWNIX=${CLOONIX_COMMON}/lib_io_clownix
RPC_CLOWNIX=${CLOONIX_COMMON}/lib_rpc_clownix/
UTILS=${CLOONIX_COMMON}/lib_utils/
RPC_LAYOUT=${CLOONIX_COMMON}/lib_rpc_layout/
GLOB_INCLUDE=${CLOONIX_COMMON}/glob_include
RPCT=${CLOONIX_COMMON}/lib_rpct


LIB_CLIENT=../lib_client
LIB_INTF=./src/interface
LIB_BANK=./src/bank
LIB_SUBSETS=./src/subsets
LIB_BDPLOT=./src/bdplot

EXE      = cloonix_gui
WARNINGS=-Wall
VPATH=src:${LIB_CLIENT}:${LIB_INTF}/src

INCLUDES= -I. $(shell pkg-config --cflags gtk+-3.0) \
          -I. $(shell pkg-config --cflags pangocairo) \
          -I./src \
          -I${IO_CLOWNIX}/include \
          -I${GLOB_INCLUDE} \
          -I${RPC_CLOWNIX}/include \
          -I${UTILS}/include \
          -I${RPC_LAYOUT}/include \
          -I${LIB_CLIENT}/include \
          -I${LIB_INTF}/include \
          -I${LIB_BANK}/include \
          -I${LIB_SUBSETS}/include \
          -I${LIB_BDPLOT}/include \
          -I./crcanvas_gtk

LIBS = $(shell pkg-config --libs gtk+-3.0) \
       -L${LIB_CLIENT}/ -lglibclient \
       -L${RPC_LAYOUT}/ -lrpc_layout \
       -L${RPC_CLOWNIX}/ -lrpc_clownix \
       -L${LIB_INTF}/ -linterface \
       -L${LIB_BANK}/ -lbank \
       -L${LIB_SUBSETS}/ -lsubsets \
       -L${LIB_BDPLOT}/ -lbdplot \
       -L${IO_CLOWNIX}/ -lio_clownix \
       -L${RPCT}/ -lrpct \
       -lssl -lcrypto -lrt \
       -L./crcanvas_gtk/ -lcrcanvas_gtk \
       -L${LIB_CLIENT}/ -lglibclient \
       $(shell pkg-config --libs pangocairo) \
       -L${UTILS}/ -lutils -lm \
       $(shell pkg-config --libs gtk+-3.0) 

SRCS     = popup.c topo.c menus.c cloonix.c move.c menu_utils.c \
           main_timer_loop.c canvas_ctx.c eventfull_eth.c make_layout.c \
           menu_dialog_kvm.c menu_dialog_d2d.c layout_topo.c 

OBJ      = $(SRCS:.c=.o)
OBJ_DIR  = ./obj/
OBJS     = $(addprefix $(OBJ_DIR), $(OBJ))


CFLAGS   = -g -rdynamic $(WARNINGS) $(INCLUDES) -DWITH_GLIB -D_GNU_SOURCE

 
CFLAGS+= -DGTK_DISABLE_SINGLE_INCLUDES \
         -DGTK_DISABLE_DEPRECATED \
         -DGDK_DISABLE_DEPRECATED \
         -DGSEAL_ENABLE


CC       = gcc 

all: $(shell mkdir -p ./obj) $(EXE)

$(EXE): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBS) $(LDFLAGS)
	@echo

$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

clean:	
	@-rm -vf *~ cloonix_gui $(OBJ_DIR)*.o


















