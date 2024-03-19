/*****************************************************************************/
/*    Copyright (C) 2006-2024 clownix@clownix.net License AGPL-3             */
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
#define MAX_XML_MARK_LEN 80
#define MAX_XML_ITEM_LEN 2*MAX_PATH_LEN 
char *xml_goto_open_mark(char *xml_string, char *mark_txt);
char *xml_goto_close_mark(char *xml_string, char *mark_txt);
char *xml_get_item(char *xml_string, char *mark_txt);
char *xml_get_txt_between_markups(char *xml_string, char *mark_txt, int *len);
/*--------------------------------------------------------------------------*/






