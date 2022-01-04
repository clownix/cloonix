/*****************************************************************************/
/*    Copyright (C) 2006-2022 clownix@clownix.net License AGPL-3             */
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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "mdl.h"
#include "getsock.h"
#include "wrap.h"
#include "debug.h"


/****************************************************************************/
static char *make_cmd_msg(int argc, char **argv)
{
  char *result = NULL;
  int i, len = 0;
  for (i=0; i<argc; i++)
    {
    len += strlen(argv[i]) + 8;
    }
  result = (char *) wrap_malloc(len);
  memset(result, 0, len);
  for (i=0; i<argc; i++)
    {
    if (strchr(argv[i], ' '))
      {
      strcat(result, "\"");
      strcat(result, argv[i]);
      strcat(result, "\"");
      }
    else
      strcat(result, argv[i]);
    if (i != argc-1)
      strcat(result, " ");
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int check_dest_dir_path(char *src_file, char *dst_dir, char **dst)
{
  int len, result = -1;
  struct stat sb;
  char *bn;
  if (stat(dst_dir, &sb) == -1)
    printf("\n\n%s does not exist\n\n", dst_dir);
  else if ((sb.st_mode & S_IFMT) != S_IFDIR)
    printf("\n\n%s is not a directory\n\n", dst_dir);
  else
    {
    bn = basename(src_file);
    len = strlen(bn) + strlen(dst_dir) + 2;
    *dst = (char *) wrap_malloc(len+1);
    (*dst)[len] = 0;
    if (dst_dir[strlen(dst_dir)-1] == '/')
      snprintf(*dst, len, "%s%s", dst_dir, bn);
    else
      snprintf(*dst, len, "%s/%s", dst_dir, bn);
    if (stat(*dst, &sb) != -1)
      printf("\n\n%s exists already\n\n", *dst);
    else
      result = 0;
      }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int check_source_file_path(char *src_file, char *dst_dir, char **dst)
{
  int len, result = -1;
  struct stat sb;
  char *bn;
  if (stat(src_file, &sb) == -1)
    printf("\n\n%s does not exist\n\n", src_file);
  else if ((sb.st_mode & S_IFMT) != S_IFREG)
    printf("\n\n%s not regular file\n\n", src_file);
  else
    {
    bn = basename(src_file);
    len = strlen(bn) + strlen(dst_dir) + 2;
    *dst = (char *) wrap_malloc(len+1);
    if (dst_dir[strlen(dst_dir)-1] == '/')
      snprintf(*dst, len, "%s%s", dst_dir, bn);
    else
      snprintf(*dst, len, "%s/%s", dst_dir, bn);
    (*dst)[len] = 0;
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int get_input_params_cloonix(int argc, char **argv, int *action,
                             char **src, char **dst, char **cmd)
{
  int result = 0;
  if (!strcmp(argv[0], "-dae"))
    {
    if (argc > 1)
      {
      *action = action_dae;
      *cmd = make_cmd_msg(argc-1, &(argv[1]));
      }
    else
      {
      printf("\n\nMissing arg command\n\n");
      result = -1;
      }
    }
  else if (!strcmp(argv[0], "-cmd"))
    {
    if (argc > 1)
      {
      *action = action_cmd;
      *cmd = make_cmd_msg(argc-1, &(argv[1]));
      }
    else
      {
      printf("\n\nMissing arg command\n\n");
      result = -1;
      }
    }
  else if (!strcmp(argv[0], "-get"))
    {
    if (argc == 3)
      {
      *action = action_get;
      *src = argv[1];
      if (check_dest_dir_path(*src, argv[2], dst))
        {
        result = -1;
        }
      }
    else
      {
      if (argc > 3)
        printf("\n\nToo many arg for -get\n\n");
      else if (argc == 1)
        printf("\n\nMissing arg distant-file and local-dir for -get\n\n");
      else if (argc == 2)
        printf("\n\nMissing arg local-dir for -get\n\n");
      result = -1;
      }
    }
  else if (!strcmp(argv[0], "-put"))
    {
    if (argc == 3)
      {
      *action = action_put;
      *src = argv[1];
      if (check_source_file_path(*src, argv[2], dst))
        {
        result = -1;
        }
      }
    else
      {
      if (argc > 3)
        printf("\n\nToo many arg for -put\n\n");
      else if (argc == 1)
        printf("\n\nMissing arg local-file and distant-dir for -put\n\n");
      else if (argc == 2)
        printf("\n\nMissing arg distant-dir for -put\n\n");
      result = -1;
      }
    }
  else
    {
    printf("\n\nBad arg: %s  no arg or -cmd, -get, -put\n\n",argv[0]);
    result = -1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int get_input_params(int argc, char **argv, int *action, int *ip, int *port,
                     char **src, char **dst, char **cmd)
{
  int result = -1, eport;
  unsigned long inet_addr;
  *action = action_bash;
  *cmd = *src = *dst = NULL; 
  *ip = *port = 0;
  if (mdl_ip_string_to_int(&inet_addr, argv[1]) == 0)
    {
    if (argc < 3)
      printf("\n\nNeeds port after ip\n\n");
    else
      {
      eport = mdl_parse_val(argv[2]);
      if (eport > 0)
        {
        *ip = (int) inet_addr;
        *port = eport; 
        result = 0;
        }
      else
        printf("\n\nBad port: %s\n\n", argv[2]);
      }
    }
  else
    {
    printf("\n\nNeeds ip and port at least\n\n");
    }
  if ((result ==0 ) && (argc > 3))
    result = get_input_params_cloonix(argc-3, argv+3, action, src, dst, cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

