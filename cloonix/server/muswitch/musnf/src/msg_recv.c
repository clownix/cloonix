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
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include <libgen.h>

#include "ioc.h"
#include "pcap_file.h"

/*****************************************************************************/
static int dir_exists_writable(char *path)
{
  int result = 0;
  char tmp[MAX_PATH_LEN];
  char *pdir;
  memset(tmp, 0, MAX_PATH_LEN);
  strncpy(tmp, path, MAX_PATH_LEN - 1);
  pdir = dirname(tmp);
  if (!access(pdir, W_OK))
    result = 1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void local_process_cmd(t_all_ctx *all_ctx, char *cmd, char *resp)
{
  char *path;
  strcpy(resp, "KO UNKNOWN");
  DOUT((void *)all_ctx, FLAG_HOP_DIAG, "%s %s KO1", __FUNCTION__, cmd);
  if (!strncmp(cmd, "-set_conf", strlen("-set_conf")))
    {
    path = cmd + strlen("-set_conf");
    path = path + strspn(path, " \t\r\n");
    if (path[0] == '/')
      {
      if (dir_exists_writable(path))
        {
        if (!pcap_file_set_path(all_ctx, path))
          {
          snprintf(resp, MAX_PATH_LEN-1, "SET_CONF_OK%s",
                                          pcap_file_get_path(all_ctx));
          }
        else
          {
          strcpy(resp, "SET_CONF_KO");
          DOUT((void *)all_ctx, FLAG_HOP_DIAG, "%s %s KO1", __FUNCTION__, cmd);
          }
        }
      else
        {
        strcpy(resp, "SET_CONF_KO");
        DOUT((void *)all_ctx, FLAG_HOP_DIAG, "%s %s KO2", __FUNCTION__, cmd);
        }
      }
    else
      {
      strcpy(resp, "SET_CONF_KO");
      DOUT((void *)all_ctx, FLAG_HOP_DIAG, "%s %s KO3", __FUNCTION__, cmd);
      }
    }
  else if (!strcmp(cmd, "-get_conf"))
    {
    snprintf(resp, MAX_PATH_LEN-1, "GET_CONF_RESP%s %d", 
                                   pcap_file_get_path(all_ctx),
                                   pcap_file_is_recording(all_ctx));
    }
  else
    {
    DOUT((void *)all_ctx, FLAG_HOP_DIAG, "%s %s UNKNOWN CMD", __FUNCTION__, cmd);
    KERR("%s", cmd);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_config_modif(t_all_ctx *all_ctx)
{
  int is_blkd, cloonix_llid;
  int mutype;
  char resp[MAX_PATH_LEN];
  memset(resp, 0, MAX_PATH_LEN); 
  cloonix_llid = blkd_get_cloonix_llid((void *) all_ctx);
  snprintf(resp, MAX_PATH_LEN-1, "GET_CONF_RESP%s %d",
                                 pcap_file_get_path(all_ctx),
                                 pcap_file_is_recording(all_ctx));
  if (msg_exist_channel(all_ctx, cloonix_llid, &is_blkd, __FUNCTION__))
    {
    mutype = blkd_get_our_mutype((void *) all_ctx);
    rpct_send_cli_resp(all_ctx, cloonix_llid, mutype, 0, 0, resp);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_cli_req(void *ptr, int llid, int tid,
                    int cli_llid, int cli_tid, char *line)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  char resp[MAX_PATH_LEN];
  memset(resp, 0, MAX_PATH_LEN); 
  DOUT((void *)all_ctx, FLAG_HOP_DIAG, "%s", line);
  local_process_cmd(all_ctx, line, resp);
  rpct_send_cli_resp(all_ctx, llid, tid, cli_llid, cli_tid, resp);
}
/*---------------------------------------------------------------------------*/

