IO_CLOWNIX=../lib_io_clownix
GLOB_INCLUDE=../glob_include
RPC_CLOWNIX=../lib_rpc_clownix/
OBJ_DIR=obj/
VPATH = src
EXE = cloonix-main-server
WARNINGS=-Wall -Wno-format-truncation -Wno-format-overflow

SRC = \
      uml_clownix_switch.c \
      recv_from_client.c \
      pcap_relay.c \
      pcap_filter.c \
      recv_add_lan.c \
      heartbeat.c \
      machine_create.c \
      utils_cmd_line_maker.c \
      event_subscriber.c \
      system_callers.c \
      pid_clone.c \
      automates.c \
      lan_to_name.c \
      cfg_store.c \
      doorways_mngt.c \
      gui_server_mngt.c \
      llid_trace.c \
      eventfull.c \
      qemu_automaton.c \
      qcow2_hdr.c \
      hop_event.c \
      qmp_dialog.c \
      qmp.c \
      qga_dialog.c \
      kvm.c \
      ovs.c \
      ovs_snf.c \
      wifi_air.c \
      ack_pending.c \
      ovs_nat_main.c \
      proxymous.c \
      ovs_a2b.c \
      ovs_tap.c \
      ovs_phy.c \
      ovs_c2c.c \
      c2c_peer.c \
      msg.c \
      list_commands.c \
      fmt_diag.c \
      slowperiodic.c \
      suid_power.c \
      dispatch_diag_msg.c \
      cnt.c \
      crun.c \
      base64.c \
      mactopo.c \
      proxycrun.c \
      c2c_chainlan.c \
      layout_store.c \
      targz.c

INCLUDES= -I./src \
          -I$(GLOB_INCLUDE) \
          -I${IO_CLOWNIX}/include \
           \
          -I${RPC_CLOWNIX}/include

# rpc uses io, so rpc comes first for a static link
LIBS = -L${RPC_CLOWNIX} -lrpc_clownix \
       -L${IO_CLOWNIX} -lio_clownix \
         -lcrypto -lrt \
       -lutil -lm 

CFLAGS+=-mtune=generic -fno-stack-protector
CFLAGS+= -g -rdynamic $(WARNINGS) $(INCLUDES)  

LDFLAGS+= -g -rdynamic

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
