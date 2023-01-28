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

#include <arpa/inet.h>


#define MAX_RXTX_LEN 1500
#define MAX_TAP_BUF_LEN 1600
#define HEADER_TAP_MSG 4
#define END_FRAME_ADDED_CHECK_LEN 4
#define TOT_MSG_BUF_LEN (MAX_TAP_BUF_LEN+HEADER_TAP_MSG+END_FRAME_ADDED_CHECK_LEN)

void* utils_malloc(int size);
void utils_free(void *ptr);
void utils_init(void);
/*---------------------------------------------------------------------------*/
