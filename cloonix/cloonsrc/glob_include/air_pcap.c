/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>
#include "glob_common.h"
#include "air_pcap.h"

#define LINKTYPE_RADIOTAP 127

static void fifo_make(t_air_pcap *ap)
{
  if ((mkfifo(ap->path, 0666)) && (errno != EEXIST))
    KERR("WARNING air %s: mkfifo %s: %s", ap->name, ap->path,
         strerror(errno));
  else if (chmod(ap->path, 0666))
    KERR("WARNING air %s: chmod %s: %s", ap->name, ap->path,
         strerror(errno));
}

void air_pcap_open(t_air_pcap *ap, char *snf_dir, char *name)
{
  memset(ap, 0, sizeof(t_air_pcap));
  strncpy(ap->name, name, MAX_NAME_LEN-1);
  snprintf(ap->path, MAX_PATH_LEN-1, "%s/%s_0", snf_dir, name);
  ap->fifo = -1;
  fifo_make(ap);
}

void air_pcap_close(t_air_pcap *ap)
{
  if (ap->fifo >= 0)
    close(ap->fifo);
  ap->fifo = -1;
  if ((unlink(ap->path)) && (errno != ENOENT))
    KERR("WARNING air %s: unlink %s: %s", ap->name, ap->path,
         strerror(errno));
}

/* a radiotap header, then the frame; dropped, never waited for */
void air_pcap_write(t_air_pcap *ap, uint8_t *frame, int len, uint32_t freq)
{
  uint8_t rt[16];
  uint32_t rec[4];
  struct iovec iov[3];
  struct timeval tv;
  uint16_t chflags = (freq && (freq < 3000)) ? 0x0080 : 0x0100;
  uint64_t now;
  if (ap->fifo < 0)
    {
    uint32_t hdr[6] = { 0xa1b2c3d4, 2 | (4 << 16), 0, 0, 65535,
                        LINKTYPE_RADIOTAP };
    ap->fifo = open(ap->path, O_WRONLY | O_NONBLOCK);
    /* unlinked when the last radio left the air, made again */
    if ((ap->fifo < 0) && (errno == ENOENT))
      {
      fifo_make(ap);
      ap->fifo = open(ap->path, O_WRONLY | O_NONBLOCK);
      }
    if (ap->fifo < 0)
      {
      /* ENXIO: no dumpcap reads it yet */
      if (errno != ENXIO)
        KERR("WARNING air %s: open %s: %s", ap->name, ap->path,
             strerror(errno));
      return;
      }
    if (write(ap->fifo, hdr, sizeof(hdr)) != sizeof(hdr))
      {
      KERR("WARNING air %s: the pcap header did not go: %s", ap->name,
           strerror(errno));
      close(ap->fifo);
      ap->fifo = -1;
      return;
      }
    }
  if (len + (int) (sizeof(rec) + sizeof(rt)) > PIPE_BUF)
    {
    ap->dropped += 1;
    KERR("WARNING air %s: a frame of %d bytes is too long for the capture, "
         "not written", ap->name, len);
    return;
    }
  /* present: flags(1) channel(3) dbm antenna signal(5) */
  memset(rt, 0, sizeof(rt));
  rt[2] = sizeof(rt);
  rt[4] = (1 << 1) | (1 << 3) | (1 << 5);
  rt[10] = freq & 0xff;
  rt[11] = (freq >> 8) & 0xff;
  rt[12] = chflags & 0xff;
  rt[13] = (chflags >> 8) & 0xff;
  rt[14] = (uint8_t) AIR_SIGNAL;
  gettimeofday(&tv, NULL);
  rec[0] = (uint32_t) tv.tv_sec;
  rec[1] = (uint32_t) tv.tv_usec;
  rec[2] = rec[3] = (uint32_t) (len + sizeof(rt));
  iov[0].iov_base = rec;
  iov[0].iov_len = sizeof(rec);
  iov[1].iov_base = rt;
  iov[1].iov_len = sizeof(rt);
  iov[2].iov_base = frame;
  iov[2].iov_len = len;
  if (writev(ap->fifo, iov, 3) >= 0)
    return;
  if (errno == EAGAIN)
    {
    ap->dropped += 1;
    now = cloonix_get_msec();
    /* one line per ten seconds for a repeating condition */
    if (now - ap->last_say >= 10000)
      {
      ap->last_say = now;
      KERR("WARNING air %s: the dumpcap is not taking frames, %lu dropped "
           "so far", ap->name, ap->dropped);
      }
    }
  else
    {
    /* EPIPE, the reader gone: the next one gets a fresh header */
    if (errno != EPIPE)
      KERR("WARNING air %s: write on the capture: %s", ap->name,
           strerror(errno));
    close(ap->fifo);
    ap->fifo = -1;
    }
}
