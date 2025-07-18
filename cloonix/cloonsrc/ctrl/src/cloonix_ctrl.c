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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
/*---------------------------------------------------------------------------*/
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doorways_sock.h"
#include "client_clownix.h"
#include "cmd_help_fn.h"
#include "cmd_layout.h"
#include "file_read_write.h"
#include "cloonix_conf_info.h"
/*---------------------------------------------------------------------------*/
#define MAX_CMD_LEN 400
#define MAX_FIELD_LEN 100


static int g_inhibited;
static char g_cloonix_server_sock[MAX_PATH_LEN];
static char g_cloonix_password[MSG_DIGEST_LEN];
static char g_current_directory[MAX_PATH_LEN];



static int g_nb_cloonix_servers;
static t_cloonix_conf_info *g_cloonix_conf_info;
static int g_current_llid;

struct cmd_struct {
        char *cmd;
        char *info;
        struct cmd_struct *sub_level;
        int (*cmd_fn)(int, char **);
        void (*help_fn)(char *);
};


/*---------------------------------------------------------------------------*/
t_cloonix_conf_info *get_own_cloonix_conf_info(void)
{
  return (g_cloonix_conf_info);
}
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
static int handle_command(struct cmd_struct *command, int level, 
                          int argc, char **argv);

/****************************************************************************/
struct cmd_struct level_layout_cmd[] = {
{"stop", "No automatic moves", NULL, cmd_graph_move_no, NULL},
{"go",   "Automatic moves", NULL, cmd_graph_move_yes, NULL},
{"params", "Gives graph params", NULL, cmd_graph_save_params_req, NULL},
{"width_height", "Graph size", NULL, cmd_graph_width_height, 
                                    help_graph_width_height},
{"scale", "Give center and scale to graph", NULL, 
                                    cmd_graph_center_scale, 
                                    help_graph_center_scale},
{"zoom", "Graph zoom in out", NULL, cmd_graph_zoom, help_graph_zoom},
{"hide_kvm", "Hide kvm on graph", NULL, cmd_vm_hide, help_vm_hide},
{"hide_eth", "Hide kvm eth on graph", NULL, cmd_eth_hide, help_eth_hide},
{"hide_sat", "Hide sat on graph",  NULL, cmd_sat_hide,   help_sat_hide},
{"hide_lan", "Hide lan on graph", NULL, cmd_lan_hide,   help_lan_hide},
{"xy_kvm",  "Move kvm on graph", NULL, cmd_vm_xy, help_vm_xy},
{"xy_sat",  "Move tap,nat... on graph",  NULL, cmd_sat_xy,   help_sat_xy},
{"xy_lan",  "Move lan on graph", NULL, cmd_lan_xy,   help_lan_xy},
{"abs_xy_kvm", "Places kvm ", NULL, cmd_vm_abs_xy, help_vm_abs_xy},
{"abs_xy_eth", "Places kvm eth ",NULL,cmd_eth_abs_xy, help_eth_abs_xy},
{"abs_xy_sat", "Places tap,nat... ",  NULL, cmd_sat_abs_xy,   help_sat_abs_xy},
{"abs_xy_lan", "Places lan ", NULL, cmd_lan_abs_xy,  help_lan_abs_xy},
{"help",  "",                            level_layout_cmd, NULL, NULL},
};
/*--------------------------------------------------------------------------*/

/****************************************************************************/
struct cmd_struct level_add_cmd[] = {
{"lan", "Add lan (emulated cable)", NULL, cmd_add_vl2sat, help_add_vl2sat},
{"kvm", "Add kvm (virtualized machine)", NULL,cmd_add_vm_kvm,help_add_vm_kvm},
{"zip", "Add container crun brandzip",  NULL, cmd_add_zip, help_add_zip},
{"cvm", "Add container crun brandcvm",  NULL, cmd_add_cvm, help_add_cvm},
{"tap", "Add tap (host network interface)",  NULL, cmd_add_tap, help_add_tap},
{"phy", "Add phy (real host network interface)",  NULL, cmd_add_phy, help_add_phy},
{"nat", "Add nat (access host ip)",NULL, cmd_add_nat, help_add_nat},
{"a2b", "Add a2b (traffic delay shaping)",NULL, cmd_add_a2b, help_add_a2b},
{"c2c", "Add c2c (cloonix 2 cloonix cable)", NULL, cmd_add_c2c, help_add_c2c},
{"help",  "",                     level_add_cmd, NULL, NULL},
};
/*---------------------------------------------------------------------------*/

/****************************************************************************/
struct cmd_struct level_del_cmd[] = {
{"lan", "Delete lan", NULL, cmd_del_vl2sat, help_del_vl2sat},
{"sat", "Delete non-lan",  NULL, cmd_del_sat, help_del_sat},
{"help",  "",                     level_del_cmd, NULL, NULL},
};
/*---------------------------------------------------------------------------*/

/****************************************************************************/
struct cmd_struct level_cnt_cmd[] = {
{"color", "Color of cnt",  NULL, cmd_color_cnt, help_color_cnt},
{"help",  "",                     level_cnt_cmd, NULL, NULL},
};
/*---------------------------------------------------------------------------*/

/****************************************************************************/
struct cmd_struct level_kvm_cmd[] = {
{"color", "Color of kvm",  NULL, cmd_color_kvm, help_color_kvm},
{"reboot", "Reboot by qemu",  NULL, cmd_qreboot_vm, help_qreboot_vm},
{"help",  "",                     level_kvm_cmd, NULL, NULL},
};
/*---------------------------------------------------------------------------*/

/****************************************************************************/
struct cmd_struct level_cnf_cmd[] = {
{"kvm",  "Kvm config",    level_kvm_cmd, NULL, NULL},
{"cnt",  "Container config",    level_cnt_cmd, NULL, NULL},
{"lan",  "lan config", NULL, cmd_cnf_lan, help_cnf_lan},
{"nat",  "nat config", NULL, cmd_cnf_nat, help_cnf_nat},
{"a2b",  "a2b config", NULL, cmd_cnf_a2b, help_cnf_a2b},
{"c2c",  "c2c config", NULL, cmd_cnf_c2c, help_cnf_c2c},
{"lay",  "Layout modifications on canvas", level_layout_cmd, NULL, NULL},
{"web",  "novnc on",  NULL, cmd_novnc_on_off, help_novnc_on_off},
{"fix",  "set xauthority to distant", NULL, cmd_cnf_fix, NULL},
{"help",  "",                     level_cnf_cmd, NULL, NULL},
};
/*---------------------------------------------------------------------------*/

/****************************************************************************/
struct cmd_struct level_sub_cmd[] = {
{"endp",  "Packet counter ", NULL, cmd_sub_endp, help_sub_endp},
{"sys",  "Sysinfo of guest vm", NULL, cmd_sub_sysinfo, help_sub_sysinfo},
{"help",  "",                     level_sub_cmd, NULL, NULL},
};
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
struct cmd_struct level_main_cmd[] = {
{"snf", "Add wireshark capability on item",NULL, cmd_add_snf, help_add_snf},
{"kil", "Destroys all objects, cleans and kills switch", NULL, cmd_kill, NULL},
{"rma", "Destroys all cloon objects and graphs",  NULL, cmd_delall,    NULL},
{"dcf", "Dump config", NULL,  cmd_config_dump, NULL},
{"dmp", "Dump topo", NULL,  cmd_topo_dump, NULL},
{"lst", "List commands to replay topo", NULL,  cmd_list_commands, NULL},
{"lay", "List topo layout", NULL,  cmd_lay_commands, NULL},
{"add", "Add one cloon object to topo", level_add_cmd, NULL, NULL},
{"del", "Del one cloon object from topo", level_del_cmd, NULL, NULL},
{"sav", "Save in qcow2", NULL, cmd_sav_full, help_sav_full},
{"cnf", "Configure a cloon object", level_cnf_cmd, NULL, NULL},
{"sub", "Subscribe to stats counters", level_sub_cmd, NULL, NULL},
{"hop", "dump 1 hop debug", NULL, cmd_event_hop, help_event_hop},
{"pid", "dump pids of processes", NULL, cmd_pid_dump, NULL},
{"evt", "prints events",         NULL,  cmd_event_print, NULL},
{"sys", "prints system stats",   NULL,  cmd_event_sys, NULL},
{"help",   "",                    level_main_cmd, NULL, NULL},
};
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int cmd_count_nb(struct cmd_struct *cmd)
{
  int nb_cmd=0;
  struct cmd_struct *p;
  p = cmd;
  while (strcmp(p->cmd, "help"))
    {
    nb_cmd++;
    p++;
    }
  return nb_cmd;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static char *good_cmd_line_start(int level, char **argv, int leaf)
{
  int i;
  char fill1[MAX_FIELD_LEN];
  char ver[MAX_NAME_LEN];
  static char cmd_line[MAX_CMD_LEN];
  memset(cmd_line, 0, MAX_CMD_LEN);
  memset(ver, 0, MAX_NAME_LEN);
  snprintf(ver, MAX_NAME_LEN-1, "Version:%s", 
           cloonix_conf_info_get_version());

  if (!leaf)
    {
    strcat(cmd_line, "| cloonix_cli nemo ");
    strcat(cmd_line, ver);
    }
  else
    strcat(cmd_line, "cloonix_cli nemo ");
  if (level > 1)
    {
    for (i=3; i<level; i++)
      {
      strcat(cmd_line, argv[i]);
      strcat(cmd_line, " ");
      }
    }
  if (!leaf)
    {
    memset(fill1, 0, MAX_FIELD_LEN);
    memset(fill1, ' ', 75 - strlen(cmd_line));
    strcat(cmd_line, fill1);
    cmd_line[strlen(cmd_line)-1] = '|';
    }
  return cmd_line;
}
/*---------------------------------------------------------------------------*/

#define SEPARATOR "|-------------------------------------"\
                  "------------------------------------|"
/*****************************************************************************/
static int cmd_help(struct cmd_struct *cmd, char *cmd_line)
{
  char fill1[MAX_FIELD_LEN];
  char fill2[MAX_FIELD_LEN];
  int i, nb_cmd;
  struct cmd_struct *p;
  printf("\n");
  printf(SEPARATOR);
  printf("\n");
  printf("%s", cmd_line);
  printf("\n");
  printf(SEPARATOR);
  printf("\n");
  nb_cmd = cmd_count_nb(cmd);
  p = cmd;
  for (i=0; i<nb_cmd; i++)
    {
    if (!p->cmd)
      KOUT(" ");
    memset(fill1, 0, MAX_FIELD_LEN);
    memset(fill1, ' ', 15 - strlen(p->cmd));
    memset(fill2, 0, MAX_FIELD_LEN);
    memset(fill2, ' ', 49 - strlen(p->info));
    printf("|    %s%s : %s %s |\n", p->cmd, fill1, p->info, fill2);
    p++;
    }
  printf(SEPARATOR);
  printf("\n");
  return -1;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
struct cmd_struct *get_command(struct cmd_struct *command, char *cmd)
{
  struct cmd_struct *p = command;
  while (strcmp(p->cmd, "help")) 
    {
    if (!strcmp(p->cmd, cmd))
      break;
    p++;
    }
  return p;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int handle_command(struct cmd_struct *command, int level, int argc, char **argv)
{
  struct cmd_struct *p;
  int i, result=-1; 
  char **current_argv = &argv[level];

  if (!current_argv || !*current_argv) 
    {
    p = get_command(command, "help");
    cmd_help(p->sub_level, good_cmd_line_start(level, argv, 0));
    }
  else
    {
    p = get_command(command, argv[level]);
    if (!strcmp(p->cmd, "help"))
      cmd_help(p->sub_level, good_cmd_line_start(level, argv, 0));
    else
      {
      if (p->sub_level)
        result = handle_command(p->sub_level, level+1, argc, argv);
      else
        {
        result = p->cmd_fn(argc-level-1, &(argv[level+1]));
        if (result < 0)
          {
          printf("\nERROR:");
          for (i=0; i<argc; i++)
            printf(" %s", argv[i]);  
          }
        if ((result < 0) && (p->help_fn))
          {
          p->help_fn(good_cmd_line_start(level+1, argv, 1));
          }
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timout_connect(void *data)
{
  if (!g_inhibited)
    {
    printf("\nusage:\n\n\tcloonix_cli <reachable server> <args...>\n\n");
    exit(1);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void timout_connect_target(void *data)
{
  if (!g_inhibited)
    {
    printf("\n\n\nTIMEOUT\n");
    exit(1);
    }
}
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
void init_connection_to_uml_cloonix_switch(void)
{
  client_init("ctrl", g_cloonix_server_sock, g_cloonix_password);
  clownix_timeout_add(500, timout_connect_target, NULL, NULL, NULL);
  while (!client_is_connected())
    msg_mngt_loop_once();
  g_inhibited = 1;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void print_reacheability(int idx, char *reach)
{
  int count_spaces; 
  char spaces1[20];
  char spaces2[25];
  memset(spaces1, ' ', 20);
  memset(spaces2, ' ', 25);
  count_spaces = 15 - strlen(g_cloonix_conf_info[idx].name);
  if (count_spaces <= 0)
    count_spaces = 4;
  spaces1[count_spaces] = 0;
  count_spaces = 25 - strlen(g_cloonix_conf_info[idx].doors);
  if (count_spaces <= 0)
    count_spaces = 4;
  spaces2[count_spaces] = 0;

  printf("\t%s%s%s%s%s\n", g_cloonix_conf_info[idx].name, spaces1,
                        g_cloonix_conf_info[idx].doors, spaces2, reach);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void stub_err_cb (int llid)
{
  int i;
  for (i=0; i<g_nb_cloonix_servers; i++)
    {
    if (g_cloonix_conf_info[i].doors_llid == llid)
      {
      print_reacheability(i, "not_reachable");
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void doorways_rx_bufraw(int llid, int tid,
                               int len_bufraw, char *doors_bufraw,
                               int type, int val, int len, char *buf)
{
  if (type == doors_type_switch)
    {
    if (val == doors_val_link_ok)
      {
      }
    else if (val == doors_val_link_ko)
      {
      KOUT(" ");
      }
    else
      {
      g_current_llid = llid;
      if (doors_io_basic_decoder(llid, len, buf))
        {
        if (rpct_decoder(llid, len, buf))
          KOUT("%s", buf);
        }
      }
    }
  else
    KOUT("%d", type);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void stub_topo(int tid, t_topo_info *topo)
{
  if (g_cloonix_conf_info[tid].doors_llid == g_current_llid)
    {
    if (strcmp(topo->clc.network,  g_cloonix_conf_info[tid].name)) 
      KERR("ERROR %s %s", topo->clc.network, g_cloonix_conf_info[tid].name);
    print_reacheability(tid, "reachable");
    g_cloonix_conf_info[tid].doors_llid = 0;
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int callback_connect(int llid, int fd)
{
  int i, doors_llid; 
  for (i=0; i<g_nb_cloonix_servers; i++)
    {
    if (g_cloonix_conf_info[i].connect_llid == llid)
      {
      if (g_cloonix_conf_info[i].doors_llid == 0)
        {
        doors_llid = doorways_sock_client_inet_end(doors_type_switch, llid, fd,
                                          g_cloonix_conf_info[i].passwd,
                                          stub_err_cb, doorways_rx_bufraw);
        if (doors_llid)
          {
          g_cloonix_conf_info[i].doors_llid = doors_llid;
          if (doorways_sig_bufraw(doors_llid, llid, doors_type_switch,
                                  doors_val_init_link, "OK"))
            KERR("WARNING");
          send_event_topo_sub(doors_llid, i);
          }
        break;
        }
      else
        KERR("TWO CONNECTS FOR ONE REQUEST");
      }
    }
  return 0;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void locale_doorways_client_tx(int llid, int len, char *buf)
{
  if (doorways_tx_bufraw(llid, 0, doors_type_switch,
                         doors_val_none, len, buf))
    KERR("WARNING %s", buf);
}
/*---------------------------------------------------------------------------*/

   
/*****************************************************************************/
int main (int argc, char *argv[])
{
  int i, rank, result, running_in_crun;
  char tmpnet[MAX_NAME_LEN];
  char pathcrun[MAX_PATH_LEN];
  uint32_t ip;
  running_in_crun = pthexec_running_in_crun(tmpnet);
  if (argc < 2)
    KOUT("%d", argc);
  g_inhibited = 0;
  if (cloonix_conf_info_init(argv[1]))
    KOUT("%s", argv[1]);

  memset(g_current_directory, 0, MAX_PATH_LEN);
  if (!getcwd(g_current_directory, MAX_PATH_LEN-1))
    KOUT(" ");
  if (argc < 3)
    {
    doorways_sock_init(0);
    msg_mngt_init("ctrl", IO_MAX_BUF_LEN);
    cloonix_conf_info_get_all(&g_nb_cloonix_servers, &g_cloonix_conf_info);
    client_topo_tst_sub(stub_topo);
    printf("\nVersion:%s\n\n", cloonix_conf_info_get_version());
    for (i=0; i<g_nb_cloonix_servers; i++)
      {
      doors_io_basic_xml_init(locale_doorways_client_tx);
      g_cloonix_conf_info[i].doors_llid = 0;
      if (running_in_crun && (!strcmp(tmpnet, g_cloonix_conf_info[i].name)))
        {
        memset(pathcrun, 0, MAX_PATH_LEN);
        snprintf(pathcrun, MAX_PATH_LEN-1,
                 "%s_%s/proxy_pmain.sock", PROXYSHARE_IN, argv[2]);
        doorways_sock_client_unix_start(pathcrun, callback_connect);
        }
      else
        {
        ip = g_cloonix_conf_info[i].ip;
        g_cloonix_conf_info[i].connect_llid = 
        doorways_sock_client_inet_start(ip,
                                        g_cloonix_conf_info[i].port,
                                        callback_connect);
        }
      }
    clownix_timeout_add(400, timout_connect, NULL, NULL, NULL);
    client_loop();
    }
  else
    {
    g_cloonix_conf_info = cloonix_cnf_info_get(argv[2], &rank); 
    if (!g_cloonix_conf_info)
      {
      printf("\nBAD NAME %s:", argv[2]);
      printf("\n\n%s\n\n", cloonix_conf_info_get_names());
      exit(1);
      }
    memset(g_cloonix_server_sock, 0, MAX_PATH_LEN);
    if (running_in_crun && (!strcmp(tmpnet, argv[2])))
      {
      snprintf(g_cloonix_server_sock, MAX_PATH_LEN-1,
               "%s_%s/proxy_pmain.sock", PROXYSHARE_IN, argv[2]);
      }
    else
      {
      strncpy(g_cloonix_server_sock, g_cloonix_conf_info->doors, MAX_PATH_LEN);
      }
    memset(g_cloonix_password, 0, MSG_DIGEST_LEN);
    strncpy(g_cloonix_password, g_cloonix_conf_info->passwd, MSG_DIGEST_LEN);
    result = handle_command(level_main_cmd, 3, argc, &argv[0]);
    if (!result)
      client_loop();
    }
  return result;
}
/*---------------------------------------------------------------------------*/

