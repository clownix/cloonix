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
#define MAX_CLOONIX_SERVERS 100
/*--------------------------------------------------------------------------*/
typedef struct t_cloonix_conf_info
{
  char name[MAX_NAME_LEN];
  char doors[MAX_NAME_LEN];
  int  doors_llid;
  int  connect_llid;
  uint32_t  ip;
  int  port;
  char passwd[MSG_DIGEST_LEN];
  uint32_t c2c_udp_ip;
} t_cloonix_conf_info;
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *cloonix_conf_info_get_version(void);
char *cloonix_conf_info_get_work(void);
char *cloonix_conf_info_get_tree(void);
char *cloonix_conf_info_get_bulk(void);
char *cloonix_conf_info_get_names(void);
t_cloonix_conf_info *cloonix_cnf_info_get(char *name, int *rank);
void cloonix_conf_info_get_all(int *nb, t_cloonix_conf_info **tab);
int cloonix_conf_info_init(char *path_conf);
/*--------------------------------------------------------------------------*/


