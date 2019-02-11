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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "ioc.h"
#include "config.h"


/*****************************************************************************/
static t_connect_side g_sideA;
static t_connect_side g_sideB;
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void config_fill_resp(char *resp, int max_len)
{
  int len = 0;
  if ((max_len-len) > 20)
    len += sprintf(resp+len, "CONFIG A2B:\n");
  if ((max_len-len) > 20)
    len += sprintf(resp+len, "\tdelay:%d ms\n", g_sideB.conf_delay);
  if ((max_len-len) > 20)
    len += sprintf(resp+len, "\tloss:%d/10000\n", g_sideA.conf_loss);
  if ((max_len-len) > 20)
    len += sprintf(resp+len, "\tqsize:%d bytes\n", g_sideB.conf_qsize);
  if ((max_len-len) > 20)
    len += sprintf(resp+len, "\tbsize:%d bytes\n", g_sideB.conf_bsize);
  if ((max_len-len) > 20)
    len += sprintf(resp+len, "\tbrate:%d bytes/sec\n", g_sideB.conf_brate);

  if ((max_len-len) > 20)
    len += sprintf(resp+len, "CONFIG B2A:\n");
  if ((max_len-len) > 20)
    len += sprintf(resp+len, "\tdelay:%d\n ms", g_sideA.conf_delay);
  if ((max_len-len) > 20)
    len += sprintf(resp+len, "\tloss:%d/10000\n", g_sideB.conf_loss);
  if ((max_len-len) > 20)
    len += sprintf(resp+len, "\tqsize:%d bytes\n", g_sideA.conf_qsize);
  if ((max_len-len) > 20)
    len += sprintf(resp+len, "\tbsize:%d bytes\n", g_sideA.conf_bsize);
  if ((max_len-len) > 20)
    len += sprintf(resp+len, "\tbrate:%d bytes/sec\n", g_sideA.conf_brate);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
t_connect_side *get_sideA(void)
{
  return (&g_sideA);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_connect_side *get_sideB(void)
{
return (&g_sideB);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int config_recv_command(int input_cmd, int input_dir, int val)
{
  int result = 0;
  KERR("%d %d %d", input_cmd, input_dir, val);
  if (input_dir == input_dir_a2b)
    {
    switch (input_cmd)
      {
      case input_cmd_delay:
        g_sideB.conf_delay = val;
        break;
      case input_cmd_loss:
        g_sideA.conf_loss = val;
        break;
      case input_cmd_qsize:
        g_sideB.conf_qsize = val;
        break;
      case input_cmd_bsize:
        g_sideB.conf_bsize = val;
        break;
      case input_cmd_rate:
        g_sideB.conf_brate = val;
        break;
      default:
        KOUT("%d", input_cmd);
      }
    }
  else if (input_dir == input_dir_b2a)
    {
    switch (input_cmd)
      {
      case input_cmd_delay:
        g_sideA.conf_delay = val;
        break;
      case input_cmd_loss:
        g_sideB.conf_loss = val;
        break;
      case input_cmd_qsize:
        g_sideA.conf_qsize = val;
        break;
      case input_cmd_bsize:
        g_sideA.conf_bsize = val;
        break;
      case input_cmd_rate:
        g_sideA.conf_brate = val;
        break;
      default:
        KOUT("%d", input_cmd);
      }
    }
  else
    KOUT("%d", input_dir);
  return result;
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
void config_init(void)
{
  memset(&g_sideA, 0, sizeof(t_connect_side));
  memset(&g_sideB, 0, sizeof(t_connect_side));
  g_sideA.conf_qsize = 10000000;
  g_sideB.conf_qsize = 10000000;
  g_sideA.conf_bsize = 10000000;
  g_sideB.conf_bsize = 10000000;
  g_sideA.conf_brate = 100000000;
  g_sideB.conf_brate = 100000000;

}
/*---------------------------------------------------------------------------*/


