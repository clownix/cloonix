/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* copy_libs: run in the build chroot by rootfs_targz_store.sh. ldd every
 * executable of the rootfs (bin, sbin at depth 1, rsyslog/systemd plug-in
 * dirs, and the bin sub-dirs named in create_libs_txt) and copy the shared
 * libraries into the rootfs lib. A dir not named here is silently skipped. */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#define MAX_PATH_LEN 300
#define MAX_NAME_LEN 100

static char g_rootfs[MAX_PATH_LEN];
static char g_rootfs_lib[MAX_PATH_LEN];
static char g_collect[MAX_PATH_LEN];
static char g_names[MAX_PATH_LEN];
static char g_paths[MAX_PATH_LEN];
static char g_dumps[MAX_PATH_LEN];

typedef struct t_lib_info
{
  char name[MAX_NAME_LEN];
  char path[MAX_PATH_LEN];
  struct t_lib_info *prev;
  struct t_lib_info *next;
} t_lib_info;

static t_lib_info *g_head_lib_all;

static void alloc_lib_all(char *name, char *path)
{
  t_lib_info *cur = (t_lib_info *) malloc(sizeof(t_lib_info));
  memset(cur, 0, sizeof(t_lib_info));
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  strncpy(cur->path, path, MAX_PATH_LEN-1);
  if (g_head_lib_all)
    g_head_lib_all->prev = cur;
  cur->next = g_head_lib_all;
  g_head_lib_all = cur;
}

static void create_libs_other(FILE *col, char *common, char *name_lib)
{
  fprintf(col, "if [ -d %s/lib/%s ]; then\n",
          common, name_lib);
  fprintf(col, "  for i in `find %s/lib/%s -type f` ; do\n",
          common, name_lib);
  fprintf(col, "    ldd -v ${i} >> %s 2>/dev/null\n", g_dumps);
  fprintf(col, "  done\n");
  fprintf(col, "fi\n");
}

/* A directory under bin (bin/jscloonix holds gui-server, webshark, dumpcap). */
static void create_libs_bin_subdir(FILE *col, char *common, char *subdir)
{
  fprintf(col, "if [ -d %s/bin/%s ]; then\n", common, subdir);
  fprintf(col, "  for i in `find %s/bin/%s -maxdepth 1 -type f` ; do\n",
          common, subdir);
  fprintf(col, "    ldd -v ${i} >> %s 2>/dev/null\n", g_dumps);
  fprintf(col, "  done\n");
  fprintf(col, "fi\n");
}

static void create_libs_paragraph(FILE *col, char *common, char *name_lib)
{
  fprintf(col, "if [ -d %s/lib/x86_64-linux-gnu/%s ]; then\n",
          common, name_lib);
  fprintf(col, "  for i in `find %s/lib/x86_64-linux-gnu/%s -type f` ; do\n",
          common, name_lib);
  fprintf(col, "    ldd -v ${i} >> %s 2>/dev/null\n", g_dumps);
  fprintf(col, "  done\n");
  fprintf(col, "fi\n");
}

static int create_libs_txt(void)
{
  FILE *col;
  int result = -1;

  col = fopen(g_collect, "w+");
  if (col == NULL)
    printf("%s ERROR\n", g_collect);
  else
    {
    fprintf(col, "#!/bin/bash\n");

    fprintf(col, "echo " " > %s\n", g_dumps);

    fprintf(col, "for i in `find %s/bin -maxdepth 1 -type f` ; do\n", g_rootfs);
    fprintf(col, "  ldd -v ${i} >> %s 2>/dev/null\n", g_dumps);
    fprintf(col, "done\n");

    fprintf(col, "for i in `find %s/sbin -maxdepth 1 -type f` ; do\n", g_rootfs);
    fprintf(col, "  ldd -v ${i} >> %s 2>/dev/null\n", g_dumps);
    fprintf(col, "done\n");

    create_libs_other(col, g_rootfs, "rsyslog");
    create_libs_other(col, g_rootfs, "systemd");
    create_libs_paragraph(col, g_rootfs, "rsyslog");
    create_libs_paragraph(col, g_rootfs, "systemd");

    /* Every executable dir that is not bin or sbin. bin/qemu is data only. */
    create_libs_bin_subdir(col, g_rootfs, "jscloonix");

    fprintf(col, "echo " " >> %s\n", g_dumps);
    fclose(col);
    if (chmod(g_collect, 0700))
      printf("%s %d ERR chmod\n", g_collect, errno);
    else if (system(g_collect))
      printf("%s %d ERR system\n", g_collect, errno);
    else
      result = 0;
    }
  return result;
}

static void copy_lib_all(char *target, FILE *debug2)
{
  char cmd[2*MAX_PATH_LEN];
  t_lib_info *cur = g_head_lib_all;
  while(cur)
    {
    memset(cmd, 0, 2*MAX_PATH_LEN);
    if (strstr(cur->path, "lib/x86_64-linux-gnu"))
      snprintf(cmd, 2*MAX_PATH_LEN-1,
               "cp -f %s %s/x86_64-linux-gnu", cur->path, target);
    else
      snprintf(cmd, 2*MAX_PATH_LEN-1, "cp -f %s %s", cur->path, target);
    if (system(cmd))
      fprintf(debug2, "ERROR: %s\n", cmd);
    else
      fprintf(debug2, "OK: %s\n", cmd);
    cur = cur->next;
    }
}

static t_lib_info *find_lib_all(char *name)
{
  t_lib_info *cur = g_head_lib_all;
  while(cur)
    {
    if (!strcmp(name, cur->name))
      break;
    cur = cur->next;
    }
  return cur;
}

static void get_path_libs(FILE *dmp, FILE *debug1)
{
  char line[MAX_PATH_LEN];
  char name[MAX_PATH_LEN];
  char *ptr_line, *endp;

  memset(line, 0, MAX_PATH_LEN);
  memset(name, 0, MAX_PATH_LEN);
  while (fgets(line, MAX_PATH_LEN-1, dmp) != NULL)
    {
    ptr_line = line + strspn(line, " \r\n\t");
    if (!strncmp(ptr_line, "lib", 3))
      {
      strncpy(name, ptr_line, MAX_PATH_LEN-1);
      endp = strstr(name, ".so");
      if (endp)
        {
        endp = endp + strlen(".so");
        *endp = 0;
        ptr_line = strstr(ptr_line, "=>");
        if (ptr_line)
          {
          ptr_line += strlen("=>");
          ptr_line = ptr_line + strspn(line, " \r\n\t");
          if ((!strncmp(ptr_line, "/usr/libexec/cloonix/", 21)) ||
              (!strncmp(ptr_line, "/usr/lib", 8)) ||
              (!strncmp(ptr_line, "/lib", 4)))
            {
            endp = ptr_line + strcspn(ptr_line, " \r\n\t");
            if (endp)
              {
              *endp = 0;
              if (find_lib_all(name) == NULL)
                {
                alloc_lib_all(name, ptr_line);
                fprintf(debug1, "%s\n", name);
                }
              }
            }
          }
        }
      memset(name, 0, MAX_PATH_LEN);
      }
    memset(line, 0, MAX_PATH_LEN);
    }
}

static int init_globals(char *path)
{
  int result = -1;
  memset(g_rootfs, 0, MAX_PATH_LEN);
  memset(g_rootfs_lib, 0, MAX_PATH_LEN);
  memset(g_collect, 0, MAX_PATH_LEN);
  memset(g_names, 0, MAX_PATH_LEN);
  memset(g_paths, 0, MAX_PATH_LEN);
  memset(g_dumps, 0, MAX_PATH_LEN);
  strncpy(g_rootfs, path, MAX_PATH_LEN-1);
  strncpy(g_rootfs_lib, path, MAX_PATH_LEN-1);
  strcat(g_rootfs_lib, "/lib");
  snprintf(g_collect, MAX_PATH_LEN-1, "/tmp/ext_collect_rootfs.sh");
  snprintf(g_names, MAX_PATH_LEN-1, "/tmp/ext_names_rootfs.txt");
  snprintf(g_paths, MAX_PATH_LEN-1, "/tmp/ext_paths_rootfs.txt");
  snprintf(g_dumps, MAX_PATH_LEN-1, "/tmp/ext_dumps_rootfs.txt");
  unlink(g_collect);
  unlink(g_names);
  unlink(g_paths);
  unlink(g_dumps);
  if (access(g_rootfs_lib, W_OK))
    printf("ERROR LIB PATH: %s %s", path, g_rootfs_lib);
  else
    result = 0;
  return result;
}

int main(int argc, char *argv[])
{
  FILE *dmp, *debug1, *debug2;
  /* "chmod -R 755 " plus a path of up to MAX_PATH_LEN. */
  char cmd[2*MAX_PATH_LEN];
   
  g_head_lib_all = NULL;
  if (argc != 2)
    printf("ERROR Input path not given error\n");
  else if (access(argv[1], W_OK))
    printf("ERROR %s\n", argv[1]);
  else if (!init_globals(argv[1]))
    {
    if (!create_libs_txt())
      {
      dmp = fopen(g_dumps, "r");
      if (!dmp)
        printf("ERROR %s\n", g_dumps);
      else
        {
        debug1 = fopen(g_names, "w+");
        get_path_libs(dmp, debug1);
        fclose(dmp);
        fclose(debug1);
        debug2 = fopen(g_paths, "w+");
        copy_lib_all(g_rootfs_lib, debug2);
        fclose(debug2);
        snprintf(cmd, sizeof(cmd), "chmod -R 755 %s", g_rootfs_lib);
        system(cmd);
        }
      }
    }
  return 0;
}

