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
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "io_clownix.h"
void rpct_recv_app_msg(void *ptr, int llid, int tid, char *line){KOUT(" ");}
void rpct_recv_diag_msg(void *ptr, int llid, int tid, char *line){KOUT(" ");}
void rpct_recv_evt_msg(void *ptr, int llid, int tid, char *line){KOUT(" ");}
void rpct_recv_cli_req(void *ptr, int llid, int tid,
                       int cli_llid, int cli_tid, char *line){KOUT(" ");}
void rpct_recv_cli_resp(void *ptr, int llid, int tid,
                        int cli_llid, int cli_tid, char *line){KOUT(" ");}
void rpct_recv_kil_req(void *ptr, int llid, int tid){KOUT(" ");}
void rpct_recv_pid_req(void *ptr, int llid, int tid, char *name, int num){KOUT(" ");}
void rpct_recv_pid_resp(void *ptr, int llid, int tid,
                        char *name, int num, int toppid, int pid){KOUT(" ");}
void rpct_recv_hop_sub(void *ptr, int llid, int tid, int flags_hop){KOUT(" ");}
void rpct_recv_hop_unsub(void *ptr, int llid, int tid){KOUT(" ");}
void rpct_recv_hop_msg(void *ptr, int llid, int tid,
                       int flags_hop, char *txt){KOUT(" ");}
void rpct_recv_report(void *ptr, int llid, t_blkd_item *item){KOUT(" ");}

