CLOONIX_COMMON=../../../cloonix/common

IO_CLOWNIX=${CLOONIX_COMMON}/lib_io_clownix
GLOB_INCLUDE=${CLOONIX_COMMON}/glob_include
RPC_CLOWNIX=${CLOONIX_COMMON}/lib_rpc_clownix/
RPC_DOORS=${CLOONIX_COMMON}/lib_rpc_doors/
RPC_QMONITOR=${CLOONIX_COMMON}/lib_rpc_qmonitor/
RPC_LAYOUT=${CLOONIX_COMMON}/lib_rpc_layout
UTILS=${CLOONIX_COMMON}/lib_utils/
RPCT=${CLOONIX_COMMON}/lib_rpct
AGENT_INCLUDE=${CLOONIX_COMMON}/agent_dropbear/cloonix_agent/src


OBJ_DIR=obj/
VPATH = src
EXE = uml_cloonix_switch
#WARNINGS=-Wall -Wformat-truncation=0 -Wformat-overflow=0
WARNINGS=-Wall

SRC = \
      uml_clownix_switch.c \
      recv_from_client.c \
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
      llid_trace.c \
      eventfull.c \
      cdrom_creation_clone.c \
      qemu_automaton.c \
      hop_event.c \
      layout_topo.c \
      qmonitor.c \
      qmp_dialog.c \
      qmp.c \
      qhvc0.c \
      xwy.c \
      list_commands.c \
      stats_counters.c \
      stats_counters_sysinfo.c \
      fmt_diag.c \
      slowperiodic.c \
      suid_power.c \
      dpdk_d2d.c \
      dpdk_d2d_peer.c \
      xyx_dpdk_process.c \
      a2b_dpdk_process.c \
      d2d_dpdk_process.c \
      nat_dpdk_process.c \
      dpdk_ovs.c \
      dpdk_msg.c \
      dpdk_tap.c \
      dpdk_xyx.c \
      dpdk_nat.c \
      dpdk_a2b.c \
      dpdk_phy.c \
      dpdk_kvm.c \
      dispatch_diag_msg.c \
      container.c \
      server_stub.c

INCLUDES= -I./src \
          -I$(GLOB_INCLUDE) \
          -I${IO_CLOWNIX}/include \
          -I${RPC_LAYOUT}/include \
          -I${RPC_DOORS}/include \
          -I${UTILS}/include \
          -I${RPC_CLOWNIX}/include \
          -I${RPC_QMONITOR}/include \
          -I${AGENT_INCLUDE}

LIBS = -L${IO_CLOWNIX} -lio_clownix \
       -L${RPCT} -lrpct \
       -L${RPC_LAYOUT} -lrpc_layout \
       -L${RPC_DOORS} -lrpc_doors \
       -L${RPC_CLOWNIX} -lrpc_clownix \
       -L${RPC_QMONITOR} -lrpc_qmonitor \
       -L${UTILS} -lutils -lcrypto -lrt \
       -lutil -lm 


CFLAGS+= -g $(WARNINGS) $(INCLUDES)  
#LDFLAGS+= -rdynamic
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
