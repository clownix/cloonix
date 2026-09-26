/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
void wifi_air_beat(void);
void wifi_air_snf(int llid, int tid, char *air, int on);
int wifi_air_snf_names(char names[][MAX_NAME_LEN], int max);
int wifi_air_snf_on(char *air);
int wifi_air_is_up(char *air);
int wifi_air_in_use(char *air);
int wifi_air_wlan(char *name, int num, int *idx);
int wifi_air_num(char *name, int num);
char *wifi_air_iface(char *name, int num);
void wifi_air_plug(int llid, int tid, char *name, int num, char *air,
                   int add);
void wifi_air_kvm_radio(t_vm *vm);
void wifi_air_kvm_done(char *line);
void wifi_air_kvm_gone(t_vm *vm);
void wifi_air_kvm_default(t_topo_kvm *kvm);
