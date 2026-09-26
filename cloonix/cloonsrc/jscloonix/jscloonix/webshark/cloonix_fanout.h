/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef CLOONIX_FANOUT_H
#define CLOONIX_FANOUT_H

/* Turn this dumpcap into a server: called once from main() BEFORE
   capture_loop_start(). Serves two FIXED names derived from snf_path (-i):
   <snf_path>_pcap_sock (unix socket, one connection per subscriber, the
   switch relays it into the rpc) and <snf_path>_pcap_fifo (read by webshark).
   keep_stdout: the real stdout is dup'ed and kept as one more receiver.
   Returns 0, or -1 with nothing changed. */
int cloonix_fanout_start(const char *snf_path, int keep_stdout);

/* The two fixed names, for anyone else that has to find them. Either buffer  */
/* may be NULL. Returns 0 on success.                                        */
int cloonix_fanout_names(const char *snf_path, char *sock, int sock_len,
                         char *fifo, int fifo_len);

/* One line of counters for the log: receivers, records forwarded, records    */
/* dropped. Static buffer, not reentrant, for reporting only.                 */
char *cloonix_fanout_stats(void);

#endif
