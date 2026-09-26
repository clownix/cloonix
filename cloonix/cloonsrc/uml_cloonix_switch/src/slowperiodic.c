/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include "io_clownix.h"
#include "commun_daemon.h"
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "event_subscriber.h"
#include "llid_trace.h"
#include "utils_cmd_line_maker.h"
#include "targz.h"

static int ends_with_suffix(char *str, char *suffix)
{
  int result = 0;
  size_t lenstr, lensuffix;
  if (!str || !suffix)
    KOUT(" ");
  lenstr = strlen(str);
  lensuffix = strlen(suffix);
  if ((lenstr==0) || (lensuffix==0))
    KOUT("ERROR %lu %lu", lenstr, lensuffix);
  if (lensuffix < lenstr)
    result = (strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0);
  return result;
}

static int is_duplicate(t_slowperiodic *slowperiodic, char *name, int max)
{
  int i, result = 0;
  for (i=0; i<max; i++)
    {
    if (!strcmp(slowperiodic[i].name, name))
      result = 1;
    }
  return result;
}

/* A cnt rootfs is a directory with the shell crun starts; no init needed, */
/* cloonixsbininit is the host copy. Anything else under bulk is skipped.   */
static int has_cnt_rootfs(char *bulk, char *image)
{
  int i, result = 0;
  struct stat sb;
  char target[MAX_PATH_LEN];
  char *shell[2] = {"bin/sh", "usr/bin/sh"};
  if (!strcmp(image, "self_rootfs"))
    return 1;
  for (i=0; i<2; i++)
    {
    memset(target, 0, MAX_PATH_LEN);
    snprintf(target, MAX_PATH_LEN-1, "%s/%s/%s", bulk, image, shell[i]);
    if (lstat(target, &sb))
      continue;
    if (((sb.st_mode & S_IFMT) == S_IFLNK) ||
        ((sb.st_mode & S_IFMT) == S_IFREG))
      {
      result = 1;
      break;
      }
    KERR("ERROR BAD SHELL %s/%s/%s", bulk, image, shell[i]);
    }
  if (result == 0)
    KTRC("%s/%s has no /bin/sh, not a container rootfs", bulk, image);
  return result;
}

static int entry_is_reg(char *dir, struct dirent *ent)
{
  struct stat sb;
  char full[MAX_PATH_LEN];
  if (ent->d_type == DT_REG)
    return 1;
  if (ent->d_type != DT_UNKNOWN)
    return 0;
  snprintf(full, MAX_PATH_LEN-1, "%s/%s", dir, ent->d_name);
  if (lstat(full, &sb) != 0)
    return 0;
  return S_ISREG(sb.st_mode) ? 1 : 0;
}

static int entry_is_dir(char *dir, struct dirent *ent)
{
  struct stat sb;
  char full[MAX_PATH_LEN];
  if (ent->d_type == DT_DIR)
    return 1;
  if (ent->d_type != DT_UNKNOWN)
    return 0;
  snprintf(full, MAX_PATH_LEN-1, "%s/%s", dir, ent->d_name);
  if (lstat(full, &sb) != 0)
    return 0;
  return S_ISDIR(sb.st_mode) ? 1 : 0;
}

static void helper_get_cnt(char *path,
                           t_slowperiodic **slowperiodic,
                           int *nb, int max)
{
  DIR *dirptr;
  struct dirent *ent;
  dirptr = opendir(path);
  if (dirptr != NULL)
    {
    while ((ent = readdir(dirptr)) != NULL)
      {
      if (strlen(ent->d_name) == 0)
        {
        KERR("ERROR %s", path);
        continue;
        }
      if (ent->d_name[0] == '.')
        continue;
      if (entry_is_dir(path, ent))
        {
        if (has_cnt_rootfs(path, ent->d_name))
          {
          if (!is_duplicate(*slowperiodic, ent->d_name, *nb))
            {
            strncpy((*slowperiodic)[*nb].name, ent->d_name, MAX_NAME_LEN-1);
            *nb += 1;
            }
          }
        }
      if (*nb >= max)
        break;
      }
    if (closedir(dirptr))
      KOUT("%d", errno);
    }
}

static int get_bulk_files_cnt(t_slowperiodic **slowperiodic)
{ 
  int max = (MAX_BULK_FILES-1), nb = 0, len = max * sizeof(t_slowperiodic);
  *slowperiodic=(t_slowperiodic *)clownix_malloc(len, 39);
  memset(*slowperiodic, 0, len);
  helper_get_cnt(cfg_get_bulk(), slowperiodic, &nb, max);
  helper_get_cnt(cfg_get_bulk_host(), slowperiodic, &nb, max);
  return nb;
}

static void helper_get(char *path,
                       t_slowperiodic **slowperiodic,
                       char *suffix, int *nb, int max)
{
  DIR *dirptr;
  struct dirent *ent;
  dirptr = opendir(path);
  if (dirptr != NULL)
    {
    while ((ent = readdir(dirptr)) != NULL)
      {
      if (strlen(ent->d_name) == 0)
        {
        KERR("ERROR %s %s", path, suffix);
        continue;
        }
      if (!strcmp(ent->d_name, "."))
        continue;
      if (!strcmp(ent->d_name, ".."))
        continue;
      if (entry_is_reg(path, ent))
        {
        if (ends_with_suffix(ent->d_name, suffix))
          {
          if (!is_duplicate(*slowperiodic, ent->d_name, *nb))
            {
            strncpy((*slowperiodic)[*nb].name, ent->d_name, MAX_NAME_LEN-1);
            *nb += 1;
            }
          }
        }
      if (*nb >= max)
        break;
      }
    if (closedir(dirptr))
      KOUT("%d", errno);
    }
}

static int get_bulk_files(t_slowperiodic **slowperiodic, char *suffix)
{
  int max = (MAX_BULK_FILES-1), nb = 0, len = max * sizeof(t_slowperiodic);
  *slowperiodic=(t_slowperiodic *)clownix_malloc(len, 39);
  memset(*slowperiodic, 0, len);
  helper_get(cfg_get_bulk(), slowperiodic, suffix, &nb, max);
  helper_get(cfg_get_bulk_host(), slowperiodic, suffix, &nb, max);
  return nb;
}

/* Owner of the tree: usr/ of a directory (its top may differ), the file for a qcow2. */
static void owner_of(char *bulk, char *name, int is_dir, char *owner)
{
  struct stat sb;
  struct passwd *pw;
  char path[MAX_PATH_LEN];
  int rc;
  memset(owner, 0, MAX_NAME_LEN);
  snprintf(path, MAX_PATH_LEN-1, "%s/%s/usr", bulk, name);
  rc = is_dir ? stat(path, &sb) : -1;
  if (rc)
    {
    snprintf(path, MAX_PATH_LEN-1, "%s/%s", bulk, name);
    rc = stat(path, &sb);
    }
  if (rc)
    {
    KERR("WARNING %s/%s: owner unreadable, %s", bulk, name, strerror(errno));
    strncpy(owner, "?", MAX_NAME_LEN-1);
    return;
    }
  pw = getpwuid(sb.st_uid);
  if (pw)
    strncpy(owner, pw->pw_name, MAX_NAME_LEN-1);
  else
    snprintf(owner, MAX_NAME_LEN-1, "uid%d", sb.st_uid);
}

static int owned_list(char *bulk, t_image_item *items, int nb, int max)
{
  t_slowperiodic *lst;
  int i, n, len = max * sizeof(t_slowperiodic);
  lst = (t_slowperiodic *) clownix_malloc(len, 39);
  memset(lst, 0, len);
  n = 0;
  helper_get_cnt(bulk, &lst, &n, max);
  for (i=0; (i<n) && (nb<max); i++, nb++)
    {
    strcpy(items[nb].kind, targz_is_extracted_name(lst[i].name) ?
                           "targz" : "cnt");
    strncpy(items[nb].name, lst[i].name, MAX_NAME_LEN-1);
    owner_of(bulk, lst[i].name, 1, items[nb].owner);
    }
  memset(lst, 0, len);
  n = 0;
  helper_get(bulk, &lst, ".tar.gz", &n, max);
  for (i=0; (i<n) && (nb<max); i++, nb++)
    {
    strcpy(items[nb].kind, "tar.gz");
    strncpy(items[nb].name, lst[i].name, MAX_NAME_LEN-1);
    owner_of(bulk, lst[i].name, 0, items[nb].owner);
    }
  memset(lst, 0, len);
  n = 0;
  helper_get(bulk, &lst, ".qcow2", &n, max);
  for (i=0; (i<n) && (nb<max); i++, nb++)
    {
    strcpy(items[nb].kind, "qcow2");
    strncpy(items[nb].name, lst[i].name, MAX_NAME_LEN-1);
    owner_of(bulk, lst[i].name, 0, items[nb].owner);
    }
  clownix_free(lst, __FUNCTION__);
  return nb;
}

/* Everything add can take, with its owner: bulk then bulk_host. */
int slowperiodic_images(t_image_item **items)
{
  int max = (MAX_BULK_FILES-1), nb;
  *items = (t_image_item *) clownix_malloc(max * sizeof(t_image_item), 39);
  memset(*items, 0, max * sizeof(t_image_item));
  nb = owned_list(cfg_get_bulk(), *items, 0, max);
  if (strcmp(cfg_get_bulk(), cfg_get_bulk_host()))
    nb = owned_list(cfg_get_bulk_host(), *items, nb, max);
  return nb;
}

static void action_send_slowperiodic(void)
{
  int llid, tid, prev_llid = 0, nb_qcow2, nb_cnt;
  t_slowperiodic *slowperiodic_qcow2;
  t_slowperiodic *slowperiodic_cnt;
  nb_qcow2 = get_bulk_files(&slowperiodic_qcow2, ".qcow2");
  nb_cnt = get_bulk_files_cnt(&slowperiodic_cnt);
  while (llid_get_event2llid(sub_evt_slowperiodic, prev_llid, &llid, &tid))
    {
    prev_llid = llid;
    if (msg_exist_channel(llid))
      {
      if (nb_qcow2)
        send_slowperiodic_qcow2(llid, tid, nb_qcow2, slowperiodic_qcow2);
      if (nb_cnt)
        send_slowperiodic_cnt(llid, tid, nb_cnt, slowperiodic_cnt);
      }
    else
      KERR("WARNING slowperiodic subscriber llid %d has no channel", llid);
    }
  clownix_free(slowperiodic_qcow2, __FUNCTION__);
  clownix_free(slowperiodic_cnt, __FUNCTION__);
}

static void timeout_collect_slowperiodic(void *data)
{
  action_send_slowperiodic();
  clownix_timeout_add(1000, timeout_collect_slowperiodic, NULL, NULL, NULL);
}

void recv_slowperiodic_sub(int llid, int tid)
{
  event_subscribe(sub_evt_slowperiodic, llid, tid);
  action_send_slowperiodic();
}

void slowperiodic_init(void)
{
  clownix_timeout_add(1000, timeout_collect_slowperiodic, NULL, NULL, NULL);
}

