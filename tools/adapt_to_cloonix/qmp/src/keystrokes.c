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
#include <stdio.h>

#include <string.h>

#include "commun.h"

#define MAX_KEYSTROKE 100

#define SINGLE_NUM "[{\"type\":\"number\", \"data\":%d}]"

#define DOUBLE_NUM "[{\"type\":\"number\",\"data\":%d},"\
                    "{\"type\":\"number\",\"data\":%d}]"



/***************************************************************************/
char *qmp_keystroke(int fr, char c)
{
  static char keystroke[MAX_KEYSTROKE];
  memset(keystroke, 0, MAX_KEYSTROKE);
  switch (c)
    {
    case '%':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 40);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 6);
    case '+':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 13);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 13);
      break;
    case '~':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 184, 3);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 41);
      break;
    case '#':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 184, 4);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 4);
      break;
    case '{':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 184, 5);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 26);
      break;
    case '}':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 184, 13);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 27);
      break;
    case '[':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 184, 6);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 26);
      break;
    case ']':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 184, 12);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 27);
      break;
    case '|':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 184, 7);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 43);
      break;
    case '`':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 184, 8);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 100, 42);
      break;
    case '\\':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 184, 9);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 43);
      break;
    case '^':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 184, 10);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 7);
      break;
    case '*':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 100, 43);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 9);
      break;
    case '!':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 100, 53);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 2);
      break;
    case '$':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 100, 27);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 5);
      break;
    case '>':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 86);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 52);
      break; 
    case '<':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 86);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 51);
      break; 
    case 'a':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 16);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 30);
      break; 
    case 'b':
      snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 48);
      break; 
    case 'c':
      snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 46);
      break;
    case 'd':
      snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 32);
      break;
    case 'e':
      snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 18);
      break;
    case 'f':
      snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 33);
      break;
    case 'g':
      snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 34);
      break;
    case 'h':
      snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 35);
      break;
    case 'i':
      snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 23);
      break;
    case 'j':
      snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 36);
      break;
    case 'k':
      snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 37);
      break;
    case 'l':
      snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 38);
      break;
    case 'm':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 39);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 50);
      break;
    case 'n':
      snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 49);
      break;
    case 'o':
      snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 24);
      break;
    case 'p':
      snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 25);
      break;
    case 'q':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 30);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 16);
      break;
    case 'r':
      snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 19);
      break;
    case 's':
      snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 31);
      break;
    case 't':
      snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 20);
      break;
    case 'u':
      snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 22);
      break;
    case 'v':
      snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 47);
      break;
    case 'w':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 44);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 17);
      break;
    case 'x':
      snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 45);
      break;
    case 'y':
      snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 21);
      break;
    case 'z':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 17);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 44);
      break;
    case 'A':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 16);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 30);
      break;
    case 'B':
      snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 48);
      break;
    case 'C':
      snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 46);
      break;
    case 'D':
      snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 32);
      break;
    case 'E':
      snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 18);
      break;
    case 'F':
      snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 33);
      break;
    case 'G':
      snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 34);
      break;
    case 'H':
      snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 35);
      break;
    case 'I':
      snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 23);
      break;
    case 'J':
      snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 36);
      break;
    case 'K':
      snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 37);
      break;
    case 'L':
      snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 38);
      break;
    case 'M':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 39);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 50);
      break;
    case 'N':
      snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 49);
      break;
    case 'O':
      snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 24);
      break;
    case 'P':
      snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 25);
      break;
    case 'Q':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 30);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 16);
      break;
    case 'R':
      snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 19);
      break;
    case 'S':
      snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 31);
      break;
    case 'T':
      snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 20);
      break;
    case 'U':
      snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 22);
      break;
    case 'V':
      snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 47);
      break;
    case 'W':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 44);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 17);
      break;
    case 'X':
      snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 45);
      break;
    case 'Y':
      snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 21);
      break;
    case 'Z':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 17);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 44);
      break;
    case '0':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 11);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 11);
      break;
    case '1':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 2);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 2);
      break;
    case '2':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 3);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 3);
      break;
    case '3':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 4);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 4);
      break;
    case '4':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 5);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 5);
      break;
    case '5':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 6);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 6);
      break;
    case '6':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 7);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 7);
      break;
    case '7':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 8);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 8);
      break;
    case '8':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 9);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 9);
      break;
    case '9':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 10);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 10);
      break;
    case '-':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 7);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 12);
      break;
    case '.':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 51);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 52);
      break;
    case '/':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 52);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 53);
      break;
    case ' ':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 57);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 57);
      break;
    case '_':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 9);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 12);
      break;
    case ',':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 50);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 51);
      break;
    case ';':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 51);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 39);
      break;
    case ':':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 52);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 39);
      break;
    case '(':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 6);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 10);
      break;
    case ')':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 12);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 11);
      break;
    case '=':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 13);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 13);
      break;
    case '"':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 4);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 40);
      break;
    case '\'':
      snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 5);
      break;
    case '@':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 184, 11);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 3);
      break;
    case '&':
      if (fr)
        snprintf(keystroke, MAX_KEYSTROKE-1, SINGLE_NUM, 2);
      else
        snprintf(keystroke, MAX_KEYSTROKE-1, DOUBLE_NUM, 42, 8);
      break;
    default:
      KERR("%c", c);
    }
  return keystroke;
}
/*-------------------------------------------------------------------------*/
