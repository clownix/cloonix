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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "io_clownix.h"
#include "lib_doorways.h"


/****************************************************************************/
static int tst_port(char *str_port, int *port)
{
  int result = 0;
  unsigned long val;
  char *endptr;
  val = strtoul(str_port, &endptr, 10);
  if ((endptr == NULL)||(endptr[0] != 0))
    result = -1;
  else
    {
    if ((val < 1) || (val > 65535))
      result = -1;
    *port = (int) val;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void shift_left_3_char(char *str)
{
  char *ptr_dst=str;
  char *ptr_src=str+3;
  while (*ptr_src)
    {
    *ptr_dst = *ptr_src;
    ptr_src += 1;
    ptr_dst += 1;
    }
  *ptr_dst = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void shift_right_2_char(char *str)
{
  char *ptr_dst = str+2+strlen(str)-1;
  char *ptr_src = str+strlen(str)-1;
  if (strlen(str) < 1)
    {
    fprintf(stderr, "FATAL %s\n", __FUNCTION__);
    exit(255);
    }
  *(ptr_dst+1) = 0;
  while (ptr_src >= str)
    {
    *ptr_dst = *ptr_src;
    ptr_src -= 1;
    ptr_dst -= 1;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *get_full_bin_path(char *input_callbin)
{
  static char path[MAX_PATH_LEN];
  char callbin[MAX_PATH_LEN];
  int len, must_put_dot = 0;
  char *ptr;
  memset(path, 0, MAX_PATH_LEN);
  memset(callbin, 0, MAX_PATH_LEN);
  strncpy(callbin, input_callbin, MAX_PATH_LEN-1);
  if (!getcwd(path, MAX_PATH_LEN-1))
    {
    fprintf(stderr, "FATAL getcwd %s\n", input_callbin);
    exit(255);
    }
  ptr = strrchr(callbin, '/');
  if (!ptr)
    {
    fprintf(stderr, "FATAL get_full_bin_path %s\n", input_callbin);
    exit(255);
    }
  else
    {
   while((callbin[0] == '.') && (callbin[1] == '.') && (callbin[2] == '/'))
      {
      shift_left_3_char(callbin);
      ptr = strrchr(path, '/');
      if (!ptr)
        {
        fprintf(stderr, "FATAL %s %s\n", callbin, path);
        exit(255);
        }
      *ptr = 0;
      must_put_dot = 1;
      }
    if (must_put_dot)
      {
      shift_right_2_char(callbin);
      callbin[0] = '.';
      callbin[1] = '/';
      }
    if (callbin[0] == '.')
      {
      if (callbin[1] != '/')
        {
        fprintf(stderr, "FATAL %s not managed\n", callbin);
        exit(255);
        }
      len = strlen(path);
      strncat(path, &(callbin[1]), MAX_PATH_LEN-1-len);
      }
    else if (callbin[0] == '/')
      {
      ptr = callbin;
      snprintf(path, MAX_PATH_LEN-1, "%s",ptr);
      }
    else
      {
      fprintf(stderr, "FATAL %s not managed\n", callbin);
      exit(255);
      }
    }
  return path;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int get_ip_port_from_path(char *param, uint32_t *ip, int *port)
{
  char pm[MAX_PATH_LEN];
  int result = -1;
  char *ptr_ip, *ptr_port;
  if (strlen(param) >= MAX_PATH_LEN)
    fprintf(stderr, "param LENGTH Problem\n");
  else
    {
    memset(pm, 0, MAX_PATH_LEN);
    strncpy(pm, param, MAX_PATH_LEN-1);
    ptr_ip = pm;
    ptr_port = strchr(pm, '=');
    if (ptr_port)
      {
      *ptr_port = 0;
      ptr_port++;
      if (ip_string_to_int (ip, ptr_ip))
        fprintf(stderr, "IP param Problem %s\n", param);
      else if (tst_port(ptr_port, port))
        fprintf(stderr, "PORT param Problem %s\n", param);
      else
        result = 0;
      }
    else
      fprintf(stderr, "Bad address: %s\n", param);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int my_rand(int max)
{
  unsigned int result = rand();
  result %= max;
  return (int) result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_random_choice_str(void)
{
  static char name[8];
  int i, chance;
  long long date_us = cloonix_get_usec();
  srand((int) (date_us & 0xFFFF));
  memset (name, 0 , 8);
  for (i=0; i < 4; i++)
    {
    chance = my_rand(3);
    if (chance == 0)
      name[i] = 'A'+ my_rand(26);
    else if (chance == 1)
      name[i] = 'a'+ my_rand(26);
    else
      name[i] = '0'+ my_rand(10);
    }
  return name;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int test_file_is_socket(char *path)
{
  int result = -1;
  struct stat stat_file;
  if (!stat(path, &stat_file))
    {
    if (S_ISSOCK(stat_file.st_mode))
      result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/



