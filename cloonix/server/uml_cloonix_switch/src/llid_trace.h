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
int llid_trace_exists(int llid);
void llid_trace_alloc(int llid, char *name, int vm_id, int guest_fd, int type);
void llid_trace_free(int llid, int is_clone, const char *fct);
void llid_trace_init(void);
void llid_set_llid_2_num(int llid, int num);
void llid_get_llid_2_num(int llid, int *vm_id, int *num);
void llid_trace_vm_delete(int vm_id);
void llid_trace_free_if_exists(int llid);
void llid_set_event2llid(int llid, int event, int tid);
void llid_unset_event2llid(int llid, int event);
int  llid_get_event2llid(int event, int prev_llid, int *llid, int *tid);

int  llid_get_type_of_con(int llid, char *name, int *id);
void llid_free_all_llid(void);
void llid_set_qmonitor(int llid, char *name, int val);
void llid_count_beat(void);
int  llid_get_count_beat(int llid);
/*---------------------------------------------------------------------------*/



