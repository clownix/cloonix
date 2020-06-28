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
/*---------------------------------------------------------------------------*/
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doorways_sock.h"
#include "client_clownix.h"
#include "cloonix_conf_info.h"


/*****************************************************************************/
void help_sav_derived(char *line)
{
  printf( "\n\n\n%s <name> <saving_file>\n", line);
  printf( "\nThe saving_file must not exist.");
  printf( "\nNeeds the backing associated qcow2 to run.\n\n\n");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void help_sav_full(char *line)
{
  printf( "\n\n\n%s <name> <saving_file>\n", line);
  printf( "\nThe saving_file must not exist.");
  printf( "\nSaves full file.\n\n\n");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void help_qreboot_vm(char *line)
{
  printf("\n\n\n%s <name>\n", line);
  printf("\nRequest a reboot to qemu kvm\n\n\n");
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void help_qhalt_vm(char *line)
{
  printf("\n\n\n%s <name>\n", line);
  printf("\nRequest a poweroff to qemu kvm\n\n\n");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void help_reboot_vm(char *line)
{
  printf("\n\n\n%s <name>\n", line);
  printf("\nRequest a reboot to the cloonix agent\n\n\n");
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void help_halt_vm(char *line)
{
  printf("\n\n\n%s <name>\n", line);
  printf("\nRequest a poweroff to the cloonix agent\n\n\n");
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void help_del_vm(char *line)
{
  printf("\n\n\n%s <name>\n\n\n", line);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void help_add_sat(char *line)
{
  printf("\n\n\n%s <name>\n\n", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void help_add_wif(char *line)
{
  printf("\n\n\n%s <name>\n\n", line);
  printf("\n%s wlan0\n\n", line);
  printf("  Specifique to have a raw socket to a wlan\n");
  printf("  real hardware host interface, the name of the\n");
  printf("  real interface must be given. \n");
  printf("  Swaps mac address to show the real wlan mac to the outside\n");
  printf("  Do not use wif, hack for personal use.\n\n\n");
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void help_del_sat(char *line)
{
  printf("\n\n\n%s <sat>\n\n\n", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void help_add_c2c(char *line)
{
  printf("\n\n\n%s <name> <distant_name>",line);
  printf("\n\tDistant cloonix server name:");
  printf("\n%s\n", cloonix_conf_info_get_names());
  printf("\n\nexemple:\n");
  printf("\t%s to_mito mito\n\n\n", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void help_add_d2d(char *line)
{
  printf("\n\n\n%s <name> <distant_name>",line);
  printf("\n\tDistant cloonix server name:");
  printf("\n%s\n", cloonix_conf_info_get_names());
  printf("\n\nexemple:\n");
  printf("\t%s to_mito mito\n\n\n", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void help_add_vl2sat(char *line)
{
  printf("\n\n\n%s <name> <num> <lan>\n\n\n", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void help_del_vl2sat(char *line)
{
  printf("\n\n\n%s <name> <num> <lan>\n\n\n", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void help_snf_on(char *line)
{
  printf("\n\n\n%s <name>\n\n\n", line);
}
void help_snf_off(char *line)
{
  printf("\n\n\n%s <name>\n\n\n", line);
}
void help_snf_get_file(char *line)
{
  printf("\n\n\n%s <name>\n\n\n", line);
}
void help_snf_set_file(char *line)
{
  printf("\n\n\n%s <name> <pcap file path>\n\n\n", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void help_mud_lan(char *line)
{
  printf("\n\n\n%s <name> \"txt of msg\"\n\n\n", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void help_mud_sat(char *line)
{
  printf("\n\n\n%s <name> \"txt of msg\"", line);
  printf("\n%s nat \"whatip Cloon1\"\n\n\n", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void help_mud_eth(char *line)
{
  printf("\n\n\n%s Cloon1 0 \"set_link 0\"", line);
  printf("\n%s Cloon1 0 \"set_promisc 0\"\n", line);
  printf("\n%s Cloon1 0 \"set_shaping 100\"", line);
  printf("\n (value for set_shaping is in kilo bytes per sec)");
  printf("\n\n\n");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void help_sub_sysinfo(char *line)
{
  printf("\n\n\n%s <name>\n\n\n", line);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void help_sub_endp(char *line)
{
  printf("\n\n\n%s <name> <num>\n\n\n", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void help_sav(char *line)
{
  printf("\n\n\n%s <directory path>\n\n\n", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void help_event_hop(char *line)
{
  printf("\n\n%s doors\n", line);
  printf("\n     or\n");
  printf("\n%s eth <name> <num>\n", line);
  printf("\n     or\n");
  printf("\n%s <hop_type> <name>\n", line);
  printf("for hop_type = lan, sat\n\n\n");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void help_a2b_config(char *line)
{
  printf("\n\n%s <name> <cmd> <dir> <val>\n", line);
  printf("\nname is the a2b name.");
  printf("\ncmd =  \"loss\" \"delay\" \"qsize\" \"bsize\" \"brate\"");
  printf("\ndir =  \"A2B\" \"B2A\"");
  printf("\nval is an integer.\n");
  printf("\nloss: 0 = no loss 10000 = all lost.\n");
  printf("\ndelay: milli-sec.\n");
  printf("\nqsize : octets that can be put in waiting queue.\n");
  printf("\nbsize : max octets to fill the bucket.\n");
  printf("\nbrate : octets per second throughput.\n");
  printf("\n\n");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void help_a2b_dump(char *line)
{
  printf("\n\n%s <name>\n", line);
  printf("\nname is the a2b name.");
  printf("\n\n");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void help_hwsim_config(char *line)
{
  printf("\n\n%s <name> <num> <txt>\n", line);
  printf("\n\n");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void help_hwsim_dump(char *line)
{
  printf("\n\n%s <name> <num>\n", line);
  printf("\n\n");
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void help_sub_qmp(char *line)
{
  printf("\n\n%s <name>\n", line);
  printf("\n\n%s\n", line);
  printf("\n\n");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void help_snd_qmp(char *l)
{
  printf("\n%s Cloon1 \"{ \\\"execute\\\": \\\"system_reset\\\" }\"",l);
  printf("\n%s Cloon1 \"{ \\\"execute\\\": \\\"query-status\\\" }\"",l);
  printf("\n%s Cloon1 \"{ \\\"execute\\\": \\\"query-commands\\\" }\"",l);
  printf("\n%s Cloon1 \"{ \\\"execute\\\": \\\"query-block\\\" }\"",l);
  printf("\n%s Cloon1 \"{ \\\"execute\\\": \\\"query-blockstats\\\" }\"",l);
  printf("\n%s Cloon1 \"{ \\\"execute\\\": \\\"query-cpus\\\" }\"",l);
  printf("\n%s Cloon1 \"{ \\\"execute\\\": \\\"system_powerdown\\\" }\"",l);
  printf("\n%s Cloon1 \"{ \\\"execute\\\": \\\"quit\\\" }\"",l);
  printf("\n...\n\n");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void help_dpdk_ovs_cnf(char *line)
{
  printf("\n\n%s <lcore_mask> <socket_mem> <cpu_mask>", line);
  printf("\n\n%s 0x%%X %%d 0x%%X", line);
  printf("\n%s 0x01 2048 0x0F\n", line);
  printf("\n\n");
}
/*---------------------------------------------------------------------------*/
