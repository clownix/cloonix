/*****************************************************************************/
/*    Copyright (C) 2006-2023 clownix@clownix.net License AGPL-3             */
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
#define DOCKER_BIN "/usr/bin/docker"
#define PODMAN_BIN "/usr/bin/podman"
#define MAX_IMAGE_TAB 100
#define MAX_CONTAINER_TAB 100
void docker_kill_all(void);
void docker_recv_poldiag_msg(int llid, int tid, char *line);
void docker_recv_sigdiag_msg(int llid, int tid, char *line);
void docker_beat(int llid);
void docker_init(void);
/*--------------------------------------------------------------------------*/
