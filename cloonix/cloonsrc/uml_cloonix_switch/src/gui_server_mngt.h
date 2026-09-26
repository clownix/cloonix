/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef GUI_SERVER_MNGT_H
#define GUI_SERVER_MNGT_H

void gui_server_init(char *net, char *root_work, int web_port,
                     char *passwd, char *doors_addr);
void gui_server_stop(void);
void gui_server_pid_resp(int llid, int tid, char *name, int num,
                         int toppid, int pid);
void gui_server_llid_closed(int llid, int from_clone);
int  gui_server_get_pid(void);

#endif
