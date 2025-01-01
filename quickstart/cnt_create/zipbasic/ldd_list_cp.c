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
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <libgen.h>


#define MAX_PATH_LEN 300
#define MAX_NAME_LEN 100

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



/****************************************************************************/
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
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int create_libs_txt(char *path)
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

    fprintf(col, "for i in `find %s -maxdepth 1 -type f` ; do\n", path);
    fprintf(col, "  ldd -v ${i} >> %s 2>/dev/null\n", g_dumps);
    fprintf(col, "done\n");

    fprintf(col, "echo " " >> %s\n", g_dumps);
    fclose(col);
    if (chmod(g_collect, 0700))
      printf("%s %d ERR chmod\n", g_collect, errno);
    else if (system(g_collect))
      printf("%s %d ERR system\n", col, errno);
    else
      result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
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
/*--------------------------------------------------------------------------*/

/****************************************************************************/
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
          if ((!strncmp(ptr_line, "/usr/lib", 8)) ||
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
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int file_exists(char *path)
{
  int err, result = 0;
  err = access(path, F_OK);
  if (!err)
    result = 1;
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void copy_dependancies(char *target)
{
  char *duppath, *dirpath;
  t_lib_info *cur;
  char complete_target[MAX_PATH_LEN];
  char cmd[MAX_PATH_LEN];
  cur = g_head_lib_all;
  while(cur)
    {
    duppath =  strdup(cur->path);
    dirpath = dirname(duppath);
    memset(complete_target, 0, MAX_PATH_LEN);
    snprintf(complete_target, MAX_PATH_LEN-1, "%s%s", target, dirpath);
    memset(cmd, 0, MAX_PATH_LEN);
    snprintf(cmd, MAX_PATH_LEN-1, "mkdir -p %s", complete_target);
    system(cmd);
    memset(cmd, 0, MAX_PATH_LEN);
    snprintf(cmd, MAX_PATH_LEN-1, "cp -f %s %s", cur->path, complete_target);
    printf("%s\n", cmd);
    system(cmd);
    free(duppath);
    cur = cur->next;
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int main(int argc, char *argv[])
{
  FILE *dmp, *debug1;
  char cmd[MAX_PATH_LEN];
  char *path, *target;
   
  g_head_lib_all = NULL;
  if (argc != 3)
    printf("Input path not given error\n");
  else if (access(argv[1], W_OK))
    printf("1File: %s error\n", argv[1]);
  else if (access(argv[2], W_OK))
    printf("2File: %s error\n", argv[2]);
  else
    {
    snprintf(g_collect, MAX_PATH_LEN-1, "/tmp/ext_collect.sh");
    snprintf(g_names,   MAX_PATH_LEN-1, "/tmp/ext_names.txt");
    snprintf(g_paths,   MAX_PATH_LEN-1, "/tmp/ext_paths.txt");
    snprintf(g_dumps,   MAX_PATH_LEN-1, "/tmp/ext_dumps.txt");
    unlink(g_collect);
    unlink(g_names);
    unlink(g_paths);
    unlink(g_dumps);
    path = argv[1];
    target = argv[2];
    if (!create_libs_txt(path))
      {
      dmp = fopen(g_dumps, "r");
      if (!dmp)
        printf("3File: %s error\n", g_dumps);
      else
        {
        debug1 = fopen(g_names, "w+");
        get_path_libs(dmp, debug1);
        fclose(dmp);
        fclose(debug1);
        copy_dependancies(target);
        }
      }
    }
  return 0;
}
/*--------------------------------------------------------------------------*/

