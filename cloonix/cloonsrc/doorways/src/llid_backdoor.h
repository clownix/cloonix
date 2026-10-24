/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
typedef struct t_llid_dido
{
  int dido_llid;
  struct t_llid_dido *prev;
  struct t_llid_dido *next;
} t_llid_dido;
typedef struct t_backdoor_vm
{                       
  char name[MAX_NAME_LEN];
  char backdoor[MAX_PATH_LEN];
  int backdoor_llid; 
  int backdoor_fd;
  int connect_count;
  long long heartbeat_abeat;
  int heartbeat_ref;
  long long connect_abs_beat_timer;
  int connect_ref_timer;
  t_llid_dido *llid_dido_traf;
  t_rx_pktbuf rx_pktbuf;
  int ping_agent_rx;
  int last_ping_agent_rx;
  int fail_ping_agent_rx;
  int ping_status;
  int ping_id;
  int reboot_id;
  int halt_id;
  int cloonix_up_and_running;
  /* So the "no ping is being sent" warning is written once, not per beat. */
  int said_not_up;
  int cloonix_not_yet_connected;
  uint64_t reboot_ms;
  /* a close waits BACKDOOR_CLOSE_GRACE_MS for the delete that explains it */
  uint64_t closed_ms;
  char closed_how[MAX_NAME_LEN];
  /* the radios of the kvm, see kvmwifi.c */
  int wifi;
  int wifi_nb;
  /* per radio the peers suid_power told last, -1 for none yet */
  uint8_t wifi_peers[MAX_VWIF][KVMWIFI_MAX_PEERS][6];
  int wifi_nb_peers[MAX_VWIF];
  char wifi_req[MAX_NAME_LEN];
  uint64_t wifi_ask_ms;
  t_kerr_gate kerr_gate;
  struct t_backdoor_vm *prev;
  struct t_backdoor_vm *next;
} t_backdoor_vm;
int  llid_backdoor_get_info(char *name, int *llid_backdoor);
void llid_backdoor_add_traf(char *name, int llid_backdoor, int llid_dido_traf);
void llid_backdoor_del_traf(char *name, int llid_backdoor, int llid_dido_traf);
void llid_backdoor_tx(char *name, int llid_backdoor, int llid_dido_traf,
                      int len, int type, int val, char  *buf);
void llid_backdoor_begin_unix(char *name, char *path);
void llid_backdoor_end_unix(char *name);
void llid_backdoor_tx_halt_to_agent(char *name);
void llid_backdoor_tx_clip_set_to_agent(char *name, char *txt);
void llid_backdoor_tx_clip_get_to_agent(char *name);
void llid_backdoor_tx_reboot_to_agent(char *name);
void llid_backdoor_init(void);
int  llid_backdoor_ping_status_is_ok(char *name);
void llid_backdoor_cloonix_up_vport_and_running(char *name);
void llid_backdoor_cloonix_down_and_not_running(char *name);

t_backdoor_vm *llid_backdoor_get_bvm(char *name);
t_backdoor_vm *llid_backdoor_get_first(void);
void llid_backdoor_tx_bvm(t_backdoor_vm *bvm, int len, int type, int val,
                          char *buf);
void llid_backdoor_reboot_announced(char *name);
