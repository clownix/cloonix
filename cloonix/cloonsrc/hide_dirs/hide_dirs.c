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
#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <syslog.h>

#include "glob_common.h"


/****************************************************************************/
static void write_in_file(char *path, char *content)
{
  int len, fd = open(path, O_RDWR | O_CLOEXEC);
  if (fd < 0)
    KOUT("ERROR %s %s", path, content);
  len = write(fd, content, strlen(content) + 1);
  if (len != strlen(content) + 1)
    KOUT("ERROR %s %s %d %d %s",
                path,content,len,(int)(strlen(content)+1),strerror(errno));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void become_uid0(uid_t orig_uid, gid_t orig_gid)
{
  char path[MAX_PATH_LEN];
  char content[MAX_NAME_LEN];

  memset(path, 0, MAX_PATH_LEN);
  memset(content, 0, MAX_NAME_LEN);
  snprintf(path, MAX_PATH_LEN-1, "/proc/self/setgroups");
  snprintf(content, MAX_PATH_LEN-1, "deny\n");
  write_in_file(path, content);

  memset(path, 0, MAX_PATH_LEN);
  memset(content, 0, MAX_NAME_LEN);
  snprintf(path, MAX_PATH_LEN-1, "/proc/self/gid_map");
  snprintf(content, MAX_PATH_LEN-1, "0 %u 1\n", orig_gid);
  write_in_file(path, content);

  memset(content, 0, MAX_NAME_LEN);
  memset(path, 0, MAX_PATH_LEN);
  snprintf(content, MAX_PATH_LEN-1, "0 %u 1\n", orig_uid);
  snprintf(path, MAX_PATH_LEN-1, "/proc/self/uid_map");
  write_in_file(path, content);

  if (setuid(0))
    KOUT("ERROR setuid %s\n", __FUNCTION__);
  if (setgid(0))
    KOUT("ERROR setgid %s\n", __FUNCTION__);
} 
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void become_orig(uid_t orig_uid, gid_t orig_gid)
{
  char path[MAX_PATH_LEN];
  char content[MAX_NAME_LEN];

  memset(path, 0, MAX_PATH_LEN);
  memset(content, 0, MAX_NAME_LEN);
  snprintf(path, MAX_PATH_LEN-1, "/proc/self/gid_map");
  snprintf(content, MAX_PATH_LEN-1, "%u 0 1\n", orig_gid);
  write_in_file(path, content);

  memset(content, 0, MAX_NAME_LEN);
  memset(path, 0, MAX_PATH_LEN);
  snprintf(content, MAX_PATH_LEN-1, "%u 0 1\n", orig_uid);
  snprintf(path, MAX_PATH_LEN-1, "/proc/self/uid_map");
  write_in_file(path, content);

  if (setuid(orig_uid))
    KOUT("ERROR setuid %s\n", __FUNCTION__);
  if (setgid(orig_gid))
    KOUT("ERROR setgid %s\n", __FUNCTION__);
} 
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int dir_is_link(const char *input)
{ 
  struct stat sb;
  int result = 0;
  if (lstat(input, &sb) == 0) 
    {
    if ((sb.st_mode & S_IFMT) == S_IFLNK)
      result = 1;
    }
  return result;
} 
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void hide_dir_if_necessary(const char *input)
{ 
  struct stat sb;
  if (lstat(input, &sb) == 0)
    {
    if (!dir_is_link(input))
      {
      if ((sb.st_mode & S_IFMT) == S_IFDIR)
        {
        if (mount("tmpfs", input, "tmpfs", 0, NULL))
          KOUT("ERROR %s %s", __FUNCTION__, input);
        }
      else
        KERR("ERROR %s %s", __FUNCTION__, input);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void setup_mounts(void)
{
  char *curdir = get_current_dir_name();
  assert(curdir);
  assert(mount("none", "/", NULL, MS_REC | MS_SLAVE, NULL) == 0);
  hide_dir_if_necessary("/etc");
  hide_dir_if_necessary("/bin");
  hide_dir_if_necessary("/sbin");
  hide_dir_if_necessary("/lib");
  hide_dir_if_necessary("/libx32");
  hide_dir_if_necessary("/lib32");
  hide_dir_if_necessary("/lib64");

  mkdir("/var/lib/cloonix", 0777);
  mkdir("/var/lib/cloonix/cache", 0777);
  mkdir("/var/lib/cloonix/cache/libexec", 0777);

  assert(mount("/usr/libexec/cloonix",
               "/var/lib/cloonix/cache/libexec",
               NULL, MS_BIND, NULL) == 0);

  hide_dir_if_necessary("/usr");

  mkdir("/usr/bin", 0777);
  mkdir("/usr/share", 0777);
  mkdir("/usr/share/i18n", 0777);
  mkdir("/usr/libexec", 0777);
  mkdir("/usr/libexec/cloonix", 0777);
  mkdir("/usr/lib", 0777);
  mkdir("/usr/lib64", 0777);
  mkdir("/usr/tmp", 0777);

  assert(mount("/var/lib/cloonix/cache/libexec",
               "/usr/libexec/cloonix", NULL, MS_BIND, NULL) == 0);

  assert(mount("/var/lib/cloonix/cache/libexec/cloonfs/lib", "/usr/lib",
               NULL, MS_BIND, NULL) == 0);

  assert(mount("/var/lib/cloonix/cache/libexec/cloonfs/lib64", "/usr/lib64",
               NULL, MS_BIND, NULL) == 0);

  assert(mount("/var/lib/cloonix/cache/libexec/cloonfs/share",
               "/usr/share", NULL, MS_BIND, NULL) == 0);

  assert(mount("/var/lib/cloonix/cache/libexec/cloonfs/etc",
               "/etc", NULL, MS_BIND, NULL) == 0);

  assert(mount("/var/lib/cloonix/cache/libexec/cloonfs/bin",
               "/usr/bin", NULL, MS_BIND, NULL) == 0);

  if (!dir_is_link("/bin"))
    assert(mount("/var/lib/cloonix/cache/libexec/cloonfs/bin",
                 "/bin", NULL, MS_BIND, NULL) == 0);

  chdir("/");
  chdir(curdir);
  free(curdir);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void hide_real_machine_cli(void)
{
  uid_t my_uid;
  gid_t my_gid;
  pthexec_init();
  if (!pthexec_running_in_crun(NULL))
    {  
    my_uid = getuid();;
    my_gid = getgid();;
    assert(unshare(CLONE_NEWNS | CLONE_NEWUSER) == 0);
    become_uid0(my_uid, my_gid);
    setup_mounts();
    assert(unshare(CLONE_NEWUSER) == 0);
    become_orig(my_uid, my_gid);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void hide_real_machine_serv(void)
{
  uid_t my_uid;
  gid_t my_gid;
  pthexec_init();
  if (!pthexec_running_in_crun(NULL))
    {
    my_uid = getuid();;
    my_gid = getgid();;
    assert(unshare(CLONE_NEWNS | CLONE_NEWUSER) == 0);
    become_uid0(my_uid, my_gid);
    setup_mounts();
    assert(unshare(CLONE_NEWUSER) == 0);
    become_orig(my_uid, my_gid);
    }
}
/*--------------------------------------------------------------------------*/
