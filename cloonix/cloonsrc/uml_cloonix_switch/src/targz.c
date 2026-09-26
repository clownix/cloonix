/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* A cnt image given as bulk/<image>.tar.gz when bulk/<image>/ is absent.   */
/* Extracted once into bulk/<image>_targz, shared by every net; the cnt is  */
/* created on that directory. Never persistent, by its name. The tree stays */
/* what the tar.gz holds: its modes are the image's (tar -p), so they do   */
/* not protect it; suid_power mounts it read only under the overlay.        */
/* Top writable: a plain rm -rf removes it.                                 */
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "pthexec.h"
#include "cfg_store.h"
#include "pid_clone.h"
#include "uml_clownix_switch.h"
#include "targz.h"

#define TARGZ_SUFFIX "_targz"
/* Marker at the top of an extracted tree; a tree without the current one is redone. */
#define TARGZ_MARKER ".cloonix_targz_3"

typedef struct t_pending
{
  int llid;
  int tid;
  t_topo_cnt cnt;
  struct t_pending *next;
} t_pending;

typedef struct t_job
{
  char image[MAX_NAME_LEN];
  char bulk[MAX_PATH_LEN];
  t_pending *head;
  struct t_job *next;
} t_job;

static t_job *g_head;

/* Host-side uid of this process (toy: uid 0 of a userns), via /proc/self/uid_map. */
static int host_uid(void)
{
  unsigned int in, out, len, me = getuid();
  char line[128];
  FILE *fp = fopen("/proc/self/uid_map", "r");
  if (fp)
    {
    while (fgets(line, sizeof(line), fp))
      {
      if ((sscanf(line, "%u %u %u", &in, &out, &len) == 3) &&
          (me >= in) && (me < in + len))
        {
        fclose(fp);
        return (int) (out + (me - in));
        }
      }
    fclose(fp);
    }
  return (int) me;
}

static int is_dir(char *path)
{
  struct stat sb;
  return ((stat(path, &sb) == 0) && S_ISDIR(sb.st_mode));
}

static int ends_with(char *name, char *suffix)
{
  int ln = strlen(name), ls = strlen(suffix);
  return ((ln > ls) && (!strcmp(name + ln - ls, suffix)));
}

/* The bulk holding <image>.tar.gz, when no bulk holds <image>/. */
static char *targz_bulk(char *image)
{
  static char bulk[MAX_PATH_LEN];
  char path[MAX_PATH_LEN];
  char *cand[2];
  int i, nb = 1;
  cand[0] = cfg_get_bulk();
  if (get_running_in_crun())
    cand[nb++] = cfg_get_bulk_host();
  for (i=0; i<nb; i++)
    {
    snprintf(path, MAX_PATH_LEN-1, "%s/%s", cand[i], image);
    if (is_dir(path))
      return NULL;
    }
  for (i=0; i<nb; i++)
    {
    snprintf(path, MAX_PATH_LEN-1, "%s/%s.tar.gz", cand[i], image);
    if (access(path, R_OK) == 0)
      {
      strncpy(bulk, cand[i], MAX_PATH_LEN-1);
      return bulk;
      }
    }
  return NULL;
}

/* The directory of the image, in whichever bulk, has no write bit for its */
/* owner: a tree chmoded read only by its owner is not persistent.         */
static int dir_read_only(char *image, char *found)
{
  struct stat sb;
  char path[MAX_PATH_LEN];
  char *cand[2];
  int i, nb = 1;
  cand[0] = cfg_get_bulk();
  if (get_running_in_crun())
    cand[nb++] = cfg_get_bulk_host();
  for (i=0; i<nb; i++)
    {
    snprintf(path, MAX_PATH_LEN-1, "%s/%s", cand[i], image);
    if ((stat(path, &sb) == 0) && S_ISDIR(sb.st_mode))
      {
      strncpy(found, path, MAX_PATH_LEN-1);
      return ((sb.st_mode & S_IWUSR) == 0);
      }
    }
  return 0;
}

static int rw_dir_cb(const char *path, const struct stat *sb, int flag,
                     struct FTW *ftw)
{
  if ((flag == FTW_D) || (flag == FTW_DNR))
    chmod(path, 0700);
  return 0;
}

static int rm_cb(const char *path, const struct stat *sb, int flag,
                 struct FTW *ftw)
{
  if (flag == FTW_DP)
    rmdir(path);
  else if (flag != FTW_D)
    unlink(path);
  return 0;
}

/* Directories without a write bit are made writable first. */
static void rm_tree(char *path)
{
  nftw(path, rw_dir_cb, 16, FTW_PHYS);
  nftw(path, rm_cb, 16, FTW_DEPTH|FTW_PHYS);
}

static int tree_is_current(char *dir)
{
  char marker[MAX_PATH_LEN];
  struct stat sb;
  if (!is_dir(dir))
    return 0;
  snprintf(marker, MAX_PATH_LEN-1, "%s/%s", dir, TARGZ_MARKER);
  return (stat(marker, &sb) == 0);
}

static int run_tar(char *targz, char *dir, char *log)
{
  int status, fd;
  pid_t pid = fork();
  if (pid < 0)
    {
    KERR("ERROR %s: fork: %s", targz, strerror(errno));
    return -1;
    }
  if (pid == 0)
    {
    fd = open(log, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (fd >= 0)
      {
      dup2(fd, 2);
      close(fd);
      }
    execl(pthexec_tar_bin(), pthexec_tar_bin(),
          "--use-compress-program", pthexec_gzip_bin(),
          "--no-same-owner", "-p", "-xf", targz, "-C", dir, (char *) NULL);
    _exit(127);
    }
  if (waitpid(pid, &status, 0) != pid)
    return -1;
  return (WIFEXITED(status) && (WEXITSTATUS(status) == 0)) ? 0 : -1;
}

/* In the clone. The lock serialises every net of the machine on one tar.  */
static int extract_clone(void *data)
{
  t_job *job = (t_job *) data;
  char targz[MAX_PATH_LEN], dir[MAX_PATH_LEN], tmp[MAX_PATH_LEN];
  char lock[MAX_PATH_LEN], log[MAX_PATH_LEN], marker[MAX_PATH_LEN];
  int fd, fd_marker, result = -1;
  snprintf(targz, MAX_PATH_LEN-1, "%s/%s.tar.gz", job->bulk, job->image);
  snprintf(dir, MAX_PATH_LEN-1, "%s/%s%s", job->bulk, job->image,
           TARGZ_SUFFIX);
  /* Per uid: a leftover of another user's killed net is not ours to erase. */
  snprintf(tmp, MAX_PATH_LEN-1, "%s/.%s%s.%d.tmp", job->bulk, job->image,
           TARGZ_SUFFIX, host_uid());
  snprintf(lock, MAX_PATH_LEN-1, "%s/.%s%s.lock", job->bulk, job->image,
           TARGZ_SUFFIX);
  snprintf(log, MAX_PATH_LEN-1, "%s.%d.log", dir, host_uid());
  fd = open(lock, O_RDWR|O_CREAT, 0666);
  if (fd >= 0)
    chmod(lock, 0666);
  else
    fd = open(lock, O_RDONLY);
  if ((fd < 0) || (flock(fd, LOCK_EX)))
    {
    KERR("ERROR %s: cannot lock: %s", lock, strerror(errno));
    if (fd >= 0)
      close(fd);
    return -1;
    }
  if (tree_is_current(dir))
    result = 0;
  else
    {
    if (is_dir(dir))
      {
      KERR("WARNING %s: tree of an earlier extraction, erased and "
           "extracted again", dir);
      rm_tree(dir);
      }
    rm_tree(tmp);
    if (mkdir(tmp, 0755))
      result = -1;
    else if (run_tar(targz, tmp, log))
      rm_tree(tmp);
    else
      {
      /* tar -p gave the top the mode of "./" in the archive. */
      chmod(tmp, 0755);
      snprintf(marker, MAX_PATH_LEN-1, "%s/%s", tmp, TARGZ_MARKER);
      fd_marker = open(marker, O_WRONLY|O_CREAT|O_TRUNC, 0644);
      if (fd_marker >= 0)
        close(fd_marker);
      else
        KERR("ERROR %s: %s, the tree will be extracted at every add",
             marker, strerror(errno));
      if (rename(tmp, dir) == 0)
        {
        unlink(log);
        result = 0;
        }
      else
        {
        KERR("ERROR %s: could not be put in place: %s (a tree of another "
             "user in the way? erase it on the host)", dir, strerror(errno));
        rm_tree(tmp);
        }
      }
    }
  flock(fd, LOCK_UN);
  close(fd);
  return result;
}

static void extract_death(void *data, int status, char *nm)
{
  t_job *job = (t_job *) data, *cur, *prev = NULL;
  t_pending *p, *next;
  char err[MAX_PATH_LEN];
  for (cur = g_head; cur && (cur != job); prev = cur, cur = cur->next);
  if (cur == NULL)
    KOUT("ERROR targz job %s", job->image);
  if (prev)
    prev->next = job->next;
  else
    g_head = job->next;
  if (status)
    KERR("ERROR %s/%s.tar.gz: extraction failed, see %s/%s%s.%d.log",
         job->bulk, job->image, job->bulk, job->image, TARGZ_SUFFIX,
         host_uid());
  for (p = job->head; p; p = next)
    {
    next = p->next;
    if (!msg_exist_channel(p->llid))
      KERR("WARNING %s: client gone during the extraction", p->cnt.name);
    else if (status)
      {
      snprintf(err, MAX_PATH_LEN-1,
               "%s.tar.gz could not be extracted into %s/%s%s, "
               "see %s/%s%s.%d.log", job->image, job->bulk, job->image,
               TARGZ_SUFFIX, job->bulk, job->image, TARGZ_SUFFIX, host_uid());
      send_status_ko(p->llid, p->tid, err);
      }
    else
      recv_cnt_add(p->llid, p->tid, &(p->cnt));
    clownix_free(p, __FUNCTION__);
    }
  clownix_free(job, __FUNCTION__);
}

static void park(t_job *job, int llid, int tid, t_topo_cnt *cnt)
{
  t_pending *p, *last;
  p = (t_pending *) clownix_malloc(sizeof(t_pending), 7);
  memset(p, 0, sizeof(t_pending));
  p->llid = llid;
  p->tid = tid;
  memcpy(&(p->cnt), cnt, sizeof(t_topo_cnt));
  for (last = job->head; last && last->next; last = last->next);
  if (last)
    last->next = p;
  else
    job->head = p;
}

int targz_resolve(int llid, int tid, t_topo_cnt *cnt, char *err)
{
  char dir[MAX_PATH_LEN];
  char *bulk;
  t_job *job;
  if (ends_with(cnt->image, TARGZ_SUFFIX))
    {
    if (cnt->is_persistent)
      {
      snprintf(err, MAX_PATH_LEN-1, "%s comes from a tar.gz and is read "
               "only: --persistent needs the directory form, untar the "
               "tar.gz yourself in %s", cnt->image, cfg_get_bulk());
      return -1;
      }
    return 0;
    }
  bulk = targz_bulk(cnt->image);
  if (bulk == NULL)
    {
    if (cnt->is_persistent && dir_read_only(cnt->image, dir))
      {
      snprintf(err, MAX_PATH_LEN-1, "%s is read only, no write bit for its "
               "owner on %s: --persistent needs a writable directory",
               cnt->image, dir);
      return -1;
      }
    return 0;
    }
  if (cnt->is_persistent)
    {
    snprintf(err, MAX_PATH_LEN-1, "%s is in %s as a tar.gz, read only: "
             "--persistent needs the directory form, untar %s.tar.gz "
             "yourself there", cnt->image, bulk, cnt->image);
    return -1;
    }
  snprintf(dir, MAX_PATH_LEN-1, "%s/%s%s", bulk, cnt->image, TARGZ_SUFFIX);
  if (tree_is_current(dir))
    {
    /* image is MAX_PATH_LEN: the room left, never a negative turned huge. */
    if (strlen(cnt->image) + strlen(TARGZ_SUFFIX) < MAX_PATH_LEN)
      strncat(cnt->image, TARGZ_SUFFIX,
              MAX_PATH_LEN - 1 - strlen(cnt->image));
    return 0;
    }
  for (job = g_head; job; job = job->next)
    if (!strcmp(job->image, cnt->image))
      break;
  if (job == NULL)
    {
    job = (t_job *) clownix_malloc(sizeof(t_job), 7);
    memset(job, 0, sizeof(t_job));
    strncpy(job->image, cnt->image, MAX_NAME_LEN-1);
    strncpy(job->bulk, bulk, MAX_PATH_LEN-1);
    job->next = g_head;
    g_head = job;
    pid_clone_launch(extract_clone, extract_death, NULL, (void *) job,
                     (void *) job, NULL, cnt->name, -1, 1);
    }
  park(job, llid, tid, cnt);
  return 1;
}

int targz_is_extracted_name(char *name)
{
  return ends_with(name, TARGZ_SUFFIX);
}

void targz_init(void)
{
  g_head = NULL;
}
