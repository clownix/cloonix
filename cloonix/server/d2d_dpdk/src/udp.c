/*****************************************************************************/
/*    Copyright (C) 2006-2020 clownix@clownix.net License AGPL-3             */
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
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>

#include <rte_compat.h>
#include <rte_bus_pci.h>
#include <rte_config.h>
#include <rte_cycles.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_flow.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_meter.h>
#include <rte_pci.h>
#include <rte_version.h>
#include <rte_vhost.h>
#include <rte_arp.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "utils.h"
#include "udp.h"

#define PLEN 70000
#define PHEAD 2 
#define MHNB 2 
#define MHLN 2 
#define MHID 4 
#define MAXUDPLEN (MHID+MHLN+MHNB+PLEN+PHEAD)

struct rte_mempool *get_rte_mempool(void);
static struct sockaddr_in g_loc_addr;
static struct sockaddr_in g_dist_addr;
static int g_llid;
static int g_fd;
static uint8_t g_rx[MAXUDPLEN];
static uint8_t g_tx[MAXUDPLEN];
static int g_traffic_mngt;
void reply_probe_udp(void);
char *get_net_name(void);
char *get_d2d_name(void);

/*****************************************************************************/
static void nonblock(int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
    KOUT(" ");
  flags |= O_NONBLOCK|O_NDELAY;
  if (fcntl(fd, F_SETFL, flags) == -1)
    KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/***************************************************************************/
void udp_tx_sig(int len, uint8_t *buf)
{
  uint32_t ln = sizeof(struct sockaddr_in);
  int len_tx;
  len_tx = sendto(g_fd,buf,len,0,(const struct sockaddr *)&g_dist_addr,ln);
  if (len_tx != len)
    {
    KERR("%d %d %d", len_tx, len, errno);
    }
}
/*--------------------------------------------------------------------------*/

/***************************************************************************/
static uint8_t *udp_tx_traf_header(int tot, int nbp)
{
  g_tx[0] = 0xCA;
  g_tx[1] = 0xFE;
  g_tx[2] = 0xDE;
  g_tx[3] = 0xCA;
  g_tx[4] = (tot & 0xFF00) >> 8;
  g_tx[5] =  tot & 0x00FF;
  g_tx[6] = (nbp & 0xFF00) >> 8;
  g_tx[7] =  nbp & 0x00FF;
  return (&(g_tx[8]));
}
/*--------------------------------------------------------------------------*/

/***************************************************************************/
static uint8_t *udp_tx_traf_add_elem(int len, uint8_t *data, uint8_t *buf)
{
  buf[0] = (len & 0xFF00) >> 8;
  buf[1] =  len & 0x00FF;
  memcpy(&(buf[2]), data, len);
  return(&(buf[2+len]));
}
/*--------------------------------------------------------------------------*/

/***************************************************************************/
static int udp_tx_traf_send(int tot)
{
  uint32_t ln = sizeof(struct sockaddr_in);
  int len_tx, result = 0;
  len_tx = sendto(g_fd,g_tx,tot,0,(const struct sockaddr *)&g_dist_addr,ln);
  while ((len_tx == -1) && (errno == EAGAIN))
    {
    usleep(1000);
    len_tx = sendto(g_fd,g_tx,tot,0,(const struct sockaddr *)&g_dist_addr,ln);
    }
  if (len_tx != tot) 
    {
    KERR("%d %d %d", len_tx, tot, errno);
    result = -1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int udp_read_bulk(int *nb_packets, uint8_t **start)
{
  int nbp, len, tot_len;
  int result = read(g_fd, g_rx, MAXUDPLEN);
  if (result == 0)
    {
    result = -1;
    }
  else if (result < 0)
    {
    if ((errno != EAGAIN) && (errno != EINTR))
      {
      result = -1;
      }
    else
      result = 0;
    }
  else if (result <= MHID + MHLN + MHNB)
    {
    KERR("%d", result);
    result = -1;
    }
  else if ((g_rx[0] != 0xCA) || (g_rx[1] != 0xFE) || 
           (g_rx[2] != 0xDE) || (g_rx[3] != 0xCA))
    {
    KERR("%02hhx %02hhx %02hhx %02hhx",
         g_rx[0],g_rx[1],g_rx[2],g_rx[3]);
    result = -1;
    }
  else
    {
    tot_len   = (((int) g_rx[4]) << 8) & 0xFF00;
    tot_len  +=  g_rx[5] & 0x00FF; 
    nbp       = (((int) g_rx[6]) << 8) & 0xFF00;
    nbp      +=  g_rx[7] & 0x00FF; 
    len       = (((int) g_rx[8]) << 8) & 0xFF00;
    len      +=  g_rx[9] & 0x00FF;
    if (tot_len != result)
      {
      KERR("%d %d %d %d", result, tot_len, nbp, len);
      result = -1;
      }
    else if (nbp > MAX_PKT_BURST)
      {
      KERR("%d", nbp);
      result = -1;
      }
    else
      {
      *nb_packets = nbp;
      *start = &g_rx[MHID + MHLN + MHNB];
      result = tot_len - (MHID + MHLN + MHNB);
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int process_udp_rx(int nb_packets, int tot_len, uint8_t *pkts,
                          struct rte_mbuf **mbufs)
{
  int i, len, result = 0, len_done = 0;
  uint8_t *ptr = pkts;
  char *data;
  if (rte_pktmbuf_alloc_bulk(get_rte_mempool(), mbufs, nb_packets))
    {
    KERR("NO MEM 1");
    result = -1;
    }
  else
    {
    for (i=0; i < nb_packets; i++)
      {
      len = ((int)(ptr[0] << 8) & 0xFF00);
      len += ptr[1] & 0xFF; 
      if (len_done + len > tot_len)
        {
        KERR("ERROR %d %d %d", len, tot_len, len_done);
        rte_pktmbuf_free_bulk(mbufs, nb_packets);
        result = -1;
        break;
        }
      data = rte_pktmbuf_append(mbufs[i], len);
      if (data == NULL)
        {
        KERR("NO MEM 2 len=%d nb_packets=%d", len, nb_packets);
        rte_pktmbuf_free_bulk(mbufs, nb_packets);
        result = -1;
        break;
        }
      memcpy(data, &(ptr[2]), len);
      checksum_compute(mbufs[i]);
      ptr += len + PHEAD;
      len_done += (len + PHEAD); 
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_cb(void *ptr, int llid, int fd)
{
  int len, result = 0;
  if (g_traffic_mngt == 0)
    {
    len = read(fd, g_rx, PLEN);
    if (len == 0)
      {
      udp_close();
      KERR(" ");
      }
    else if (len < 0)
      {
      if ((errno != EAGAIN) && (errno != EINTR))
        {
        udp_close();
        KERR(" ");
        }
      }
    else
      {
      if (!strcmp((char *) g_rx, "probe"))
        reply_probe_udp();
      result = len;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void err_cb(void *ptr, int llid, int err, int from)
{
  if (g_traffic_mngt == 0)
    {
    udp_close();
    KERR(" ");
    }
}
/*--------------------------------------------------------------------------*/

/***************************************************************************/
void udp_fill_dist_addr(uint32_t ip, uint16_t udp_port)
{
  g_dist_addr.sin_family = AF_INET;
  g_dist_addr.sin_addr.s_addr = htonl(ip);
  g_dist_addr.sin_port = htons(udp_port);
}
/*--------------------------------------------------------------------------*/

/***************************************************************************/
void udp_close(void)
{
  uint32_t len = sizeof(struct sockaddr_in);
KERR("CCCCCCCCCCCCCCLLLLLLLLLLLLLOOOOOOOOOOOOOOOOOOOSSSSSSSSSSSSSEEEEEEEEEEE");
  if (msg_exist_channel(g_llid))
    msg_delete_channel(g_llid);
  if (g_fd != -1) 
    close(g_fd);
  memset(&g_loc_addr,  0, len);
  memset(&g_dist_addr, 0, len);
  g_llid = 0;
  g_fd = -1;
}
/*--------------------------------------------------------------------------*/

/***************************************************************************/
static int udp_init_bind_loc_addr(uint16_t *udp_port)
{
  int i, fd, result = -1, max_try=1000;
  uint16_t port;
  uint32_t len = sizeof(struct sockaddr_in);
  if ((fd = socket(AF_INET, SOCK_DGRAM, 0))< 0)
    KOUT("%d", errno);
  memset(&g_loc_addr,  0, len);
  memset(&g_dist_addr, 0, len);
  g_loc_addr.sin_family = AF_INET;
  g_loc_addr.sin_addr.s_addr = INADDR_ANY;
  for(i=0; i<max_try; i++)
    { 
    port = 51001 + (rand()%4000); 
    g_loc_addr.sin_port = htons(port);
    if (bind(fd, (const struct sockaddr *)&g_loc_addr, len) == 0) 
      {
      nonblock(fd);
      result = 0;
      *udp_port = port;
      g_fd = fd;
      break;
      }
    }
  if (result)
    KERR("No port udp found");
  else
    g_llid = msg_watch_no_erase_fd(fd, rx_cb, err_cb, "udpd2d");
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int sub_udp_tx_burst(int nb, struct rte_mbuf **mbuf, int tot_len)
{
  int i, len, result;
  uint8_t *data, *buf;
  buf = udp_tx_traf_header(tot_len, nb);
  for (i=0; i < nb; i++)
    {
    len = (int) (mbuf[i]->pkt_len);
    data = rte_pktmbuf_mtod(mbuf[i], uint8_t *);
    buf = udp_tx_traf_add_elem(len, data, buf);
    }
  result = udp_tx_traf_send(tot_len);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int udp_get_traffic_mngt(void)
{
  return g_traffic_mngt;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void udp_enter_traffic_mngt(void)
{
  if (msg_exist_channel(g_llid))
    msg_delete_channel(g_llid);
  g_traffic_mngt = 1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int  udp_rx_burst(int *nb, struct rte_mbuf **mbuf)
{
  int nb_packets;
  uint8_t *pkts;
  int result = udp_read_bulk(&nb_packets, &pkts);
  *nb = 0;
  if (result > 0)
    {
    result = process_udp_rx(nb_packets, result, pkts, mbuf);
    if (result != -1)
      *nb = nb_packets;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int udp_tx_burst(int nb, struct rte_mbuf **mbuf)
{
  int result, i, tot_len, len;
  tot_len = MHID + MHLN + MHNB; 
  for (i=0; i < nb; i++)
    {
    tot_len += PHEAD; 
    tot_len += (int) (mbuf[i]->pkt_len); 
    }
  if (tot_len > 65500)
    {
    for (i=0; i < nb; i++)
      {
      len = PHEAD + MHID + MHLN + MHNB; 
      len += (int) (mbuf[i]->pkt_len); 
      result = sub_udp_tx_burst(1, &(mbuf[i]), len);
      if (result != 0)
        {
        KERR("BAD UNIT: %d %d", nb, tot_len);
        break;
        }
      }
    }
  else
    {
    result = sub_udp_tx_burst(nb, mbuf, tot_len);
    }
  for (i=0; i < nb; i++)
    rte_pktmbuf_free(mbuf[i]);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int udp_init(void)
{
  int result = -1;
  uint16_t udp_port;
  g_traffic_mngt = 0;
  g_fd = -1;
  if (udp_init_bind_loc_addr(&udp_port) == 0)
    result = udp_port;
  return result;
}
/*--------------------------------------------------------------------------*/
                                                                                                              
