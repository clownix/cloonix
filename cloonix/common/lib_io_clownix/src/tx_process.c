/*****************************************************************************/
/*    Copyright (C) 2006-2019 cloonix@cloonix.net License AGPL-3             */
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
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>


#include "io_clownix.h"
#include "channel.h"
#include "util_sock.h"
#include "msg_layer.h"
#include "chunk.h"



/*****************************************************************************/
static int tx_write(char *msg, int len, int fd)
{
  int tx_len = 0;
  tx_len = write(fd, (unsigned char *) msg, len);
  if (tx_len < 0)
    {
    if ((errno == EAGAIN) || (errno == EINTR))
      tx_len = 0;
    }
  return tx_len;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
int tx_try_send_chunk(t_data_channel *dchan, int cidx, int *correct_send,
                      t_fd_error err_cb)
{
  int len, fstr_len, result;
  char *fstr;
  fstr_len = dchan->tx->len - dchan->tx->len_done;
  fstr = dchan->tx->chunk + dchan->tx->len_done;
  len = tx_write(fstr, fstr_len, get_fd_with_cidx(cidx));
  *correct_send = 0;
  if (len > 0)
    {
    *correct_send = len;
    if (dchan->tot_txq_size < (unsigned int) len)
      KOUT("%lu %d", dchan->tot_txq_size, len);
    dchan->tot_txq_size -= len;
    if (len == fstr_len)
      {
      first_elem_delete(&(dchan->tx), &(dchan->last_tx));
      if (dchan->tx)
        result = 1;
      else
        result = 0;
      }
    else
      {
      dchan->tx->len_done += len;
      result = 2;
      }
    }
  else if (len == 0)
    {
    result = 0;
    }
  else if (len < 0)
    {
    KERR("%d", errno);
    dchan->tot_txq_size = 0;
    chain_delete(&(dchan->tx), &(dchan->last_tx));
    if (!err_cb)
      KOUT(" ");
    err_cb(NULL, dchan->llid, errno, 3);
    result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/




