/*****************************************************************************/
/*    Copyright (C) 2006-2024 clownix@clownix.net License AGPL-3             */
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

#include "mdl.h"
#include "wrap.h"
#include "glob_common.h"


/*****************************************************************************/
static int read_with_wait(int x11_fd, char *buf, int len_to_get)
{
  int len, result = -1;
  int count = 0;
  do
    {
    count += 1;
    if (count == 5)
      break;
    usleep(10000);
    len = wrap_read_x11_rd_x11(x11_fd, buf, len_to_get);
    if (len == 0)
      {
      KERR("%d  X11 READ 0", x11_fd);
      break;
      }
    else if (len < 0)
      {
      KERR("%d  %s", x11_fd, strerror(errno));
      if ((errno != EINTR) && (errno != EAGAIN))
        break;
      }
    else
      result = len;
    } while (len < 0);
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_msg *helper_read_from_x11(uint32_t randid, int x11_fd, 
                                   int len_to_get, int srv_idx, int cli_idx)
{
  int len = g_msg_header_len + MAX_X11_MSG_LEN;
  t_msg *msg = (t_msg *) wrap_malloc(len);
  mdl_set_header_vals(msg, randid, thread_type_x11,
                      fd_type_x11_rd_x11, srv_idx, cli_idx);

  len = read_with_wait(x11_fd, msg->buf, len_to_get);
  if (len == -1)
    {
    wrap_free(msg, __LINE__);
    msg = NULL;
    }
  else
    msg->len = len;
  return msg;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int x11_spoofing_read_12(char *data, int *pl, int *dl)
{
  int result = 0;
  if (data[0] == 0x42)
    {
    *pl = 256 * data[6] + data[7];
    *dl = 256 * data[8] + data[9];
    }
  else if (data[0] == 0x6c)
    {
    *pl = data[6] + 256 * data[7];
    *dl = data[8] + 256 * data[9];
    }
  else
    {
    KERR("END THREAD %02X", data[0] & 0xFF);
    }
  result = ((*pl + 3) & ~3) + ((*dl + 3) & ~3);
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
t_msg *first_read_magic(uint32_t randid, int x11_fd,
                        int srv_idx, int cli_idx, char *magic_cookie)
{
  int i, used, wlen, len, ln_to_read, proto_len, data_len;
  char magic[2*MAGIC_COOKIE_LEN+1];
  char *ptr_cookie;
  t_msg *msg = helper_read_from_x11(randid, x11_fd, 12, srv_idx, cli_idx);
  if (!msg)
    KERR("(%d-%d) %d", srv_idx, cli_idx, x11_fd);
  else if (msg->len != 12)
    {
    KERR("(%d-%d) %d     len: %d", srv_idx, cli_idx, x11_fd, len);
    wrap_free(msg, __LINE__);
    msg = NULL;
    }
  else
    {
    ln_to_read = x11_spoofing_read_12(msg->buf, &proto_len, &data_len);
    len = read_with_wait(x11_fd, msg->buf + 12, ln_to_read);
    if (len != ln_to_read)
      {
      KERR("(%d-%d) %d  %d %d", srv_idx, cli_idx, x11_fd, len, ln_to_read);
      wrap_free(msg, __LINE__);
      msg = NULL;
      }
    else
      {
      msg->len += ln_to_read;
      if ((proto_len != strlen(MAGIC_COOKIE)) ||
          (memcmp(msg->buf + 12, MAGIC_COOKIE, proto_len)) != 0)
        {
        KERR("(%d-%d) %d %s", srv_idx, cli_idx, x11_fd, msg->buf + 12);
        wrap_free(msg, __LINE__);
        msg = NULL;
        }
      else if (data_len != MAGIC_COOKIE_LEN)
        {
        KERR("(%d-%d) %d %d", srv_idx, cli_idx, x11_fd, data_len);
        wrap_free(msg, __LINE__);
        msg = NULL;
        }
      else
        {
        msg->len = 12 + ln_to_read;
        ptr_cookie = msg->buf + 12 + ((proto_len + 3) & ~3);
        for (i=0; i < MAGIC_COOKIE_LEN; i++)
          snprintf(&(magic[2*i]), 3, "%02x", ptr_cookie[i] & 0xFF);
        magic[2*MAGIC_COOKIE_LEN] = 0;
        if (strcmp(magic, magic_cookie))
          KERR("%s DIFFER: expected: %s\n received: %s\n",
               MAGIC_COOKIE, magic, magic_cookie);
        }
      }
    }
  return msg;
}
/*--------------------------------------------------------------------------*/


