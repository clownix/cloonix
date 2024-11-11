/*****************************************************************************/
/*    Copyright (C) 2006-2024 clownix@clownix.net License AGPL-3             */
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

#define MAX_PATH_LEN 300
#define MAX_NAME_LEN 100

#define LIBDIR "/usr/libexec/cloonix/common/wireshark/lib"
#define DUMPCAP "/usr/libexec/cloonix/server/cloonix-dumpcap"
#define WIRESHARK "/usr/libexec/cloonix/server/cloonix-wireshark"

static char g_collect1[MAX_PATH_LEN];
static char g_dumps1[MAX_PATH_LEN];
static char g_collect2[MAX_PATH_LEN];
static char g_dumps2[MAX_PATH_LEN];
static char g_debug1[MAX_PATH_LEN];
static char g_debug2[MAX_PATH_LEN];
static char g_debug3[MAX_PATH_LEN];
static char g_debug4[MAX_PATH_LEN];

typedef struct t_lib_info
{
  char name[MAX_NAME_LEN];
  char path[MAX_PATH_LEN];
  struct t_lib_info *prev;
  struct t_lib_info *next;
} t_lib_info;

static t_lib_info *g_head_lib_all;


/****************************************************************************/
static void free_lib_all(void)
{
  t_lib_info *cur, *next;
  cur = g_head_lib_all;
  while(cur)
    {
    next = cur->next;
    free(cur); 
    cur = next;
    }
  g_head_lib_all = NULL;
}   
/*--------------------------------------------------------------------------*/

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
static void copy_lib_all(char *target, FILE *debug2)
{
  char cmd[2*MAX_PATH_LEN];
  t_lib_info *cur = g_head_lib_all;
  while(cur)
    {
    memset(cmd, 0, 2*MAX_PATH_LEN);
    snprintf(cmd, 2*MAX_PATH_LEN-1, "cp -f %s %s", cur->path, target);
    if (system(cmd))
      fprintf(debug2, "ERROR: %s\n", cmd);
    else
      fprintf(debug2, "OK: %s\n", cmd);
    cur = cur->next;
    }
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
                fprintf(debug1, "%s %s\n", name, ptr_line);
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
int main(int argc, char *argv[])
{
  FILE *col, *dmp, *debug1, *debug2, *debug3, *debug4;
  char cmd[MAX_PATH_LEN];
  char *path;
   
  g_head_lib_all = NULL;
  snprintf(g_collect1, MAX_PATH_LEN-1, "/root/wire_libs_collect1.sh");
  snprintf(g_dumps1,   MAX_PATH_LEN-1, "/root/wire_libs_dumps1.txt");
  snprintf(g_collect2, MAX_PATH_LEN-1, "/root/wire_libs_collect2.sh");
  snprintf(g_dumps2,   MAX_PATH_LEN-1, "/root/wire_libs_dumps2.txt");
  snprintf(g_debug1,   MAX_PATH_LEN-1, "/root/wire_libs_debug1.txt");
  snprintf(g_debug2,   MAX_PATH_LEN-1, "/root/wire_libs_debug2.txt");
  snprintf(g_debug3,   MAX_PATH_LEN-1, "/root/wire_libs_debug3.txt");
  snprintf(g_debug4,   MAX_PATH_LEN-1, "/root/wire_libs_debug4.txt");
  unlink(g_collect1);
  unlink(g_dumps1);
  unlink(g_collect2);
  unlink(g_dumps2);
  unlink(g_debug1);
  unlink(g_debug2);
  unlink(g_debug3);
  unlink(g_debug4);

  col = fopen(g_collect1, "w+");
  if (col == NULL)
    {
    printf("MAIN1 %s ERROR\n", g_collect1);
    return -1;
    }
  fprintf(col, "#!/bin/bash\n");
  fprintf(col, "echo " " > %s\n", g_dumps1);
  fprintf(col, "ldd -v %s | grep -v ld-linux-x86-64.so >> %s 2>/dev/null\n", DUMPCAP, g_dumps1);
  fprintf(col, "ldd -v %s | grep -v ld-linux-x86-64.so >> %s 2>/dev/null\n", WIRESHARK, g_dumps1);
  fprintf(col, "echo " " >> %s\n", g_dumps1);
  fclose(col);
  if (chmod(g_collect1, 0700))
    {
    printf("MAIN2 %s %d ERR chmod\n", g_collect1, errno);
    return -1;
    }
  if (system(g_collect1))
    {
    printf("MAIN3 %s %d ERR system\n", col, errno);
    return -1;
    }
  dmp = fopen(g_dumps1, "r");
  if (!dmp)
    {
    printf("MAIN4 3File: %s error\n", g_dumps1);
    return -1;
    }
  debug1 = fopen(g_debug1, "w+");
  get_path_libs(dmp, debug1);
  fclose(dmp);
  fclose(debug1);
  debug2 = fopen(g_debug2, "w+");
  copy_lib_all(LIBDIR, debug2);
  fclose(debug2);
  free_lib_all();
  col = fopen(g_collect2, "w+");
  if (col == NULL)
    {
    printf("MAIN5 %s ERROR\n", g_collect2);
    return -1;
    }
  fprintf(col, "#!/bin/bash\n");
  fprintf(col, "echo " " > %s\n", g_dumps2);
  fprintf(col, "for i in `find %s -maxdepth 1 -type f` ; do\n", LIBDIR);
  fprintf(col, "  ldd -v ${i} | grep -v ld-linux-x86-64.so >> %s 2>/dev/null\n", g_dumps2);
  fprintf(col, "done\n");
  fprintf(col, "for i in `find %s/qt6/plugins/platforms -maxdepth 1 -type f` ; do\n", LIBDIR);
  fprintf(col, "  ldd -v ${i} | grep -v ld-linux-x86-64.so >> %s 2>/dev/null\n", g_dumps2);
  fprintf(col, "done\n");
  fprintf(col, "echo " " >> %s\n", g_dumps2);
  fclose(col);
  if (chmod(g_collect2, 0700))
    {
    printf("MAIN6 %s %d ERR chmod\n", g_collect2, errno);
    return -1;
    }
  if (system(g_collect2))
    {
    printf("MAIN7 %s %d ERR system\n", col, errno);
    return -1;
    }
  dmp = fopen(g_dumps2, "r");
  if (!dmp)
    {
    printf("MAIN8 3File: %s error\n", g_dumps2);
    return -1;
    }
  debug3 = fopen(g_debug3, "w+");
  get_path_libs(dmp, debug3);
  fclose(dmp);
  fclose(debug3);
  debug4 = fopen(g_debug4, "w+");
  copy_lib_all(LIBDIR, debug4);
  fclose(debug4);
  return 0;
}
/*--------------------------------------------------------------------------*/

