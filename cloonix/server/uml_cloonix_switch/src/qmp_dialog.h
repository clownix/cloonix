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
typedef void (*t_dialog_qmp_resp)(char *name, int llid, int tid, 
                                  char *req, char *resp);
typedef void (*t_end_qmp_conn)(char *name);
int  qmp_dialog_req(char *name, int llid, int tid, char *req,
                    t_dialog_qmp_resp cb);
void qmp_dialog_alloc(char *name, t_end_qmp_conn cb);
void qmp_dialog_free(char *name);
void qmp_dialog_init(void);
/*****************************************************************************/

