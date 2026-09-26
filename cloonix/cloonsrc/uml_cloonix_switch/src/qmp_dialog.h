/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
typedef void (*t_dialog_qmp_resp)(char *name, int llid, int tid, 
                                  char *req, char *resp);
typedef void (*t_end_qmp_conn)(char *name);
int  qmp_dialog_req(char *name, int llid, int tid, char *req,
                    t_dialog_qmp_resp cb);
void qmp_dialog_alloc(char *name, t_end_qmp_conn cb);
void qmp_dialog_free(char *name);
void qmp_dialog_init(void);

