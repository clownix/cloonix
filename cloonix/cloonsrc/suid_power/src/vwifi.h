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
typedef struct t_elem_vwifi
{
  char hwsim[MAX_NAME_LEN];
  char nspace[MAX_NAME_LEN];
  char net_main[MAX_NAME_LEN];
  char net_ns[MAX_NAME_LEN];
  char phy_main[MAX_NAME_LEN];
  struct t_elem_vwifi *prev;
  struct t_elem_vwifi *next;
} t_elem_vwifi;
void net_phy_get_hwsim(void);
t_elem_vwifi *get_diff_past_hwsim(int *nb);
int vwifi_insert_wlan_to_namespace(int nb_vwif, char *nspacecrun);
int vwifi_del_wlan(char *nspacecrun, char *phy, char *wlan);
void *vwifi_wlan_create(char *name, char *nspacecrun, int nb_vwif,
                        char mac_vwif[6], t_elem_vwifi **vwifi);
void vwifi_init(void);
void vwifi_after_suidroot_init(void);
void vwifi_iw_del_wlan(t_elem_vwifi *cur);
int vwifi_wlan_delete(int nb_vwif, char mac_vwif[6]);
/*--------------------------------------------------------------------------*/
