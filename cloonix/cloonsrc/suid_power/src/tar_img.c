/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include "io_clownix.h"
#include "config_json.h"
#include "crun.h"
#include "net_phy.h"
#include "crun_utils.h"
#include "utils.h"

struct t_elem;
typedef struct t_loop
{
  char bulktarget[MAX_PATH_LEN];
  char brandtype[MAX_NAME_LEN];
  int uid_image;
  int is_persistent;
  struct t_elem *head_elem;
  struct t_loop *prev;
  struct t_loop *next;
} t_loop;
typedef struct t_elem
{
  t_loop *loop;
  char name[MAX_NAME_LEN];
  struct t_elem *prev;
  struct t_elem *next;
} t_elem;

static t_loop *g_head_loop;

static int get_user_name(int uid_image, char *user_name)
{
  FILE *fh;
  char line[MAX_PATH_LEN];
  char no_use1[MAX_PATH_LEN];
  int i, uid, gid, found = 0, result = -1;
  char *ptr;
  
  fh = fopen("/etc/passwd", "r");
  if (fh)
    {
    while(fgets(line, MAX_PATH_LEN-1, fh))
      {
      for (i=0; i<4; i++)
        {
        ptr = strchr(line, ':');
        if (!ptr)
          KERR("ERROR %s", line);
        else
          *ptr = ' ';
        }
      if (sscanf(line, "%299s %299s %d %d ", user_name, no_use1, &uid, &gid) == 4)
        {
        if (uid == uid_image)
          {
          found = 1;
          result = 0;
          break;
          }
        }
      }
    fclose(fh);
    } 
  else
    KERR("ERROR READING /etc/passwd");
  if (!found)
    KERR("ERROR /etc/passwd does not have uid_image %d", uid_image);
  return result;
}

static int read_subid(char *file, char *user_name, int *start, int *qty)
{
  int i, found = 0;
  char line[MAX_PATH_LEN];
  char name[MAX_PATH_LEN];
  FILE *fh = fopen(file, "r");
  char *ptr;
  if (fh)
    {
    while(fgets(line, MAX_PATH_LEN-1, fh))
      {
      for (i=0; i<2; i++)
        {
        ptr = strchr(line, ':');
        if (ptr)
          *ptr = ' ';
        }
      if ((sscanf(line, "%299s %d %d", name, start, qty) == 3) &&
          (!strcmp(name, user_name)))
        {
        found = 1;
        break;
        }
      }
    fclose(fh);
    }
  return found;
}

#define SUBID_FIRST 100000
#define SUBID_QTY   65536
static int add_subid(char *file, char *user_name, int *start, int *qty)
{
  int i, s, q, top = SUBID_FIRST;
  char line[MAX_PATH_LEN];
  char name[MAX_PATH_LEN];
  FILE *fh = fopen(file, "r");
  char *ptr;
  if (fh)
    {
    while(fgets(line, MAX_PATH_LEN-1, fh))
      {
      for (i=0; i<2; i++)
        {
        ptr = strchr(line, ':');
        if (ptr)
          *ptr = ' ';
        }
      if ((sscanf(line, "%299s %d %d", name, &s, &q) == 3) && (s + q > top))
        top = s + q;
      }
    fclose(fh);
    }
  fh = fopen(file, "a");
  if (fh == NULL)
    {
    KERR("ERROR cannot write %s to give %s a range", file, user_name);
    return -1;
    }
  fprintf(fh, "%s:%d:%d\n", user_name, top, SUBID_QTY);
  fclose(fh);
  KERR("WARNING %s had no range in %s, added %s:%d:%d",
       user_name, file, user_name, top, SUBID_QTY);
  *start = top;
  *qty = SUBID_QTY;
  return 0;
}

/* Follows the links of path inside root, an absolute target stays in root. */
static int rootfs_resolve(char *root, char *path, char *out, struct stat *sb)
{
  int hops = 0, len;
  char cur[MAX_PATH_LEN], done[MAX_PATH_LEN], link[MAX_PATH_LEN];
  char next[2*MAX_PATH_LEN];
  char *rest, *comp, *slash;
  memset(cur, 0, MAX_PATH_LEN);
  strncpy(cur, path, MAX_PATH_LEN-1);
  while (1)
    {
    memset(done, 0, MAX_PATH_LEN);
    rest = cur;
    while ((comp = strsep(&rest, "/")) != NULL)
      {
      if ((comp[0] == 0) || (!strcmp(comp, ".")))
        continue;
      if (!strcmp(comp, ".."))
        {
        slash = strrchr(done, '/');
        if (slash)
          *slash = 0;
        continue;
        }
      if (strlen(done) + strlen(comp) + 2 >= MAX_PATH_LEN)
        return -1;
      strcat(done, "/");
      strcat(done, comp);
      snprintf(out, MAX_PATH_LEN-1, "%s%s", root, done);
      if (lstat(out, sb))
        return -1;
      if ((sb->st_mode & S_IFMT) != S_IFLNK)
        continue;
      if (++hops > 40)
        return -1;
      len = readlink(out, link, MAX_PATH_LEN-2);
      if (len <= 0)
        return -1;
      link[len] = 0;
      if (link[0] != '/')
        {
        slash = strrchr(done, '/');
        *(slash+1) = 0;
        }
      else
        done[0] = 0;
      snprintf(next, 2*MAX_PATH_LEN-1, "%s%s/%s", done, link, rest ? rest : "");
      if (strlen(next) >= MAX_PATH_LEN)
        return -1;
      strcpy(cur, next);
      break;
      }
    if (comp == NULL)
      return 0;
    }
}

static unsigned long le_num(unsigned char *p, int nb)
{
  unsigned long v = 0;
  while (nb--)
    v = (v << 8) | p[nb];
  return v;
}

/* PT_INTERP of a little-endian elf: 1 found, 0 none, -1 not an elf. */
static int elf_interp(char *file, char *interp)
{
  unsigned char h[64], p[56];
  unsigned long phoff, off, sz;
  int i, phnum, phsz, rd, is64, result = 0;
  FILE *fh = fopen(file, "r");
  memset(interp, 0, MAX_PATH_LEN);
  if (fh == NULL)
    {
    if (errno != ENOENT)
      KERR("WARNING %s: %s", file, strerror(errno));
    return -1;
    }
  if ((fread(h, 1, 64, fh) != 64) || (memcmp(h, "\x7f" "ELF", 4)) ||
      (h[5] != 1))
    {
    fclose(fh);
    return -1;
    }
  is64 = (h[4] == 2);
  phoff = is64 ? le_num(h+32, 8) : le_num(h+28, 4);
  phsz  = is64 ? le_num(h+54, 2) : le_num(h+42, 2);
  phnum = is64 ? le_num(h+56, 2) : le_num(h+44, 2);
  rd = (phsz < 56) ? phsz : 56;
  for (i=0; (result == 0) && (i<phnum); i++)
    {
    if ((fseek(fh, phoff + i*phsz, SEEK_SET)) ||
        (fread(p, 1, rd, fh) != (size_t) rd))
      break;
    if (le_num(p, 4) != 3)
      continue;
    off = is64 ? le_num(p+8, 8)  : le_num(p+4, 4);
    sz  = is64 ? le_num(p+32, 8) : le_num(p+16, 4);
    if ((sz == 0) || (sz >= MAX_PATH_LEN) || (fseek(fh, off, SEEK_SET)) ||
        (fread(interp, 1, sz, fh) != sz))
      break;
    interp[sz] = 0;
    result = 1;
    }
  fclose(fh);
  return result;
}

/* The files a container needs to start, a copy without tar loses the links */
static int check_rootfs_start_files(char *root, char *image, char *why)
{
  static char *dirs[] = {"/usr", "/etc", "/bin", "/sbin", "/lib", "/lib64", NULL};
  struct stat sb;
  char path[MAX_PATH_LEN], resolved[MAX_PATH_LEN], interp[MAX_PATH_LEN];
  char *bad = NULL, *what = "";
  int i;
  for (i=0; (bad == NULL) && (dirs[i] != NULL); i++)
    {
    snprintf(path, MAX_PATH_LEN-1, "%s%s", root, dirs[i]);
    if (lstat(path, &sb))
      {
      if (i < 3)
        {
        bad = dirs[i];
        what = "is missing";
        }
      }
    else if ((sb.st_mode & S_IFMT) == S_IFREG)
      {
      bad = dirs[i];
      what = "is a plain file where a directory or a link is expected";
      }
    else if ((rootfs_resolve(root, dirs[i], resolved, &sb)) ||
             ((sb.st_mode & S_IFMT) != S_IFDIR))
      {
      bad = dirs[i];
      what = "is a link that leads to no directory";
      }
    }
  if (bad == NULL)
    {
    if ((rootfs_resolve(root, "/bin/sh", resolved, &sb)) ||
        ((sb.st_mode & S_IFMT) != S_IFREG))
      {
      bad = "/bin/sh";
      what = "leads to no file";
      }
    else if (!(sb.st_mode & S_IXUSR))
      {
      bad = "/bin/sh";
      what = "is not executable";
      }
    else if ((elf_interp(resolved, interp) == 1) &&
             ((rootfs_resolve(root, interp, resolved, &sb)) ||
              ((sb.st_mode & S_IFMT) != S_IFREG)))
      {
      bad = interp;
      what = "(the loader of /bin/sh) leads to no file";
      }
    }
  if (bad)
    {
    snprintf(why, MAX_PATH_LEN-1, "image %s: %s %s, the root file-system is "
             "damaged (copied without tar?), re-extract it from its tar.gz",
             image, bad, what);
    return -1;
    }
  return 0;
}

static int get_path_uid_image(char *bulktarget, int *uid_image,
                              int is_persistent,
                              int *subuid_start, int *subuid_qty,
                              int *subgid_start, int *subgid_qty,
                              char *why)
{
  struct stat statbuf;
  char user_name[MAX_PATH_LEN];
  int result = 0;
  *uid_image = (int) get_caller_uid();
  *subuid_start = 0;
  *subuid_qty = 0;
  *subgid_start = 0;
  *subgid_qty = 0;
  if (stat(bulktarget, &statbuf))
    {
    snprintf(why, MAX_PATH_LEN-1, "image %s: %s", bulktarget, strerror(errno));
    result = -1;
    }
  else if ((*uid_image) == 0)
    {
    if ((statbuf.st_uid != 0) && (is_persistent))
      {
      char *rel = strstr(bulktarget, "/bulk_host/");
      char host[MAX_PATH_LEN];
      if (rel)
        snprintf(host, MAX_PATH_LEN-1, "/var/lib/cloonix/bulk/%s", rel+11);
      else
        snprintf(host, MAX_PATH_LEN-1, "%s (its path on the host)", bulktarget);
      /* chown -R would clear the setuid bits: a copy by tar keeps them */
      snprintf(why, MAX_PATH_LEN-1, "image %s is owned by another user "
               "and this toy cannot write to it (persistent), a copy of "
               "your own on the host: d=%s; mkdir $d.mine && sudo tar -C $d "
               "-cf - . | tar -C $d.mine -xpf -", bulktarget, host);
      result = -1;
      }
    }
  else
    {
    if (get_user_name(*uid_image, user_name))
      result = -1;
    else
      {
      if ((!read_subid("/etc/subuid", user_name, subuid_start, subuid_qty)) &&
          (add_subid("/etc/subuid", user_name, subuid_start, subuid_qty)))
        result = -1;
      if ((!read_subid("/etc/subgid", user_name, subgid_start, subgid_qty)) &&
          (add_subid("/etc/subgid", user_name, subgid_start, subgid_qty)))
        result = -1;
      }
    }
  return result;
}

static t_loop *find_bulk_target(char *bulktarget, char *brandtype)
{
  t_loop *cur = g_head_loop;
  while(cur)
    {
    if ((!strcmp(cur->bulktarget, bulktarget)) &&
        (!strcmp(cur->brandtype, brandtype)))
      break;
    cur = cur->next;
    }
  return cur;
}

static t_elem *find_elem(char *name)
{
  t_elem *elem = NULL;
  t_loop *cur = g_head_loop;
  while(cur)
    {
    elem = cur->head_elem;
    while(elem)
      {
      if (!strcmp(elem->name, name))
        break;
      elem = elem->next;
      }
    if (elem)
      break;
    cur = cur->next;
    }
  return elem;
}

static t_loop *alloc_loop(char *bulktarget, int is_persistent,
                          char *brandtype, int uid_image)
{
  t_loop *cur = (t_loop *) malloc(sizeof(t_loop));
  memset(cur, 0, sizeof(t_loop));
  strncpy(cur->bulktarget, bulktarget, MAX_PATH_LEN-1);
  strncpy(cur->brandtype, brandtype, MAX_NAME_LEN-1);
  cur->is_persistent = is_persistent;
  cur->uid_image = uid_image;
  cur->next = g_head_loop;
  if (g_head_loop)
    g_head_loop->prev = cur;
  g_head_loop = cur; 
  return cur;
}

static void free_loop(t_loop *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_loop)
    g_head_loop = cur->next;
  free(cur);
}

static void alloc_loop_elem(t_loop *cur, char *name)
{
  t_elem *elem = (t_elem *) malloc(sizeof(t_elem));
  memset(elem, 0, sizeof(t_elem));
  strncpy(elem->name, name, MAX_NAME_LEN-1); 
  elem->loop = cur;
  elem->next = cur->head_elem; 
  if (cur->head_elem)
    cur->head_elem->prev = elem;
  cur->head_elem = elem;
}

static t_loop *free_loop_elem(t_elem *elem)
{
  t_loop *loop = elem->loop;
  if (elem->prev)
    elem->prev->next = elem->next;
  if (elem->next)
    elem->next->prev = elem->prev;
  if (elem == loop->head_elem)
    loop->head_elem = elem->next;
  free(elem);
  return loop;
}

int tar_img_add(char *name, char *bulk, char *image, int uid_image,
                char *cnt_dir, int is_persistent, char *brandtype)
{
  int result = -1;
  char bulktarget[MAX_PATH_LEN];
  t_loop *cur;
  t_elem *elem;
  memset(bulktarget, 0, MAX_PATH_LEN);
  snprintf(bulktarget, MAX_PATH_LEN-1, "%s/%s", bulk, image);
  cur = find_bulk_target(bulktarget, brandtype); 
  elem = find_elem(name);
  if ((!strcmp(image, "self_rootfs")) && (is_persistent))
    KERR("ERROR %s: self_rootfs cannot be persistent", name);
  else if (elem)
    KERR("ERROR %s: already registered on an image", name);
  else if (cur)
    {
    if (cur->is_persistent != is_persistent)
      KERR("ERROR %s: image %s is in use with persistent=%d, asked %d", name,
           bulktarget, cur->is_persistent, is_persistent);
    else if (is_persistent)
      KERR("ERROR %s: persistent image %s is already in use", name,
           bulktarget);
    else
      {
      alloc_loop_elem(cur, name);
      result = 0;
      }
    }
  else if (!strcmp(brandtype, "brandcnt"))
    {
    cur = alloc_loop(bulktarget, is_persistent,
                     brandtype, uid_image);
    alloc_loop_elem(cur, name);
    result = 0;
    }
  else
    KOUT("ERROR %s %s %s (only brandcnt)", name, bulk, image);
  return result;
}

int tar_img_del(char *name, char *bulk, char *image, char *cnt_dir,
                int is_persistent, char *brandtype)
{
  int result = -1;
  t_loop *loop;
  t_elem *elem = find_elem(name);

  if (elem == NULL)
    KTRC("%s: no image element to delete", name);
  else
    {
    result = 0;
    loop = free_loop_elem(elem);
    if (loop == NULL)
      KOUT("ERROR %s %s %s", name, bulk, image);
    if (strcmp(loop->brandtype, brandtype))
      KERR("ERROR %s %s %s", name, bulk, image);
    if (strcmp(loop->brandtype, "brandcnt"))
      KERR("ERROR %s %s %s (only brandcnt)", name, bulk, image);
    if (loop->is_persistent != is_persistent)
      KERR("ERROR %s %s %s %d %d", name, bulk, image,
            loop->is_persistent, is_persistent);
    if (loop->head_elem == NULL)
      {
      free_loop(loop);
      }
    }
  return result;
}

int tar_img_rootfs_get(char *name, char *bulk, char *image,
                       char *brandtype, char *mnt_tar_img)
{
  int result = -1;
  t_loop *cur;
  t_elem *elem;
  char bulktarget[MAX_PATH_LEN];
  memset(bulktarget, 0, MAX_PATH_LEN);
  memset(mnt_tar_img, 0, MAX_PATH_LEN);
  snprintf(bulktarget, MAX_PATH_LEN-1, "%s/%s", bulk, image);
  cur = find_bulk_target(bulktarget, brandtype); 
  elem = find_elem(name);
  if (cur == NULL)
    KERR("ERROR %s %s %s", name, bulk, image);
  else if (strcmp(cur->brandtype, brandtype))
    KERR("ERROR %s %s %s", name, bulk, image);
  else if (elem == NULL)
    KERR("ERROR %s %s %s", name, bulk, image);
  else if (!strcmp(cur->brandtype, "brandcnt"))
    { 
    snprintf(mnt_tar_img, MAX_PATH_LEN-1, "%s", bulktarget);
    result = 0;
    }
  else
    KERR("ERROR %s %s %s", name, bulk, image);
  return result;
}

int tar_img_check(char *bulk, char *image, int *uid_image, 
                  int is_persistent, char *brandtype,
                  char *sbin_init, char *sh_starter,
                  int *subuid_start, int *subuid_qty,
                  int *subgid_start, int *subgid_qty, char *why)
{
  int result = 0;
  char bulktarget[MAX_PATH_LEN];
  struct stat sb;
  t_loop *cur;
  memset(sbin_init, 0, MAX_PATH_LEN);
  memset(sh_starter, 0, MAX_PATH_LEN);
  memset(bulktarget, 0, MAX_PATH_LEN);
  memset(why, 0, MAX_PATH_LEN);
  snprintf(bulktarget, MAX_PATH_LEN-1, "%s/%s", bulk, image);
  cur = find_bulk_target(bulktarget, brandtype);
  if (get_path_uid_image(bulktarget, uid_image, is_persistent,
                         subuid_start, subuid_qty,
                         subgid_start, subgid_qty, why))
    result = -1;
  else if (!strcmp(brandtype, "brandcnt"))
    {
    strcpy(sh_starter, "/bin/sh");
    strcpy(sbin_init,"/cloonixmnt/cnf_fs/cloonixsbininit");
    /* Run as root, access() answers W_OK for anything: the mode bit of
       the owner is the test, a tree left read only is not persistent. */
    if ((access(bulktarget, R_OK)) ||
        (is_persistent && (stat(bulktarget, &sb) || !(sb.st_mode & S_IWUSR))))
      {
      snprintf(why, MAX_PATH_LEN-1, "image %s is not %s", bulktarget,
               is_persistent ? "writable, --persistent needs to write in it" :
               "readable");
      result = -1;
      }
    else if (check_rootfs_start_files(bulktarget, image, why))
      result = -1;
    }
  if ((result == 0) && (cur))
    {
    if (cur->is_persistent != is_persistent)
      {
      snprintf(why, MAX_PATH_LEN-1, "image %s is in use by a %s container: "
               "an image in use is either persistent or not, delete that "
               "container first", bulktarget,
               cur->is_persistent ? "--persistent" : "non persistent");
      result = -1;
      }
    else if (is_persistent)
      {
      snprintf(why, MAX_PATH_LEN-1, "image %s is in use by a --persistent "
               "container: a persistent image takes one container at a time",
               bulktarget);
      result = -1;
      }
    }
  return result;
}

void tar_img_init(void)
{
  g_head_loop = NULL;
}
