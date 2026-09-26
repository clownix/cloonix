/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#define PING_KO "ping_agent_ko"
#define PING_OK "ping_agent_ok"
#define BACKDOOR_CONNECTED "backdoor_connected"
#define BACKDOOR_DISCONNECTED "backdoor_disconnected"
#define HALT_REQUEST "halt_requested"
#define REBOOT_REQUEST "reboot_requested"
/* a reboot asked of qemu: the agent is silent a while, as expected */
#define REBOOT_ANNOUNCED "reboot_announced"
#define REBOOT_QUIET_MS 90000
#define CLOONIX_UP_VPORT_AND_RUNNING "cloonix_up_vport_and_running"
#define CLIPBOARD_SET "clipboard_set"
#define CLIPBOARD_GET "clipboard_get"
#define CLIPBOARD_TXT "clipboard_txt"
/* a KERR line of a guest agent, after it */
#define AGENT_KERR "agent_kerr "
/* the whole command is KVMWIFI_ON_REQ of agent_wire.h */
#define KVMWIFI_ON "kvmwifi_on "
#define CLOONIX_DOWN_AND_NOT_RUNNING "cloonix_down_and_not_running"
void doors_send_info(int llid, int tid, char *type, char *info);
void doors_recv_info(int llid, int tid, char *type, char *info);
void doors_send_event(int llid, int tid, char *name, char *line);
void doors_recv_event(int llid, int tid, char *name, char *line);
void doors_send_command(int llid, int tid, char *name, char *cmd);
void doors_recv_command(int llid, int tid, char *name, char *cmd);
void doors_send_status(int llid, int tid, char *name, char *status);
void doors_recv_status(int llid, int tid, char *name, char *status);
void doors_send_add_vm(int llid, int tid, char *name, char *way2agent);
void doors_recv_add_vm(int llid, int tid, char *name, char *way2agent);
void doors_send_del_vm(int llid, int tid, char *name);
void doors_recv_del_vm(int llid, int tid, char *name);
void doors_json_init(void);
int doors_json_decoder (int llid, int len, char *str_rx);
