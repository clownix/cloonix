/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
enum
{
backdoor_evt_none = 0,
backdoor_evt_connected,
backdoor_evt_disconnected,
backdoor_evt_ping_ok,
backdoor_evt_ping_ko,
};

void qga_event_backdoor(char *name, int backdoor_evt);
void qga_dialog_init(void);

