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
#include "doorways_sock.h"
#include "client_clownix.h"
#include "file_read_write.h"
#include "cmd_help_fn.h"
/*---------------------------------------------------------------------------*/

void callback_end(int tid, int status, char *err);


/***************************************************************************/
void help_add_vm_kvm(char *line)
{
  printf("\n\n\n %s <name> ram=<mem_qty> cpu=<nb_proc> dpdk=<nb_eth> sock=<nb_eth> hwsim=<nb_wlan> <rootfs> [options]\n",
  line);
  printf("\n\tram is the mem_qty in mega");
  printf("\n\tcpu is the number of processors");
  printf("\n\tdpdk: number of eth provided with dhcp (low latency and high throughput), max:%d", MAX_DPDK_VM);
  printf("\n\tsock: number of eth provided with classic unix socket, max:%d", MAX_ETH_VM);
  printf("\n\thwsim: number of wlan provided with hwsim modules and unix sockets, max:%d\n", MAX_WLAN_VM);
  printf("\n\t[options]");
  printf("\n\t       --vhost-vsock");
  printf("\n\t       --persistent ");
  printf("\n\t       --9p_share=<host_shared_dir_file_path>");
  printf("\n\t       --install_cdrom=<cdrom_file_path>");
  printf("\n\t       --no_reboot");
  printf("\n\t       --added_cdrom=<cdrom_file_path>");
  printf("\n\t       --added_disk=<disk_file_path>");
  printf("\n\t       --fullvirt");
  printf("\n\t       --mac_addr=eth%%d:%%02x:%%02x:%%02x:%%02x:%%02x:%%02x");
  printf("\n\t       --balloon");
  printf("\n\t       --uefi");
  printf("\n\tnote: for the --persistent option, the rootfs must be a full");
  printf("\n\t      path to a file system. If not set, the rootfs writes are");
  printf("\n\t      evenescent, lost at shutdown.");
  printf("\n\t      If set, those writes are persistent after shutwown.\n");
  printf("\n\tnote: for the 9p_share, the shared dir mount point is");
  printf("\n\t      /mnt/p9_host_share in the guest kvm.\n\n");
  printf("\n\nexample:\n\n");

  printf("%s vm_name ram=2000 cpu=4 dpdk=2 sock=2 hwsim=2 buster.qcow2\n", line);
  printf("%s vm_name ram=2000 cpu=4 dpdk=0 sock=3 hwsim=0 buster.qcow2\n", line);
  printf("%s vm_name ram=2000 cpu=4 dpdk=0 sock=3 hwsim=0 buster.qcow2 --persistent\n", line);
  printf("\n\n\n");
}
/*-------------------------------------------------------------------------*/

/***************************************************************************/
static int fill_eth_params_from_argv(char *input, int eth_max,
                                     t_eth_params *eth_params)
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
      if ((num < 0) || (num >= eth_max))
        printf("\nBad interface number\n");
      else
        {
        if (sscanf(ptr, "%02x:%02x:%02x:%02x:%02x:%02x",
            &(mac[0]), &(mac[1]), &(mac[2]), 
            &(mac[3]), &(mac[4]), &(mac[5])) == MAC_ADDR_LEN) 
          {
          for (i=0; i<MAC_ADDR_LEN; i++)
            eth_params[num].mac_addr[i] = mac[i] & 0xFF;
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
static int local_add_kvm(char *name, int mem, int cpu, int dpdk, int eth,
                         int wlan, char *rootfs, int argc, char **argv)
{
  int i, result = 0, prop_flags = 0;
  char *img_linux = NULL;
  char *p9_host_shared=NULL;
  char *install_cdrom=NULL;
  char *added_cdrom=NULL;
  char *added_disk=NULL;
  t_eth_params eth_params[MAX_ETH_VM];

  memset(eth_params, 0, MAX_ETH_VM * sizeof(t_eth_params));
  prop_flags |= VM_CONFIG_FLAG_EVANESCENT;
  for (i=0; i<argc; i++)
    {
    if (!strncmp(argv[i], "--arm_kernel=", strlen("--arm_kernel=")))
      {
      prop_flags |= VM_CONFIG_FLAG_ARM;
      img_linux = argv[i] + strlen("--arm_kernel=");
      }
    else if (!strncmp(argv[i],"--aarch64_kernel=",strlen("--aarch64_kernel=")))
      {
      prop_flags |= VM_CONFIG_FLAG_AARCH64;
      img_linux = argv[i] + strlen("--aarch64_kernel=");
      }
    else if (!strcmp(argv[i], "--persistent"))
      {
      prop_flags |= VM_CONFIG_FLAG_PERSISTENT;
      prop_flags &= ~VM_CONFIG_FLAG_EVANESCENT;
      }
    else if (!strcmp(argv[i], "--fullvirt"))
      prop_flags |= VM_CONFIG_FLAG_FULL_VIRT;
    else if (!strcmp(argv[i], "--vhost-vsock"))
      prop_flags |= VM_CONFIG_FLAG_VHOST_VSOCK;
    else if (!strcmp(argv[i], "--balloon"))
      prop_flags |= VM_CONFIG_FLAG_BALLOONING;
    else if (!strcmp(argv[i], "--uefi"))
      prop_flags |= VM_CONFIG_FLAG_UEFI;
    else if (!strncmp(argv[i], "--9p_share=", strlen("--9p_share=")))
      {
      prop_flags |= VM_CONFIG_FLAG_9P_SHARED;
      p9_host_shared = argv[i] + strlen("--9p_share=");
      }
    else if (!strncmp(argv[i], "--install_cdrom=", strlen("--install_cdrom=")))
      {
      prop_flags |= VM_CONFIG_FLAG_INSTALL_CDROM;
      install_cdrom = argv[i] + strlen("--install_cdrom=");
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
    else if (!strcmp(argv[i], "--cisco"))
      {
      prop_flags |= VM_CONFIG_FLAG_CISCO;
      }
    else if (!strncmp(argv[i], "--mac_addr=eth", strlen("--mac_addr=eth")))
      {
      if (fill_eth_params_from_argv(argv[i], eth, eth_params))
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
    client_add_vm(0, callback_end, name, dpdk, eth, wlan, prop_flags, cpu,
                  mem, img_linux, rootfs, install_cdrom, added_cdrom,
                  added_disk, p9_host_shared, eth_params);
    }
  return result;
}
/*---------------------------------------------------------------------------*/


/***************************************************************************/
int cmd_add_vm_kvm(int argc, char **argv)
{
  int cpu, mem, dpdk, eth, wlan, result = -1;
  char *rootfs, *name;
  if (argc < 7) 
    printf("\nNot enough parameters for add kvm\n");

  else if (sscanf(argv[1], "ram=%d", &mem) != 1)
    printf("\nBad ram= parameter\n");
  else if ((mem < 1) || (mem > 500000))
    printf("\nBad ram range, must be between 1 and 500000 (in mega)\n");

  else if (sscanf(argv[2], "cpu=%d", &cpu) != 1)
    printf("\nBad cpu= parameter\n");
  else if ((cpu < 1) || (cpu > 128))
    printf("\nBad cpu range, must be between 1 and 128\n");

  else if (sscanf(argv[3], "dpdk=%d", &dpdk) != 1)
    printf("\nBad dpdk= parameter\n");
  else if ((dpdk < 0) || (dpdk > MAX_DPDK_VM))
    printf("\nBad dpdk range, must be between 0 and %d\n", MAX_DPDK_VM);

  else if (sscanf(argv[4], "sock=%d", &eth) != 1)
    printf("\nBad sock= parameter\n");
  else if ((eth < 0) || (eth > MAX_ETH_VM))
    printf("\nBad sock range, must be between 0 and %d\n", MAX_ETH_VM);

  else if (sscanf(argv[5], "hwsim=%d", &wlan) != 1)
    printf("\nBad hwsim= parameter\n");
  else if ((wlan < 0) || (wlan > MAX_ETH_VM))
    printf("\nBad hwsim range, must be between 0 and %d\n", MAX_WLAN_VM);

  else
    {
    name = argv[0];
    rootfs = argv[6];
    result = local_add_kvm(name, mem, cpu, dpdk, eth, wlan, rootfs,
                           argc-7, &(argv[7])); 
    }
  return result;
}
/*-------------------------------------------------------------------------*/











