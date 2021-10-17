/*****************************************************************************/
/*    Copyright (C) 2006-2021 clownix@clownix.net License AGPL-3             */
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
void rpct_recv_sigdiag_msg(int llid, int tid, char *line){KOUT(" ");}
void rpct_recv_poldiag_msg(int llid, int tid, char *line){KOUT(" ");}
void rpct_recv_kil_req(int llid, int tid){KOUT(" ");}
void rpct_recv_pid_req(int llid, int tid, char *name, int num){KOUT(" ");}
void rpct_recv_pid_resp(int llid, int tid,
                        char *name, int num, int toppid, int pid){KOUT(" ");}
void rpct_recv_hop_sub(int llid, int tid, int flags_hop){KOUT(" ");}
void rpct_recv_hop_unsub(int llid, int tid){KOUT(" ");}
void rpct_recv_hop_msg(int llid, int tid,
                       int flags_hop, char *txt){KOUT(" ");}

