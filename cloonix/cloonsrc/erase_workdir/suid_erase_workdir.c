/*****************************************************************************/
/*    Copyright (C) 2006-2025 clownix@clownix.net License AGPL-3             */
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
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <syslog.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>



#define MAX_NAME_LEN 50
#define MAX_PATH_LEN 200

/*****************************************************************************/
static int unlink_sub_dir_files(char *dir)
{ 
  int result = 0;
  char pth[MAX_PATH_LEN+MAX_NAME_LEN];
  DIR *dirptr;
  struct dirent *ent;
  dirptr = opendir(dir);
  if (dirptr)
    {
    while ((result == 0) && ((ent = readdir(dirptr)) != NULL))
      {
      if (!strcmp(ent->d_name, "."))
        continue;
      if (!strcmp(ent->d_name, ".."))
        continue;
      snprintf(pth, MAX_PATH_LEN+MAX_NAME_LEN, "%s/%s", dir, ent->d_name);
      pth[MAX_PATH_LEN+MAX_NAME_LEN-1] = 0;
      if(ent->d_type == DT_DIR)
        result = unlink_sub_dir_files(pth);
      else if (unlink(pth))
        { 
        printf("File: %s could not be deleted\n", pth);
        result = -1;
        }
      }
    if (closedir(dirptr))
      {
      printf("ERROR closedir errno %d\n", errno);
      result = -1;
      }
    if (rmdir(dir))
      {
      printf("Dir: %s could not be deleted\n", dir);
      result = -1;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int check_and_set_uid(void)
{
  int result = -1;
  uid_t uid;
  if ((getuid() == 0) && getgid() == 0)
    {
    result = 0;
    }
  else
    {
    seteuid(0);
    setegid(0);
    uid = geteuid();
    if (uid == 0)
      {
      result = 0;
      if (setuid(0))
        {
        printf("ERROR setuid(0)\n");
        result = -1;
        }
      if (setgid(0))
        {
        printf("ERROR setgid(0)\n");
        result = -1;
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int main (int argc, char *argv[])
{
  char path[MAX_PATH_LEN];
  struct stat stat_path;

  if (argc != 2)
    {
    printf("ERROR, param needed:\n");
    printf("%s <cloonix name>\n", argv[0]);
    return -1;
    }
  if ((strlen(argv[1]) < 2) || (strlen(argv[1]) >= MAX_NAME_LEN))
    {
    printf("ERROR len of cloonix name wrong: %d\n", strlen(argv[1]));
    return -1;
    }
  if (check_and_set_uid())
    {
    printf("ERROR, cloonix-suid-erase-workdir is not suid root\n");
    }
  memset(path, 0, MAX_PATH_LEN);
  sprintf(path, "/var/lib/cloonix/%s", argv[1]);
  if (stat(path, &stat_path) == 0)

    {
    if (unlink_sub_dir_files(path))
      {
      printf( "Path: %s already exists "
              "please remove and restart \n", path);
      return -1;
      }
    else
      printf( "Path: %s already exists, removing it\n", path);
    }

  return 0;
}
/*--------------------------------------------------------------------------*/

