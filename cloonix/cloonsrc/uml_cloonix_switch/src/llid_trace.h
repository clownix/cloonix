/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
int llid_trace_exists(int llid);
void llid_trace_alloc(int llid, char *name, int vm_id, int guest_fd, int type);
void llid_trace_free(int llid, int is_clone, const char *fct);
void llid_trace_init(void);
void llid_trace_vm_delete(int vm_id);
void llid_trace_free_if_exists(int llid);
void llid_set_event2llid(int llid, int event, int tid);
void llid_unset_event2llid(int llid, int event);
int  llid_get_event2llid(int event, int prev_llid, int *llid, int *tid);

void llid_free_all_llid(void);
void llid_count_beat(void);

