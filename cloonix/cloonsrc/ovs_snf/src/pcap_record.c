/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <errno.h>
#include <linux/types.h>
#include <libgen.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "pcap_record.h"
#include "inotify_trigger.h"
#include "eventfull.h"

static int g_fd;
static int g_working_ok;
static char g_pcap_file[MAX_PATH_LEN];

typedef struct pcap_hdr_s
{
  uint32_t magic_number;
  uint16_t version_major;
  uint16_t version_minor;
  uint32_t thiszone;
  uint32_t sigfigs;
  uint32_t snaplen;
  uint32_t network;
} pcap_hdr_s;

typedef struct pcaprec_hdr_s
{
  uint32_t ts_sec;
  uint32_t ts_usec;
  uint32_t incl_len;
  uint32_t orig_len;
} pcaprec_hdr_s; 

void close_pcap_record(void);

void pcap_record_close(void)
{
  pcap_record_close_writer();
  unlink(g_pcap_file);
}

/* The fifo stays: the next reader gets a fresh header on it. */
void pcap_record_close_writer(void)
{
  if (g_fd != -1)
    close(g_fd);
  g_fd = -1;
  g_working_ok = 0;
}

int pcap_record_is_writing(void)
{
  return ((g_fd != -1) && (g_working_ok == 1));
}

void pcap_record_rx_packet(uint64_t usec, int len, uint8_t *buf)
{
  int ln, hlen = sizeof(pcaprec_hdr_s);
  uint64_t tmp;
  pcaprec_hdr_s rec_hdr_s;
  if ((g_working_ok == 1) && (g_fd != -1))
    {
    memset(&rec_hdr_s, 0, sizeof(pcaprec_hdr_s));
    tmp = usec/1000000;
    rec_hdr_s.ts_sec = (int) tmp;
    rec_hdr_s.ts_usec = usec%1000000;
    rec_hdr_s.incl_len = len;
    rec_hdr_s.orig_len = len;
    ln = write(g_fd, (void *)&rec_hdr_s, hlen);
    if (ln != hlen)
      {
      KERR("ERROR SNIFFER: %d %d %s", ln, hlen, strerror(errno));
      close_pcap_record();
      }
    else
      {
      if (write(g_fd, (void *) buf, len) != len)
        {
        KERR("ERROR SNIFFER: %s", strerror(errno));
        close_pcap_record();
        }
      }
    }
}

/* quiet: a probe, no reader yet is the expected answer.                    */
static void open_for_reader(int quiet)
{
  pcap_hdr_s hdr_s;
  if (g_fd == -1)
    {
    /* O_NONBLOCK for the OPEN, and only for the open. */
    g_fd = open(g_pcap_file, O_WRONLY | O_NONBLOCK);
    if (g_fd < 0)
      {
      if (errno == ENXIO)
        {
        if (!quiet)
          KERR("WARNING the reader of %s was gone before this end could be "
               "opened, waiting for the next one", g_pcap_file);
        }
      else
        KERR("ERROR opening %s: %s", g_pcap_file, strerror(errno));
      }
    else
      {
      int fl = fcntl(g_fd, F_GETFL, 0);
      if ((fl == -1) || (fcntl(g_fd, F_SETFL, fl & (~O_NONBLOCK))))
        KERR("ERROR could not put %s back to blocking writes: %s",
             g_pcap_file, strerror(errno));
      memset(&hdr_s, 0, sizeof(pcap_hdr_s));
      hdr_s.magic_number   = 0xa1b2c3d4;
      hdr_s.version_major  = 2;
      hdr_s.version_minor  = 4;
      hdr_s.thiszone       = 0;
      hdr_s.sigfigs        = 0;
      hdr_s.snaplen        = 65535;
      hdr_s.network        = 1;
      if (write(g_fd,(void *)&hdr_s,sizeof(pcap_hdr_s))!= sizeof(pcap_hdr_s))
        KERR("ERROR %s", strerror(errno));
      else
        g_working_ok = 1;
      }
    }
}

void pcap_record_start_phase2(void)
{
  open_for_reader(0);
}

/* A reader may already hold the fifo when the previous one is seen gone. */
int pcap_record_probe_reader(void)
{
  open_for_reader(1);
  return pcap_record_is_writing();
}

static void timer_pcap_record_prepare(void *data)
{
  static int armed;
  struct stat st;
  if ((!armed) && (stat(g_pcap_file, &st) == 0) && (S_ISFIFO(st.st_mode)))
    {
    /* Fifo left by the previous snf: a reader may already hold it, try at once then watch. */
    armed = 1;
    inotify_trigger_init(g_pcap_file);
    open_for_reader(1);
    }
  else if (access(g_pcap_file, R_OK))
    {
    armed = 1;
    if (mkfifo(g_pcap_file, 0666) == -1)
      {
      if (errno == ENOENT)
        {
        static int said;
        if (!said)
          {
          said = 1;
          KERR("WARNING %s: its directory is gone, the net is being taken "
               "down. This snf stops recording", g_pcap_file);
          }
        close_pcap_record();
        return;
        }
      KERR("ERROR mkfifo %s %s", g_pcap_file, strerror(errno));
      close_pcap_record();
      }
    else
      inotify_trigger_init(g_pcap_file);
    }
  clownix_timeout_add(20, timer_pcap_record_prepare, NULL, NULL, NULL);
}

void pcap_record_init(char *pcap_record)
{
  memset(g_pcap_file, 0, MAX_PATH_LEN);
  strncpy(g_pcap_file, pcap_record, MAX_PATH_LEN-1); 
  g_fd = -1;
  clownix_timeout_add(20, timer_pcap_record_prepare, NULL, NULL, NULL);
}
