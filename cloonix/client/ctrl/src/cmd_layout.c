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
/*---------------------------------------------------------------------------*/
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "layout_rpc.h"
#include "doorways_sock.h"
#include "client_clownix.h"
#include "cmd_help_fn.h"
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_move_on_off(int llid, int tid, int on)
{
  printf("%s %d", __FUNCTION__, on);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_width_height(int llid, int tid, int width, int height)
{
  printf("%s %d %d", __FUNCTION__, width, height);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_zoom(int llid, int tid, int zoom)
{
  printf("%s %d", __FUNCTION__, zoom);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_center_scale(int llid, int tid, int x, int y, int w, int h)
{
  printf("%s %d %d %d %d", __FUNCTION__, x, y, w, h);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_save_params_resp(int llid, int tid, int on,
                                  int width, int height,
                                  int cx, int cy, int cw, int ch)
{
  printf("\n\nON=%d WIDTH=%d HEIGHT=%d X=%d Y=%d W=%d H=%d\n\n", 
         on, width, height, cx, cy, cw, ch);
  exit(0);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_lan(int llid, int tid, t_layout_lan *layout_lan)
{
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_sat(int llid, int tid, t_layout_sat *layout_sat)
{
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_node(int llid, int tid, t_layout_node *layout_node)
{
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_event_sub(int llid, int tid, int on)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_layout_save_params_req(int llid, int tid)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void callback_end(int tid, int status, char *err)
{
  if (tid != 42)
    KOUT(" ");
  printf("\n%s\n", err);
  if (!status)
    exit (0);
  else
    exit (-1);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void callback_end_wait_resp(int tid, int status, char *err)
{
  if (tid)
    KOUT(" ");
  printf("\n%s\n", err);
  if (status)
    exit (-1);
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
int cmd_graph_move_yes(int argc, char **argv)
{
  int tid, llid;
  init_connection_to_uml_cloonix_switch();
  llid = get_clownix_main_llid();
  tid = set_response_callback(callback_end, 42);
  send_layout_move_on_off(llid, tid, 1);
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_graph_move_no(int argc, char **argv)
{
  int tid, llid;
  init_connection_to_uml_cloonix_switch();
  llid = get_clownix_main_llid();
  tid = set_response_callback(callback_end, 42);
  send_layout_move_on_off(llid, tid, 0);
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void help_vm_hide(char *line)
{
  printf("\n\n\n%s <name> <0 or 1>\n", line);
  printf("\n0 to see 1 to hide\n\n\n");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_vm_hide(int argc, char **argv)
{
  int tid, val, result = -1;
  if (argc == 2)
    {
    if (sscanf(argv[1], "%d", &val) == 1)
      {
      init_connection_to_uml_cloonix_switch();
      tid = set_response_callback(callback_end, 42);
      send_layout_modif(get_clownix_main_llid(), tid, 
                        layout_modif_type_vm_hide,
                        argv[0], 0, val, 0);
      result = 0;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void help_eth_hide(char *line)
{
  printf("\n\n\n%s <name> <num> <0 or 1>\n", line);
  printf("\n0 to see 1 to hide\n\n\n");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_eth_hide(int argc, char **argv)
{
  int tid, val, num, result = -1;
  if (argc == 3)
    {
    if ((sscanf(argv[1], "%d", &num) == 1) &&
        (sscanf(argv[2], "%d", &val) == 1))
      {
      init_connection_to_uml_cloonix_switch();
      tid = set_response_callback(callback_end, 42);
      send_layout_modif(get_clownix_main_llid(), tid,
                        layout_modif_type_eth_hide,
                        argv[0], num, val, 0);
      result = 0;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void help_sat_hide(char *line)
{
  printf("\n\n\n%s <sat> <0 or 1>\n", line);
  printf("\n0 to see 1 to hide\n\n\n");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_sat_hide(int argc, char **argv)
{
  int tid, val, result = -1;
  if (argc == 2)
    {
    if (sscanf(argv[1], "%d", &val) == 1)
      {
      init_connection_to_uml_cloonix_switch();
      tid = set_response_callback(callback_end, 42);
      send_layout_modif(get_clownix_main_llid(), tid,
                        layout_modif_type_sat_hide,
                        argv[0], 0, val, 0);
      result = 0;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void help_lan_hide(char *line)
{
  printf("\n\n\n%s <name> <0 or 1>\n", line);
  printf("\n0 to see 1 to hide\n\n\n");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_lan_hide(int argc, char **argv)
{
  int tid, val, result = -1;
  if (argc == 2)
    {
    if (sscanf(argv[1], "%d", &val) == 1)
      {
      init_connection_to_uml_cloonix_switch();
      tid = set_response_callback(callback_end, 42);
      send_layout_modif(get_clownix_main_llid(), tid,
                        layout_modif_type_lan_hide,
                        argv[0], 0, val, 0);
      result = 0;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/




/*****************************************************************************/
void help_vm_xy(char *line)
{
  printf("\n\n\n%s <name> <dx> <dy>\n", line);
  printf("\n\"%s Cloon1 10 10\" moves Cloon1 diagonaly\n\n\n", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void help_vm_abs_xy(char *line)
{
  printf("\n\n\n%s <name> <x> <y>\n", line);
  printf("\n\"%s Cloon1 100 100\" puts Cloon1 at 100 100\n\n\n", line);
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
static int local_cmd_vm_xy(int argc, char **argv, int layout_modif_type)
{
  int tid, val1, val2, result = -1;
  if (argc == 3)
    {
    if ((sscanf(argv[1], "%d", &val1) == 1) &&
        (sscanf(argv[2], "%d", &val2) == 1))
      {
      init_connection_to_uml_cloonix_switch();
      tid = set_response_callback(callback_end, 42);
      send_layout_modif(get_clownix_main_llid(), tid,
                        layout_modif_type,
                        argv[0], 0, val1, val2);
      result = 0;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_vm_abs_xy(int argc, char **argv)
{
  int result;
  result = local_cmd_vm_xy(argc, argv, layout_modif_type_vm_abs_xy);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_vm_xy(int argc, char **argv)
{
  int result;
  result = local_cmd_vm_xy(argc, argv, layout_modif_type_vm_xy);
  return result;
}
/*---------------------------------------------------------------------------*/




/*****************************************************************************/
void help_eth_abs_xy(char *line)
{
  printf("\n\n\n%s <name> <num> <pos>\n", line);
  printf("\n\"%s Cloon1 1 0\" moves Cloon1 eth1 on top", line);
  printf("\n\"%s Cloon1 1 78\" moves Cloon1 eth1 on the right", line);
  printf("\n\"%s Cloon1 1 157\" moves Cloon1 eth1 on the bottom", line);
  printf("\n\"%s Cloon1 1 236\" moves Cloon1 eth1 on the left", line);
  printf("\nValue from 0 to 313 included\n\n\n");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int local_cmd_eth_xy(int argc, char **argv, int layout_modif_type)
{
  int tid, num, val1, result = -1;
  if (argc == 3)
    {
    if ((sscanf(argv[1], "%d", &num) == 1) &&
        (sscanf(argv[2], "%d", &val1) == 1))
      {
      init_connection_to_uml_cloonix_switch();
      tid = set_response_callback(callback_end, 42);
      send_layout_modif(get_clownix_main_llid(), tid,
                        layout_modif_type,
                        argv[0], num, val1, 0);
      result = 0;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_eth_abs_xy(int argc, char **argv)
{
  int result;
  result = local_cmd_eth_xy(argc, argv, layout_modif_type_eth_abs_xy);
  return result;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void help_sat_xy(char *line)
{
  printf("\n\n\n%s <sat> <dx> <dy>\n", line);
  printf("\n\"%s sat0 10 10\" moves sat0 diagonaly\n\n\n", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void help_sat_abs_xy(char *line)
{
  printf("\n\n\n%s <name> <x> <y>\n", line);
  printf("\n\"%s sat0 100 100\" positions sat0\n\n\n", line);
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
static int local_cmd_sat_xy(int argc, char **argv, int layout_modif_type)
{
  int tid, val1, val2, result = -1;
  if (argc == 3)
    {
    if ((sscanf(argv[1], "%d", &val1) == 1) &&
        (sscanf(argv[2], "%d", &val2) == 1))
      {
      init_connection_to_uml_cloonix_switch();
      tid = set_response_callback(callback_end, 42);
      send_layout_modif(get_clownix_main_llid(), tid,
                        layout_modif_type,
                        argv[0], 0, val1, val2);
      result = 0;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_sat_xy(int argc, char **argv)
{
  int result;
  result = local_cmd_sat_xy(argc, argv, layout_modif_type_sat_xy);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_sat_abs_xy(int argc, char **argv)
{
  int result;
  result = local_cmd_sat_xy(argc, argv, layout_modif_type_sat_abs_xy);
  return result;
} 
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void help_lan_xy(char *line)
{
  printf("\n\n\n%s <name> <dx> <dy>\n", line);
  printf("\n\"%s 0101 10 10\" moves lan 0101 diagonaly\n\n\n", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void help_lan_abs_xy(char *line)
{
  printf("\n\n\n%s <name> <x> <y>\n", line);
  printf("\n\"%s 0101 100 100\" positions lan 0101\n\n\n", line);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static int local_cmd_lan_xy(int argc, char **argv, int layout_modif_type)
{
  int tid, val1, val2, result = -1;
  if (argc == 3)
    {
    if ((sscanf(argv[1], "%d", &val1) == 1) &&
        (sscanf(argv[2], "%d", &val2) == 1))
      {
      init_connection_to_uml_cloonix_switch();
      tid = set_response_callback(callback_end, 42);
      send_layout_modif(get_clownix_main_llid(), tid,
                        layout_modif_type,
                        argv[0], 0, val1, val2);
      result = 0;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
int cmd_lan_abs_xy(int argc, char **argv)
{
  int result;
  result = local_cmd_lan_xy(argc, argv, layout_modif_type_lan_abs_xy);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_lan_xy(int argc, char **argv)
{
  int result;
  result = local_cmd_lan_xy(argc, argv, layout_modif_type_lan_xy);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void help_graph_width_height(char *line)
{
  printf("\n\n\n%s <width> <height>\n", line);
  printf("width min = 400\n");
  printf("height min = 300\n\n\n");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void help_graph_zoom(char *line)
{
  printf("\n\n\n%s <zoom>\n", line);
  printf("+1 zoom in (move closer)\n");
  printf("-1 zoom out\n");
  printf("Max absolute value 5\n\n\n");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void help_graph_center_scale(char *line)
{
  printf("\n\n\n%s <x> <y> <width> <height>\n\n\n", line);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
int cmd_graph_width_height(int argc, char **argv)
{
  int tid, val1, val2, result = -1;
  if (argc == 2)
    {
    if ((sscanf(argv[0], "%d", &val1) == 1) &&
        (sscanf(argv[1], "%d", &val2) == 1))
      {
      init_connection_to_uml_cloonix_switch();
      tid = set_response_callback(callback_end, 42);
      send_layout_width_height(get_clownix_main_llid(), tid, val1, val2);
      result = 0;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_graph_center_scale(int argc, char **argv)
{
  int tid, x, y, w, h, result = -1;
  if (argc == 4)
    {
    if ((sscanf(argv[0], "%d", &x) == 1) &&
        (sscanf(argv[1], "%d", &y) == 1) &&
        (sscanf(argv[2], "%d", &w) == 1) &&
        (sscanf(argv[3], "%d", &h) == 1))
      {
      init_connection_to_uml_cloonix_switch();
      tid = set_response_callback(callback_end, 42);
      send_layout_center_scale(get_clownix_main_llid(), tid, x, y, w, h);
      result = 0;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
int cmd_graph_zoom(int argc, char **argv)
{
  int tid, val1, result = -1;
  if (argc == 1)
    {
    if (sscanf(argv[0], "%d", &val1) == 1) 
      {
      if ((val1 >= -5) && (val1 <= 5))
        {
        init_connection_to_uml_cloonix_switch();
        tid = set_response_callback(callback_end, 42);
        send_layout_zoom(get_clownix_main_llid(), tid, val1);
        result = 0;
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cmd_graph_save_params_req(int argc, char **argv)
{
  int tid, result = -1;
  if (argc == 0)
    {
    init_connection_to_uml_cloonix_switch();
    tid = set_response_callback(callback_end_wait_resp, 42);
    send_layout_save_params_req(get_clownix_main_llid(), tid);
    result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/
    



