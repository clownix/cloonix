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
#define MAX_CLOWNIX_BOUND_LEN      64
#define MIN_CLOWNIX_BOUND_LEN      2
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
#define INFO_O  "<doors_any_info>\n"\
                 "  <tid>  %d </tid>\n"\
                 "  <info> %s </info>\n"

#define INFO_I  "  <doors_info_delimiter>%s</doors_info_delimiter>\n"

#define INFO_C  "</doors_any_info>"
/*---------------------------------------------------------------------------*/
#define EVENT_O  "<doors_event>\n"\
                 "  <tid>  %d </tid>\n"\
                 "  <name> %s </name>\n"

#define EVENT_I  "  <doors_event_delimiter>%s</doors_event_delimiter>\n"

#define EVENT_C  "</doors_event>"
/*---------------------------------------------------------------------------*/
#define STATUS_O  "<doors_status>\n"\
                  "  <tid>  %d </tid>\n"\
                  "  <name> %s </name>\n"

#define STATUS_I  "  <doors_status_delimiter>%s</doors_status_delimiter>\n"

#define STATUS_C  "</doors_status>"
/*---------------------------------------------------------------------------*/
#define COMMAND_O  "<doors_command>\n"\
                   "  <tid>  %d </tid>\n"\
                   "  <name> %s </name>\n"

#define COMMAND_I  "  <doors_command_delimiter>%s</doors_command_delimiter>\n"

#define COMMAND_C  "</doors_command>"
/*---------------------------------------------------------------------------*/
#define ADD_VM     "<doors_add_vm>\n"\
                   "  <tid>  %d </tid>\n"\
                   "  <name> %s </name>\n"\
                   "  <way2agent> %s </way2agent>\n"\
                   "</doors_add_vm>"
/*---------------------------------------------------------------------------*/
#define DEL_VM     "<doors_del_vm>\n"\
                   "  <tid>  %d </tid>\n"\
                   "  <name> %s </name>\n"\
                   "</doors_del_vm>"
/*---------------------------------------------------------------------------*/







