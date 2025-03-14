/*****************************************************************************/
/*    Copyright (C) 2006-2025 clownix@clownix.net License AGPL-3             */
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
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <asm/types.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include "io_clownix.h"


/****************************************************************************/
int sock_header_get_size(void)
{
  return 16;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void sock_header_set_info(char *tx, 
                          int llid, int len, int type, int val,
                          char **ntx)
{
  tx[0] = ((llid & 0xFF00) >> 8) & 0xFF;
  tx[1] = llid & 0xFF;
  tx[2] = ((len & 0xFF00) >> 8) & 0xFF;
  tx[3] = len & 0xFF;
  tx[4] = ((type & 0xFF00) >> 8) & 0xFF;
  tx[5] = type & 0xFF;
  tx[6] = ((val & 0xFF00) >> 8) & 0xFF;
  tx[7] = val & 0xFF;
  tx[8] = 0xDE;
  tx[9] = 0xAD;
  tx[10] = 0xCA;
  tx[11] = 0xFE;
  tx[12] = 0xDE;
  tx[13] = 0xCA;
  tx[14] = 0xBE;
  tx[15] = 0xAF;
  *ntx = &(tx[16]);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int sock_header_get_info(char *rx, 
                          int *llid, int *len, int *type, int *val,
                          char **nrx)
{
  int result = -1;
  *llid = ((rx[0] & 0xFF) << 8) + (rx[1] & 0xFF);
  *len  = ((rx[2] & 0xFF) << 8) + (rx[3] & 0xFF);
  *type = ((rx[4] & 0xFF) << 8) + (rx[5] & 0xFF);
  *val  = ((rx[6] & 0xFF) << 8) + (rx[7] & 0xFF);
  if (((rx[8] & 0xFF) == 0xDE) && 
      ((rx[9] & 0xFF) == 0xAD) && 
      ((rx[10] & 0xFF) == 0xCA) && 
      ((rx[11] & 0xFF) == 0xFE) && 
      ((rx[12] & 0xFF) == 0xDE) && 
      ((rx[13] & 0xFF) == 0xCA) && 
      ((rx[14] & 0xFF) == 0xBE) && 
      ((rx[15] & 0xFF) == 0xAF))
    {
    result = 0;
    }
  else
    {
    *llid = 0;
    *len  = 0;
    *type = 0;
    *val  = 0;
    KERR("%02X %02X %02X %02X %02X %02X %02X %02X",
         (rx[8] & 0xFF), (rx[9] & 0xFF), (rx[10] & 0xFF), (rx[11] & 0xFF),
         (rx[12] & 0xFF), (rx[13] & 0xFF), (rx[14] & 0xFF), (rx[15] & 0xFF));
    }
  *nrx  = &(rx[16]);
  return result;
}
/*--------------------------------------------------------------------------*/



/*****************************************************************************/
static int test_file_is_socket(char *path)
{
  int result = -1;
  struct stat stat_file;
  if (!stat(path, &stat_file))
    {
    if (S_ISSOCK(stat_file.st_mode))
      result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/

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

/*****************************************************************************/
int sock_nonblock_client_unix(char *pname)
{
  int len,  sock, result = -1;
  struct sockaddr_un addr;
  if (!test_file_is_socket(pname))
    {
    sock = socket (AF_UNIX, SOCK_STREAM, 0);
    if (sock <= 0)
      KOUT(" ");
    memset (&addr, 0, sizeof (struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, pname);
    len = sizeof (struct sockaddr_un);
    nonblock(sock);
    if (connect(sock,(struct sockaddr *) &addr, len))
      close(sock);
    else
      result = sock;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

