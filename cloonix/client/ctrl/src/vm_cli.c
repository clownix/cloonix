/*****************************************************************************/
/*    Copyright (C) 2006-2023 clownix@clownix.net License AGPL-3             */
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
#include "file_read_write.h"
#include "cmd_help_fn.h"
/*---------------------------------------------------------------------------*/

void callback_end(int tid, int status, char *err);


/***************************************************************************/
void help_add_vm_kvm(char *line)
{
  printf("\n\n\n %s <name> ram=<mem_qty> cpu=<nb_proc> eth=<eth_description> <rootfs> [options]\n",
  line);
  printf("\n\tram is the mem_qty in mega");
  printf("\n\tcpu is the number of processors");
  printf("\n\teth_description: d is for dpdk without spy");
  printf("\n\teth_description example:");
  printf("\n\t\t  eth=sdd says eth0 eth1 and eth2 dpdk interfaces eth0 is spyable");
  printf("\n\tMax eth: %d", MAX_ETH_VM);
  printf("\n\t[options]");
  printf("\n\t       --i386 ");
  printf("\n\t       --no_qemu_ga ");
  printf("\n\t       --natplug=<num of eth for nat> ");
  printf("\n\t       --persistent ");
  printf("\n\t       --9p_share=<host_shared_dir_file_path>");
  printf("\n\t       --install_cdrom=<cdrom_file_path>");
  printf("\n\t       --no_reboot");
  printf("\n\t       --with_pxe");
  printf("\n\t       --added_cdrom=<cdrom_file_path>");
  printf("\n\t       --added_disk=<disk_file_path>");
  printf("\n\t       --fullvirt");
  printf("\n\t       --mac_addr=eth%%d:%%02x:%%02x:%%02x:%%02x:%%02x:%%02x");
  printf("\n\t       --balloon");
  printf("\n\t for the no_qemu_ga option, it must be set if you do not have the qemu_guest_agent int your guest.\n");
  printf("\n\t for the natplug= option, if set, the vm will have special cloonix_osh option.\n");
  printf("\n\t for the persistent option, if set, those writes are persistent after shutwown.\n\n");
  printf("\n\nexample:\n\n");

  printf("\n%s vm_name ram=2000 cpu=4 eth=sss bookworm.qcow2\n", line);
  printf("This will give 3 eth that are wireshark spy compatible\n");
  printf("\n%s vm_name ram=2000 cpu=4 eth=ddd bookworm.qcow2\n", line);
  printf("This will give 3 eth based on dpdk without spy\n");
  printf("\n%s vm_name ram=2000 cpu=4 eth=dsd bookworm.qcow2 --persistent\n", line);
  printf("This will give 3 eth, eth0 is dpdk, eth1 is spyable eth2 is dpdk\n");
  printf("\n\n\n");
}
/*-------------------------------------------------------------------------*/

/***************************************************************************/
static int fill_eth_params_from_argv(char *input, int nb_tot_eth,
                                     t_eth_table *eth_tab)
{
  int num, i, result = -1;
  char *ptr;
  char str[MAX_NAME_LEN];
  int mac[MAC_ADDR_LEN];
  memset(str, 0, MAX_NAME_LEN);
  strncpy(str, input, MAX_NAME_LEN-1);
  ptr = strchr(str, ':');
  if (ptr)
    {
    *ptr = 0;
    ptr++;
    if (sscanf(str, "--mac_addr=eth%d", &num) == 1)
      {
      if ((num < 0) || (num > nb_tot_eth))
        printf("\nBad interface number: %s\n", str);
      else if(eth_tab[num].endp_type == 0)
        printf("\nInterface number unused: %s\n", str);
      else
        {
        if (sscanf(ptr, "%02x:%02x:%02x:%02x:%02x:%02x",
            &(mac[0]), &(mac[1]), &(mac[2]), 
            &(mac[3]), &(mac[4]), &(mac[5])) == MAC_ADDR_LEN) 
          {
          for (i=0; i<MAC_ADDR_LEN; i++)
            eth_tab[num].mac_addr[i] = mac[i] & 0xFF;
          result = 0;
          }
        else
          printf("\nBad scan of %s", ptr);
        }
      }
    else
      printf("\nBad scan of %s", str);
    }
  return result;
}
/*-------------------------------------------------------------------------*/

/***************************************************************************/
static int local_add_kvm(char *name, int mem, int cpu, int nb_tot_eth,
                         t_eth_table *eth_tab, char *rootfs,
                         int argc, char **argv)
{
  int i, result = 0, prop_flags = 0;
  char *img_linux = NULL;
  char *install_cdrom=NULL;
  char *added_cdrom=NULL;
  char *added_disk=NULL;
  char *ptr;
  char *endptr;
  int nat_plug = 0;

  for (i=0; i<argc; i++)
    {
    if (!strcmp(argv[i], "--i386"))
      prop_flags |= VM_CONFIG_FLAG_I386;
    else if (!strcmp(argv[i], "--persistent"))
      prop_flags |= VM_CONFIG_FLAG_PERSISTENT;
    else if (!strcmp(argv[i], "--fullvirt"))
      prop_flags |= VM_CONFIG_FLAG_FULL_VIRT;
    else if (!strcmp(argv[i], "--balloon"))
      prop_flags |= VM_CONFIG_FLAG_BALLOONING;
    else if (!strncmp(argv[i], "--install_cdrom=", strlen("--install_cdrom=")))
      {
      prop_flags |= VM_CONFIG_FLAG_INSTALL_CDROM;
      install_cdrom = argv[i] + strlen("--install_cdrom=");
      }
    else if (!strncmp(argv[i], "--with_pxe", strlen("--with_pxe")))
      {
      prop_flags |= VM_CONFIG_FLAG_WITH_PXE;
      install_cdrom = argv[i] + strlen("--with_pxe");
      }
    else if (!strncmp(argv[i], "--no_reboot", strlen("--no_reboot")))
      {
      prop_flags |= VM_CONFIG_FLAG_NO_REBOOT;
      install_cdrom = argv[i] + strlen("--no_reboot");
      }
    else if (!strncmp(argv[i], "--added_cdrom=", strlen("--added_cdrom=")))
      {
      prop_flags |= VM_CONFIG_FLAG_ADDED_CDROM;
      added_cdrom = argv[i] + strlen("--added_cdrom=");
      }
    else if (!strncmp(argv[i], "--added_disk=", strlen("--added_disk=")))
      {
      prop_flags |= VM_CONFIG_FLAG_ADDED_DISK;
      added_disk = argv[i] + strlen("--added_disk=");
      }
    else if (!strcmp(argv[i], "--no_qemu_ga"))
      {
      prop_flags |= VM_CONFIG_FLAG_NO_QEMU_GA;
      }
    else if (!strncmp(argv[i], "--natplug=", strlen("--natplug=")))
      {
      ptr = argv[i] + strlen("--natplug=");
      nat_plug = (int) strtol(ptr, &endptr, 10);
      if ((endptr[0] != 0) || ( nat_plug < 0 ) || ( nat_plug >= nb_tot_eth))
        {
        printf("\nERROR natplug: %d\n\n", nat_plug);
        nat_plug = 0;
        }
      else
        prop_flags |= VM_CONFIG_FLAG_NATPLUG;
      }
    else if (!strncmp(argv[i], "--mac_addr=eth", strlen("--mac_addr=eth")))
      {
      if (fill_eth_params_from_argv(argv[i], nb_tot_eth, eth_tab))
        {
        printf("\nERROR: %s\n\n", argv[i]);
        result = -1;
        break;
        }
      }
    else
      {
      printf("\nERROR: %s not an option\n\n", argv[i]);
      result = -1;
      break;
      }
    }
  if (result == 0)
    {
    init_connection_to_uml_cloonix_switch();
    client_add_vm(0, callback_end, name, nb_tot_eth, eth_tab, prop_flags,
                  nat_plug, cpu, mem, img_linux, rootfs, install_cdrom,
                  added_cdrom, added_disk);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/***************************************************************************/
static int check_eth_desc(char *eth_desc, char *err,
                          int *nb_tot_eth,
                          t_eth_table *eth_tab)

{
  int i, dpdk=0, result = 0;
  int len, max = MAX_ETH_VM;
  memset(eth_tab, 0, max * sizeof(t_eth_table)); 
  len = strlen(eth_desc);
  if (len >= max)
    {
    sprintf(err, "Too long string for eth description");
    result = -1;
    }
  else
    {
    for (i=0; (result == 0) && (i < len); i++)
      {
      if (eth_desc[i] == 's')
        eth_tab[i].endp_type = endp_type_eths;
      else if (eth_desc[i] == 'v')
        eth_tab[i].endp_type = endp_type_ethv;
      else
        {
        sprintf(err, "Found bad char: %c in string", eth_desc[i]);
        result = -1;
        }
      }
    }
  if (result == 0)
    {
    for (i=0; i < max; i++)
      {
      if ((eth_tab[i].endp_type == endp_type_eths) ||
          (eth_tab[i].endp_type == endp_type_ethv))
        dpdk += 1;
      }
    if (dpdk > MAX_ETH_VM)
      {
      sprintf(err, "Too many dpdk interfaces, %d max: %d",dpdk,MAX_ETH_VM);
      result = -1;
      }
    }
  *nb_tot_eth = dpdk;
  return result;
}
/*---------------------------------------------------------------------------*/

/***************************************************************************/
int cmd_add_vm_kvm(int argc, char **argv)
{
  int cpu, mem, nb_tot_eth, result = -1;
  char *rootfs, *name;
  char eth_string[MAX_PATH_LEN];
  char err[MAX_PATH_LEN];
  t_eth_table eth_tab[MAX_ETH_VM];
  if (argc < 4) 
    printf("\nNot enough parameters for add kvm\n");

  else if (sscanf(argv[1], "ram=%d", &mem) != 1)
    printf("\nBad ram= parameter %s\n", argv[1]);
  else if ((mem < 1) || (mem > 500000))
    printf("\nBad ram range, must be between 1 and 500000 (in mega)\n");

  else if (sscanf(argv[2], "cpu=%d", &cpu) != 1)
    printf("\nBad cpu= parameter %s\n", argv[2]);
  else if ((cpu < 1) || (cpu > 128))
    printf("\nBad cpu range, must be between 1 and 128\n");

  else if (sscanf(argv[3], "eth=%s", eth_string) != 1)
    printf("\nBad eth= parameter %s\n", argv[3]);
  else if (check_eth_desc(eth_string, err, &nb_tot_eth, eth_tab))
    printf("\nBad eth= parameter %s %s\n", argv[3], err);

  else
    {
    name = argv[0];
    rootfs = argv[4];
    result = local_add_kvm(name, mem, cpu, nb_tot_eth,
                           eth_tab, rootfs, argc-5, &(argv[5])); 
    }
  return result;
}
/*-------------------------------------------------------------------------*/











