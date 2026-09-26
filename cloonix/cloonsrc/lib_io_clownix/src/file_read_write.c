/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
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

static char *get_inside_cloonix_config_path_name(void)
{
  return ("/cloonixmnt/cnf_fs/cloonix_vm_name");
}

int i_am_inside_cloonix(char *name)
{
  int result, len;
  char err[MAX_PATH_LEN];
  char *buf;
  memset(name, 0, MAX_NAME_LEN);
  result = file_exists(get_inside_cloonix_config_path_name(), R_OK);
  if (result)
    {
    buf = read_whole_file(get_inside_cloonix_config_path_name(), &len, err);
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

static int get_file_len(char *file_name)
{
  int result = -1;
  struct stat statbuf;
  if (stat(file_name, &statbuf) == 0)
    result = statbuf.st_size;
  return result;
}

char *mydirname(char *path)
{
  static char tmp[MAX_PATH_LEN];
  char *pdir;
  memset(tmp, 0, MAX_PATH_LEN);
  strncpy(tmp, path, MAX_PATH_LEN - 1);
  pdir = dirname(tmp);
  return (pdir);
}

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
    if (fd >= 0)
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

int write_whole_file(char *file_name, char *buf, int len, char *err)
{
  int result = -1;
  int fd, writelen;
  err[0] = 0;
  if (access(file_name, R_OK))
    {
    fd = open(file_name, O_CREAT|O_WRONLY, 00666);
    if (fd >= 0)
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

/* 1 when /proc/cpuinfo carries vmx or svm, the flag kvm needs.             */
int machine_is_kvm_able(void)
{
  char line[500];
  int found = 0;
  FILE *fhd = fopen("/proc/cpuinfo", "r");
  if (!fhd)
    KOUT("ERROR /proc/cpuinfo %s", strerror(errno));
  while (fgets(line, sizeof(line), fhd) != NULL)
    {
    if (!strncmp(line, "flags", strlen("flags")))
      {
      found = ((strstr(line, "vmx")) || (strstr(line, "svm")));
      break;
      }
    }
  fclose(fhd);
  return found;
}

/* 0 when dev_file can be opened read-write, else -1 and why says which of  */
/* the three steps failed.                                                  */
int dev_file_rw_ko(char *dev_file, char *why)
{
  int fd;
  if (access(dev_file, R_OK))
    strcpy(why, "not found");
  else if (access(dev_file, R_OK|W_OK))
    strcpy(why, "not writable");
  else
    {
    fd = open(dev_file, O_RDWR);
    if (fd < 0)
      strcpy(why, "open read-write refused");
    else
      {
      close(fd);
      return 0;
      }
    }
  return -1;
}

int file_exists(char *path, int mode)
{
  int err, result = 0;
  err = access(path, mode);
  if (!err)
    result = 1;
  return result;
}

/* rm -rf dir. A mount point is emptied and kept (EBUSY on the last rmdir).
   err, MAX_PATH_LEN, names the first thing that resisted; -1 then.        */
int rm_tree(char *dir, char *err)
{
  int result = 0;
  char pth[MAX_PATH_LEN+MAX_NAME_LEN];
  struct stat sb;
  struct dirent *ent;
  DIR *dirptr = opendir(dir);
  if (dirptr)
    {
    while ((result == 0) && ((ent = readdir(dirptr)) != NULL))
      {
      if ((!strcmp(ent->d_name, ".")) || (!strcmp(ent->d_name, "..")))
        continue;
      snprintf(pth, sizeof(pth), "%s/%s", dir, ent->d_name);
      if ((!lstat(pth, &sb)) && (S_ISDIR(sb.st_mode)))
        result = rm_tree(pth, err);
      else if (unlink(pth))
        {
        snprintf(err, MAX_PATH_LEN, "File: %.250s could not be deleted\n", pth);
        result = -1;
        }
      }
    closedir(dirptr);
    if ((result == 0) && rmdir(dir) && (errno != EBUSY))
      {
      snprintf(err, MAX_PATH_LEN, "Dir: %s could not be deleted\n", dir);
      result = -1;
      }
    }
  return result;
}
