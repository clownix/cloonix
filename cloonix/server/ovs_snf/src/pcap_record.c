/*****************************************************************************/
/*    Copyright (C) 2006-2023 clownix@clownix.net License AGPL-3             */
/*                                                                           */
/*  This program is free software: you can redistribute it and/or modify     */
/*  it under the terms of the GNU Affero General Public License as           */
/*  published by the Free Software Foundation, either version 3 of the       */
/*  License, or (at your option) any later version.                          */
/*                                                                           */
/*  This program is distributed in the hope that it will be useful,          */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of           */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            */
/*  GNU Affero General Public License for more details.a                     */
/*                                                                           */
/*  You should have received a copy of the GNU Affero General Public License */
/*  along with this program.  If not, see <http://www.gnu.org/licenses/>.    */
/*                                                                           */
/*****************************************************************************/
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

/*****************************************************************************/
static void timer_pcap_record_start_phase1(void *data)
{
  if (mkfifo(g_pcap_file, 0666) == -1)
    {
    KERR("ERROR %s", strerror(errno));
    g_working_ok = 0;
    }
  else
    {
    inotify_trigger_init(g_pcap_file);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void pcap_record_start_phase1(void)
{
  clownix_timeout_add(50, timer_pcap_record_start_phase1, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
void pcap_record_unlink(void)
{
  if (g_fd != -1)
    close(g_fd);
  unlink(g_pcap_file);
  g_fd = -1;
  g_working_ok = 0;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void pcap_record_close_and_reinit(void)
{
  if (g_fd != -1)
    close(g_fd);
  g_fd = -1;
  g_working_ok = 0;
  unlink(g_pcap_file);
  pcap_record_start_phase1();
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
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
      g_working_ok = 0;
      }
    else
      {
      if (write(g_fd, (void *) buf, len) != len)
        {
        KERR("ERROR SNIFFER: %s", strerror(errno));
        g_working_ok = 0;
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void pcap_record_start_phase2(void)
{
  pcap_hdr_s hdr_s;
  if (g_fd == -1)
    {
    g_fd = open(g_pcap_file, O_WRONLY);
    if (g_fd < 0)
      KERR("ERROR %s", strerror(errno));
    else
      {
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
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void pcap_record_init(char *pcap_record)
{
  memset(g_pcap_file, 0, MAX_PATH_LEN);
  strncpy(g_pcap_file, pcap_record, MAX_PATH_LEN-1); 
  g_fd = -1;
  unlink(g_pcap_file);
  pcap_record_start_phase1();
}
/*--------------------------------------------------------------------------*/
