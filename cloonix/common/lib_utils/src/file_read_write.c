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
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>
#include <dirent.h>
#include <errno.h>
#include <mntent.h>



#include "io_clownix.h"
#include "file_read_write.h"


/*****************************************************************************/
char *get_cloonix_config_path(void)
{
  return ("/mnt/cloonix_config_fs");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static char *get_cloonix_config_path_name(void)
{
  return ("/mnt/cloonix_config_fs/cloonix_vm_name");
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void make_config_cloonix_vm_name(char *confpath, char *name)
{
  char err[MAX_PATH_LEN];
  char str[MAX_PATH_LEN];
  char path[MAX_PATH_LEN];
  memset(str, 0, MAX_PATH_LEN);
  snprintf(str, MAX_PATH_LEN-1, "%s", name);
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "%s/cloonix_vm_name", confpath);
  if (write_whole_file(path, str, strlen(str), err))
    KERR("%s", err);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void make_config_cloonix_vm_p9_host_share(char *confpath, int has_p9_host_share)
{
  char err[MAX_PATH_LEN];
  char str[MAX_PATH_LEN];
  char path[MAX_PATH_LEN];
  memset(str, 0, MAX_PATH_LEN);
  if (has_p9_host_share)
    snprintf(str, MAX_PATH_LEN-1, "yes");
  else
    snprintf(str, MAX_PATH_LEN-1, "no");
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "%s/cloonix_vm_p9_host_share", confpath);
  if (write_whole_file(path, str, strlen(str), err))
    KERR("%s", err);
}
/*---------------------------------------------------------------------------*/




/****************************************************************************/
/*
static char *get_item(char *input_string)
{
  char *pstart = NULL;
  char *pend;
  pstart = input_string + strspn(input_string, " \t\r\n[");
  pend = input_string + strlen(input_string);
  while ((*(pend - 1) == '\n') || (*(pend - 1) == '\r') ||
         (*(pend - 1) == '\t') || (*(pend - 1) == ' ')  ||
         (*(pend - 1) == '>'))
    pend = pend - 1;
  *pend = 0;
  return pstart;
}
*/
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int i_am_inside_cloonix(char *name)
{
  int result, len;
  char err[MAX_PATH_LEN];
  char *buf;
  memset(name, 0, MAX_NAME_LEN);
  result = file_exists(get_cloonix_config_path_name(), F_OK);
  if (result)
    {
    buf = read_whole_file(get_cloonix_config_path_name(), &len, err);
    if (!buf)
      {
      KERR("%s", err);
      result = 0;
      }
    else
      {
      strncpy(name, buf, MAX_NAME_LEN-1);
      clownix_free(buf, __FILE__);
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/


/****************************************************************************/
/*  FUNCTION:                get_file_len                                   */
/*--------------------------------------------------------------------------*/
static int get_file_len(char *file_name)
{
  int result = -1;
  struct stat statbuf;
  if (stat(file_name, &statbuf) == 0)
    result = statbuf.st_size;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void mymkdir(char *dst_dir)
{
  struct stat stat_file;
  if (mkdir(dst_dir, 0777))
    {
    if (errno != EEXIST)
      KOUT("%s, %d", dst_dir, errno);
    else
      {
      if (stat(dst_dir, &stat_file))
        KOUT("%s, %d", dst_dir, errno);
      if (!S_ISDIR(stat_file.st_mode))
        {
        unlink(dst_dir);
        if (mkdir(dst_dir, 0777))
          KOUT("%s, %d", dst_dir, errno);
        }
      }
    }
}
/*--------------------------------------------------------------------------*/



/*****************************************************************************/
char *mybasename(char *path)
{
  static char tmp[MAX_PATH_LEN];
  char *pname;
  memset(tmp, 0, MAX_PATH_LEN);
  strncpy(tmp, path, MAX_PATH_LEN - 1);
  pname = basename(tmp);
  return (pname);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *mydirname(char *path)
{
  static char tmp[MAX_PATH_LEN];
  char *pdir;
  memset(tmp, 0, MAX_PATH_LEN);
  strncpy(tmp, path, MAX_PATH_LEN - 1);
  pdir = dirname(tmp);
  return (pdir);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int is_file_readable(char *path)
{
  int result = 0;
  struct stat stat_file;
  if (!stat(path, &stat_file))
    {
    if ((stat_file.st_mode & S_IFMT) == S_IFREG)
      if (!access(path, R_OK))
        result = 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int is_directory_readable(char *path)
{
  int result = 0;
  struct stat stat_file;
  if (!stat(path, &stat_file))
    {
    if ((stat_file.st_mode & S_IFMT) == S_IFDIR)
      {
      if (!access(path, R_OK))
        {
        result = 1;
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int is_directory_writable(char *path)
{
  int result = 0;
  struct stat stat_file;
  if (!stat(path, &stat_file))
    {
    if ((stat_file.st_mode & S_IFMT) == S_IFDIR)
      if (!access(path, W_OK))
        result = 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
char *read_whole_file(char *file_name, int *len, char *err)
{
  char *buf = NULL;
  int fd, readlen;
  *len = get_file_len(file_name);
  if (*len == -1)
    sprintf(err, "Cannot get size of file %s\n", file_name);
  else if (*len)
    {
    fd = open(file_name, O_RDONLY);
    if (fd > 0)
      {
      buf = (char *) clownix_malloc((*len+1) *sizeof(char),13);
      readlen = read(fd, buf, *len);
      if (readlen != *len)
        {
        sprintf(err, "Length of file error for file %s\n", file_name);
        clownix_free(buf, __FUNCTION__);
        buf = NULL;
        }
      else
        buf[*len] = 0;
      close (fd);
      }
    else
      sprintf(err, "Cannot open file %s\n", file_name);
    }
  else
    {
    buf = (char *) clownix_malloc(sizeof(char),13);
    buf[0] = 0;
    }
  return buf;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
int write_whole_file(char *file_name, char *buf, int len, char *err)
{
  int result = -1;
  int fd, writelen;
  err[0] = 0;
  if (access(file_name, F_OK))
    {
    fd = open(file_name, O_CREAT|O_WRONLY, 00666);
    if (fd > 0)
      {
      writelen = write(fd, buf, len);
      if (writelen != len)
        sprintf(err, "Something wrong during write of file %s\n", file_name);
      else
        result = 0;
      close (fd);
      }
    else
      sprintf(err, "Cannot open file %s\n", file_name);
    }
  else
    sprintf(err, "File %s already exists, delete it first\n", file_name);
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int file_exists(char *path, int mode)
{
  int err, result = 0;
  err = access(path, mode);
  if (!err)
    result = 1;
  return result;
}
/*---------------------------------------------------------------------------*/



