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
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include "io_clownix.h"
#include "xml_utils.h"



/****************************************************************************/
/*  FUNCTION:                create_opening_mark                            */
/*--------------------------------------------------------------------------*/
static char *create_opening_mark(char *mark_txt)
{
  static char mark[MAX_XML_MARK_LEN];
  strcpy(mark, "<");
  strcat(mark, mark_txt);
  strcat(mark, ">");
  return mark;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
/*  FUNCTION:                create_closing_mark                            */
/*--------------------------------------------------------------------------*/
static char *create_closing_mark(char *mark_txt)
{
  static char mark[MAX_XML_MARK_LEN];
  strcpy(mark, "</");
  strcat(mark, mark_txt);
  strcat(mark, ">");
  return mark;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
/*  FUNCTION:                get_raw_xml_item                               */
/*--------------------------------------------------------------------------*/
static char *get_raw_xml_item(char *xml_string, char *mark_txt)
{
  char *open_mark = create_opening_mark(mark_txt);
  char *close_mark = create_closing_mark(mark_txt);
  static char item[MAX_XML_ITEM_LEN];
  char *ptr_start, *ptr_end;
  int len;
  char *pitem = NULL;
  ptr_start = strstr(xml_string, open_mark); 
  ptr_end = strstr(xml_string, close_mark); 
  len = ptr_end - (ptr_start+strlen(open_mark));
  if (ptr_start && ptr_end && (len > 0) && (len < MAX_XML_ITEM_LEN))
    {
    memset(item, 0, MAX_XML_ITEM_LEN);
    memcpy(item, ptr_start+strlen(open_mark), len);
    pitem = item;
    } 
  return pitem;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
/*  FUNCTION:                get_xml_item                                   */
/*--------------------------------------------------------------------------*/
char *xml_get_item(char *xml_string, char *mark_txt)
{
  char *raw_item = get_raw_xml_item(xml_string, mark_txt);
  char *pstart = NULL;
  char *pend;
  if (raw_item)
    {
    pstart = raw_item + strspn(raw_item, " \t\r\n"); 
    pend = pstart + strcspn(pstart, " \t\r\n");
    *pend = 0;
    }
  return pstart;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
/*  FUNCTION:                check_root_markup                              */
/*--------------------------------------------------------------------------*/
char *xml_get_txt_between_markups(char *xml_string, char *mark_txt, int *len)
{
  char *result = NULL;
  char *ptr_start, *ptr_end;
  char *open_mark  = create_opening_mark(mark_txt);
  char *close_mark = create_closing_mark(mark_txt);
  ptr_start = strstr(xml_string, open_mark);
  ptr_end = strstr(xml_string, close_mark);
  if (ptr_start && ptr_end && (ptr_start<ptr_end))
    {
    *len = (ptr_end + strlen(close_mark)) - ptr_start;
    result = ptr_start;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
/*             xml_goto_open_mark                                           */
/*--------------------------------------------------------------------------*/
char *xml_goto_open_mark(char *xml_string, char *mark_txt)
{
  char *open_mark  = create_opening_mark(mark_txt);
  return (strstr(xml_string, open_mark));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
/*             xml_goto_close_mark                                          */
/*--------------------------------------------------------------------------*/
char *xml_goto_close_mark(char *xml_string, char *mark_txt)
{
  char *close_mark = create_closing_mark(mark_txt);
  return (strstr(xml_string, close_mark));
}
/*--------------------------------------------------------------------------*/





