/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */

/* Launch passt for one nat on socket_path; the child pid, or -1.           */
/*   ports : "tcp:8080:80,udp:5353:53" (passt -t/-u) or "none".             */
int passt_spawn_start(char *socket_path, char *ports, char *why);

/* Half a second after a start: -1 if passt already exited, else 0.        */
int passt_spawn_probe(char *socket_path);

/* 1 passt alive, 0 passt just exited: the caller tears the nat down.       */
int passt_spawn_beat(void);

void passt_spawn_stop_one(char *socket_path);
/* Stop every one of them: shutdown paths only. */
void passt_spawn_stop(void);
