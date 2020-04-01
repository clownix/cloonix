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
#define MAX_CLOWNIX_BOUND_LEN      64
#define MIN_CLOWNIX_BOUND_LEN      2

/*---------------------------------------------------------------------------*/
#define SUB2QMONITOR         "<sub2qmonitor>\n"\
                             "  <tid> %d </tid>\n"\
                             "  <name> %s </name>\n"\
                             "  <sub_on_off> %d </sub_on_off>\n"\
                             "</sub2qmonitor>"
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
#define QMONITOR_O             "<qmonitor>\n"\
                             "  <tid> %d </tid>\n"\
                             "  <name> %s </name>\n"

#define QMONITOR_I "<q_monitor_asciidata_delimiter>%s</q_monitor_asciidata_delimiter>\n"

#define QMONITOR_C \
                             "</qmonitor>"
/*---------------------------------------------------------------------------*/


