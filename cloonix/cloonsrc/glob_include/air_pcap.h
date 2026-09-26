/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* The capture of an air: the fifo <snf_dir>/<air>_0, radiotap pcap, read by
   the cloonix-dumpcap of the switch as the capture "<air> 0". Written by
   suid_power, the medium of every air. */
#ifndef AIR_PCAP_H
#define AIR_PCAP_H
#include <stdint.h>

#define AIR_SIGNAL (-50)

typedef struct t_air_pcap
{
  char name[MAX_NAME_LEN];
  char path[MAX_PATH_LEN];
  int fifo;
  unsigned long dropped;
  uint64_t last_say;
} t_air_pcap;

void air_pcap_open(t_air_pcap *ap, char *snf_dir, char *name);
void air_pcap_close(t_air_pcap *ap);
void air_pcap_write(t_air_pcap *ap, uint8_t *frame, int len, uint32_t freq);

#endif
