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
enum
{
backdoor_evt_none = 0,
backdoor_evt_connected,
backdoor_evt_disconnected,
backdoor_evt_ping_ok,
backdoor_evt_ping_ko,
};

void qhvc0_event_backdoor(char *name, int backdoor_evt);
void qhvc0_begin_qemu_unix(char *name);
void qhvc0_end_qemu_unix(char *name);
int  qhvc0_still_present(void);
void qhvc0_reinit_vm_in_doorways(void);
void init_qhvc0(void);
/*--------------------------------------------------------------------------*/
