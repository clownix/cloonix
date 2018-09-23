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
typedef void (*t_from_child_to_main)(void *data, char *msg);
typedef int (*t_fct_to_launch)(void *data);
typedef void (*t_launched_death)(void *data, int status, char *nm);
int pid_clone_launch(t_fct_to_launch fct, t_launched_death death, 
                     t_from_child_to_main fct_msg, 
                     void *data_start, void *data_end, void *data_msg, 
                     char *vm_name, int fd_not_to_close, int no_kerr);
void pid_clone_init(void);
int get_nb_running_pids(void);
void send_to_daddy (char *str);
int my_popen(char *exe, char *argv[]);
char *pid_get_clone_internal_com(void);

void pid_clone_kill_all(void);
void pid_clone_kill_single(int pid);

void kerr_running_pids(void);

int i_am_clone(void);



