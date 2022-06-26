/*****************************************************************************/
/*    Copyright (C) 2006-2022 clownix@clownix.net License AGPL-3             */
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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include "mdl.h"
#include "dialog_thread.h"
#include "wrap.h"

/*****************************************************************************/
typedef struct t_dialog_el
{
  int fd;
  char tx[MAX_TXT_LEN+1];
  int payload;
  int offset;
  t_outflow outflow;
  struct t_dialog_el *next;
} t_dialog_el;
/*--------------------------------------------------------------------------*/
typedef struct t_dialog_rec
{
  int fd;
  int payload;
  int offset;
  int nb_els;
  char rx[MAX_TXT_LEN+1];
  t_outflow outflow;
  t_inflow inflow;
  struct t_dialog_el *first;
  struct t_dialog_el *last;
} t_dialog_rec;
/*--------------------------------------------------------------------------*/
static t_dialog_rec *g_dialog_rec[MAX_FD_NUM];
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void free_first_el(t_dialog_rec *pr)
{
  t_dialog_el *el = pr->first;
  pr->first = el->next;
  if (pr->last == el)
    pr->last = NULL;
  wrap_free(el, __LINE__);
  pr->nb_els -= 1;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int write_first_el(t_dialog_rec *pr)
{
  int result = -1;
  t_dialog_el *el = pr->first;
  ssize_t len;
  if (el)
    {
    len = el->outflow(pr->fd, el->tx+el->offset, el->payload-el->offset);
    if (len < 0)
      {
      if (errno != EINTR && errno != EAGAIN)
        {
        XERR("%d %s", pr->fd, strerror(errno));
        result = -2;
        }
      }
    else if (len == 0)
      {
      XERR("WRITE CLOSED DIALOG %d", pr->fd);
      result = -2;
      }
    else
      {
      el->offset += len;
      if (el->offset == el->payload)
        result = 0;
      else
        XERR("DIALOG WRITE %d %d %d", pr->fd, len, el->payload-el->offset);
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_dialog_el *alloc_el(int fd, t_outflow outflow, 
                           int len, char *buf,
                           char head_len_byte)
{
  t_dialog_el *el = (t_dialog_el *) wrap_malloc(sizeof(t_dialog_el));
  memset(el, 0, sizeof(t_dialog_el));
  el->tx[0] = head_len_byte;
  memcpy(&(el->tx[1]), buf, len);
  el->fd        = fd;
  el->payload   = len+1;
  el->offset    = 0;
  el->outflow   = outflow;
  return el;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void chain_el(t_dialog_rec *pr, t_dialog_el *el)
{
  if (pr->first)
    {
    if (!pr->last)
      XOUT(" ");
    pr->last->next = el;
    pr->last = el;
    }
  else
    {
    if (pr->last)
      XOUT(" ");
    pr->first = el;
    pr->last = el;
    }
  pr->nb_els += 1;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void chain_msg(t_dialog_rec *pr, char *msg)
{
  int len, len_msg;
  char bufln;
  t_dialog_el *el;
  len_msg = strlen(msg) + 1;
  if ((len_msg <= 0) || (len_msg >= 0xFF) ||  (len_msg >= MAX_TXT_LEN))
    XOUT("%d", len_msg);
  bufln = (char) len_msg;
  el = alloc_el(pr->fd, pr->outflow, len_msg, msg, bufln); 
  chain_el(pr, el);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int send_any(int fd, char *buf, int sock_fd, int srv_idx, int cli_idx)
{
  int result = -1;
  t_dialog_rec *pr;
  if ((fd < 0) || (fd >= MAX_FD_NUM))
    XOUT("DIALOG FD OUT OF RANGE %d", fd);
  if (!g_dialog_rec[fd])
    XERR("DIALOG FD DOES NOT EXISTS %d", fd);
  else 
    {
    pr = g_dialog_rec[fd];
    chain_msg(pr, buf);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
int dialog_send_wake(int fd, int sock_fd, int srv_idx, int cli_idx)
{
  char buf[MAX_TXT_LEN];
  memset(buf, 0, MAX_TXT_LEN);
  snprintf(buf, MAX_TXT_LEN-1, DIALOG_WAKE, sock_fd, srv_idx, cli_idx);
  return (send_any(fd, buf, sock_fd, srv_idx, cli_idx));
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int dialog_send_killed(int fd, int sock_fd, int srv_idx, int cli_idx)
{
  char buf[MAX_TXT_LEN];
  memset(buf, 0, MAX_TXT_LEN);
  snprintf(buf, MAX_TXT_LEN-1, DIALOG_KILLED, sock_fd, srv_idx, cli_idx);
  return (send_any(fd, buf, sock_fd, srv_idx, cli_idx));
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int dialog_send_stats(int fd, int sock_fd, int srv_idx, int cli_idx,
                      int txpkts, long long txbytes,
                      int rxpkts, long long rxbytes)
{
  char buf[MAX_TXT_LEN];
  memset(buf, 0, MAX_TXT_LEN);
  snprintf(buf, MAX_TXT_LEN-1, DIALOG_STATS, sock_fd, srv_idx, cli_idx,
                               txpkts, txbytes, rxpkts, rxbytes);
  return (send_any(fd, buf, sock_fd, srv_idx, cli_idx));
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void dialog_recv_buf(char *buf, int sock_fd, int srv_idx, int cli_idx, 
                   t_err err, t_wake wake, t_killed killed, t_stats stats)
{
  int sock, srv, cli, txpkts, rxpkts;
  long long txbytes, rxbytes;
  if (sscanf(buf, DIALOG_WAKE, &sock, &srv, &cli) == 3)
    {
    if ((srv_idx != srv) || (cli_idx != cli))
      {
      XERR("%d %d %d  %d %d", sock_fd, srv_idx, cli_idx, srv, cli);
      err(sock_fd, srv_idx, cli_idx, buf);
      }
    else
      {
      wake(sock_fd, srv_idx, cli_idx, buf);
      }
    }
  else if (sscanf(buf, DIALOG_KILLED, &sock, &srv, &cli) == 3)
    {
    if ((srv_idx != srv) || (cli_idx != cli))
      {
      XERR("%d %d %d  %d %d", sock_fd, srv_idx, cli_idx, srv, cli);
      err(sock_fd, srv_idx, cli_idx, buf);
      }
    else
      {
      killed(sock_fd, srv_idx, cli_idx, buf);
      }
    }
  else if (sscanf(buf, DIALOG_STATS, &sock, &srv, &cli,
                       &txpkts, &txbytes, &rxpkts, &rxbytes) == 7)
    {
    if ((srv_idx != srv) || (cli_idx != cli))
      {
      XERR("%d %d %d  %d %d", sock_fd, srv_idx, cli_idx, srv, cli);
      err(sock_fd, srv_idx, cli_idx, buf);
      }
    else
      {
      stats(sock_fd,srv_idx,cli_idx,txpkts,txbytes,rxpkts,rxbytes,buf);
      }
    }
  else
    {
    err(sock_fd, srv_idx, cli_idx, buf);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int dialog_recv_fd(int fd, int sock_fd, int srv_idx, int cli_idx,
                 t_err err, t_wake wake, t_killed killed, t_stats stats)
{
  int len, result = -1;
  size_t ln_expected;
  char bufln;
  t_dialog_rec *pr;
  if ((fd < 0) || (fd >= MAX_FD_NUM))
    XOUT("DIALOG FD OUT OF RANGE %d", fd);
  if (!g_dialog_rec[fd])
    XERR("DIALOG FD DOES NOT EXISTS %d", fd);
  else
    {
    pr = g_dialog_rec[fd];
    if (pr->payload == 0)
      {
      len = pr->inflow(fd, &bufln, 1);
      if (len == 0)
        {
        XERR(" ");
        return -1;
        }
      else if (len < 0 )
        {
        if ((errno != EINTR) && (errno != EAGAIN))
          {
          XERR("%s", strerror(errno));
          return -1;
          }
        else
          return 0;
        }
      else if (len != 1)
        {
        XERR(" ");
        return -1;
        }
      else
        {
        pr->payload = bufln & 0xFF;
        pr->offset = 0; 
        result = 0;
        }
      }
    ln_expected = pr->payload - pr->offset;
    if (ln_expected < 0)
      XOUT("%d %d", pr->payload, pr->offset);
    if ((pr->offset + ln_expected) > MAX_TXT_LEN)
      XOUT("%d %d", pr->payload, pr->offset);
    len = pr->inflow(fd, (void *)(pr->rx + pr->offset), ln_expected);
    if (len == 0)
      {
      XERR(" ");
      return -1;
      }
    else if (len < 0 )
      {
      if ((errno != EINTR) && (errno != EAGAIN))
        {
        XERR("%s", strerror(errno));
        return -1;
        }
      else
        return 0;
      }
    else if (len < ln_expected)
      {
      pr->offset += ln_expected;
      result = 0;
      }
    else if (len == ln_expected)
      {
      dialog_recv_buf(pr->rx,sock_fd,srv_idx,cli_idx,err,wake,killed,stats);
      pr->payload = 0;
      pr->offset = 0; 
      result = 0;
      }
    else
      XOUT("%d %d", len, ln_expected);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dialog_tx_queue_non_empty(int fd)
{
  int result = 0;
  if ((fd < 0) || (fd >= MAX_FD_NUM))
    XOUT("DIALOG FD OUT OF RANGE %d", fd);
  if (!g_dialog_rec[fd])
    XERR("DIALOG FD DOES NOT EXISTS %d", fd);
  else if (g_dialog_rec[fd]->first)
    result = 1;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dialog_tx_ready(int fd)
{
  int result, used;
  t_dialog_rec *pr;
  if ((fd < 0) || (fd >= MAX_FD_NUM))
    XOUT("DIALOG FD OUT OF RANGE %d", fd);
  pr = g_dialog_rec[fd];
  if (!pr)
    XOUT("DIALOG FD DOES NOT EXIST %d", fd);
  if (pr->fd != fd)
    XOUT("DIALOG FD INCOHERENT %d %d", pr->fd, fd);
  while(1)
    {
    result = write_first_el(pr);
    if (result != 0)
      break;
    free_first_el(pr);
    }
  if (result == -2)
    result = -1;
  else
    result = 0;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dialog_open(int fd, t_outflow outflow, t_inflow inflow)
{
  int result = -1;
  t_dialog_rec *pr;
  if ((fd < 0) || (fd >= MAX_FD_NUM))
    XOUT("DIALOG FD OUT OF RANGE %d", fd);
  if (g_dialog_rec[fd])
    XERR("DIALOG FD EXISTS %d", fd);
  else
    {
    pr = (t_dialog_rec *) wrap_malloc(sizeof(t_dialog_rec));
    memset(pr, 0, sizeof(t_dialog_rec));
    pr->fd = fd;
    pr->outflow = outflow;
    pr->inflow = inflow;
    g_dialog_rec[fd] = pr;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dialog_close(int fd)
{
  t_dialog_rec *pr;
  if ((fd < 0) || (fd >= MAX_FD_NUM))
    XOUT("DIALOG FD OUT OF RANGE %d", fd);
  if (!g_dialog_rec[fd])
    XERR("DIALOG FD DOES NOT EXISTS %d", fd);
  else
    {
    pr = g_dialog_rec[fd];
    g_dialog_rec[fd] = NULL;
    if (pr->fd != fd)
      XOUT("DIALOG FD INCOHERENT %d %d", pr->fd, fd);
    if (pr->first)
      XERR("CLOSE PURGE LEFT DATA %d  %d el", fd, pr->nb_els);
    while(pr->first)
      free_first_el(pr);
    wrap_free(pr, __LINE__);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dialog_init(void)
{
  memset(g_dialog_rec, 0, MAX_FD_NUM * sizeof(t_dialog_rec *));
}
/*--------------------------------------------------------------------------*/

