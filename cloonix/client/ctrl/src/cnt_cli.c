/*****************************************************************************/
/*    Copyright (C) 2006-2022 clownix@clownix.net License AGPL-3             */
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
void help_add_cnt(char *line)
{
  printf("\n\n\n %s <name> eth=<eth_description> <image> <customer_launch> [options]\n",
  line);
  printf("\n\timage is a file containing the root file system.");
  printf("\n\tcustomer_launch is optionnal, a script that is in container.");
  printf("\n\teth_description example:");
  printf("\n\t\t  eth=svv says eth0 eth1 and eth2, eth0 is spyable");
  printf("\n\tMax eth: %d", MAX_ETH_VM);
  printf("\n\t[options]");
  printf("\n\t       --mac_addr=eth%%d:%%02x:%%02x:%%02x:%%02x:%%02x:%%02x");
  printf("\n\nexample:\n\n");
  printf("\n%s vm_name eth=sss bullseye.img\n", line);
  printf("This will give 3 eth that are wireshark spy compatible\n");
  printf("\n%s vm_name eth=vvv bullseye.img \"sleep 100\"\n", line);
  printf("This will give 3 eth that are not spyable\n");
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
static int local_add_cnt(char *name, int nb_tot_eth, t_eth_table *eth_tab,
                         char *image, char *customer_launch,
                         int argc, char **argv)
{
  int i, result = 0;
  t_topo_cnt cnt;

  for (i=0; i<argc; i++)
    {
    if (!strncmp(argv[i], "--mac_addr=eth", strlen("--mac_addr=eth")))
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
    memset(&cnt, 0, sizeof(t_topo_cnt));
    strncpy(cnt.name, name, MAX_NAME_LEN-1);
    cnt.nb_tot_eth = nb_tot_eth;
    memcpy(cnt.eth_table, eth_tab, nb_tot_eth * sizeof(t_eth_table));
    strncpy(cnt.image, image, MAX_PATH_LEN-1);
    strncpy(cnt.customer_launch, customer_launch, MAX_PATH_LEN-1);
    client_add_cnt(0, callback_end, &cnt);
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
int cmd_add_cnt(int argc, char **argv)
{
  int nb_eth, result = -1;
  char *image, *customer_launch, *name;
  char eth_string[MAX_PATH_LEN];
  char err[MAX_PATH_LEN];
  t_eth_table etht[MAX_ETH_VM];
  if (argc < 3) 
    printf("\nNot enough parameters for add cnt\n");
  else
    {
    if (sscanf(argv[1], "eth=%s", eth_string) != 1)
      printf("\nBad eth= parameter %s\n", argv[1]);
    else if (check_eth_desc(eth_string, err, &nb_eth, etht))
      printf("\nBad eth= parameter %s %s\n", argv[1], err);
    else
      {
      name = argv[0];
      image = argv[2];
      if (argc == 3)
        result = local_add_cnt(name, nb_eth, etht, image, "sleep 1000",
                               argc-3, &(argv[3])); 
      else
        {
        customer_launch = argv[3];
        result = local_add_cnt(name, nb_eth, etht, image, customer_launch,
                               argc-4, &(argv[4])); 
        }
      }
    }
  return result;
}
/*-------------------------------------------------------------------------*/











