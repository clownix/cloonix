/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
void sched_inc_queue_stored(int id, uint64_t len);
void sched_dec_queue_stored(int id, uint64_t len);
void sched_mngt(int id, uint64_t current_usec, uint64_t delta_usec);
int sched_can_enqueue(int id, uint64_t len, uint8_t *buf);
void sched_cnf(int dir, char *type, int val);
void sched_init(void);

