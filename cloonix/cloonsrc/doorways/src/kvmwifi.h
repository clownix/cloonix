/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
void kvmwifi_ask(char *name, char *req);
void kvmwifi_agent_alive(t_backdoor_vm *bvm);
void kvmwifi_agent_down(t_backdoor_vm *bvm);
void kvmwifi_rx_ctrl(t_backdoor_vm *bvm, int val, int len, char *payload);
void kvmwifi_rx_frame(t_backdoor_vm *bvm, int len, char *payload);
void kvmwifi_gone(t_backdoor_vm *bvm);
void kvmwifi_born(t_backdoor_vm *bvm);
