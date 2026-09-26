/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
enum {
  item_cnt=7,
  item_kvm,
  item_tap,
  item_phya,
  item_phym,
  item_nat,
  item_a2b,
  item_c2c,
};
int  lan_add_name(char *name, int item_type, char *item_name, int item_num);
int  lan_del_name(char *name, int item_type, char *item_name, int item_num);
int  lan_get_with_name(char *name);
void lan_rstp_set(char *name, int val);
int  lan_rstp_names(char names[][MAX_NAME_LEN], int max);
void lan_history_dump(char *name);
#define LAN_HISTORY_NOTHING 0
#define LAN_HISTORY_DIED    1
#define LAN_HISTORY_REUSED  2
int  lan_history_state(char *name);
void lan_init(void);

