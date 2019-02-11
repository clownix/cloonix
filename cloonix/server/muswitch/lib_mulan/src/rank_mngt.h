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
typedef struct t_llid_rank
{
  int llid;
  int llid_unix_sock_traf;
  char name[MAX_NAME_LEN];
  int num;
  uint32_t prechoice_rank;
  uint32_t rank;
  int tidx;
  int state;
  struct t_llid_rank *prev;
  struct t_llid_rank *next;
} t_llid_rank;

int rank_get_max(void);
t_llid_rank *rank_get_with_idx(uint32_t rank);
t_llid_rank *rank_get_with_llid(int llid);
void rank_dialog(t_all_ctx *all_ctx, int llid, char *line, char *resp);
void rank_has_become_inactive(int llid, char *name, uint32_t rank);
void init_rank_mngt(void);
/*---------------------------------------------------------------------------*/
                                                           
