/*****************************************************************************/
/*    Copyright (C) 2006-2018 cloonix@cloonix.net License AGPL-3             */
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
#define MAX_JOBS 100

typedef struct t_opaque_agent_req
{
  int llid;
  int tid;
  char name[MAX_NAME_LEN];
  char action[MAX_NAME_LEN];
} t_opaque_agent_req;


typedef void (*t_timeout_service_cb)(int job_idx,int is_timeout,void *opaque);
/*--------------------------------------------------------------------------*/
void timeout_service_trigger(int job_idx);
int  timeout_service_alloc(t_timeout_service_cb cb, void *opaque, int delay);
void timeout_service_init(void);
/*--------------------------------------------------------------------------*/


