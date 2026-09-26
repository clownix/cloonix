/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>
#include <sys/wait.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "net_phy.h"
#include "crun_utils.h"
#include "utils.h"
#include "crun.h"
#include "tar_img.h"
#include "vwifi.h"
#include "pthexec.h"

int get_cloonix_rank(void);
char *get_net_name(void);

#define DELETE_MOUNT_RETRY_MAX 10

/* One entry per container. */
typedef struct t_crun
{
  char brandtype[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  char nspacecrun[MAX_PATH_LEN];
  char bulk[MAX_PATH_LEN];
  char image[MAX_NAME_LEN];
  char cnt_dir[MAX_PATH_LEN];
  char rootfs_path[MAX_PATH_LEN];
  char nspacecrun_path[2*MAX_PATH_LEN];
  char mountbear[MAX_PATH_LEN];
  char mounttmp[MAX_PATH_LEN];
  char sbin_init[MAX_PATH_LEN];
  char sh_starter[MAX_PATH_LEN];
  int uid_image;
  int nb_vwif;
  char mac_vwif[6];
  int nb_eth;
  t_eth_mac eth_mac[MAX_ETH_VM];
  int cloonix_rank;
  int vm_id;
  int crun_pid;
  int ovspid;
  int is_persistent;
  int is_privileged;
  int subuid_start;
  int subuid_qty;
  int subgid_start;
  int subgid_qty;
  int delete_first_finalization_countdown;
  int delete_second_finalization_countdown;
  int delete_third_finalization_countdown;
  int delete_mount_retry;
  int deleting;
  int pid_missing;
  /* the start in three loop turns: whom to answer, and the stage */
  int start_llid;
  int start_tid;
  int start_stage;
  struct t_crun *prev;
  struct t_crun *next;
} t_crun;

static t_crun *g_head_crun;


/* One line per container per create and teardown step. */
#define CRUN_TRACE 0

#if CRUN_TRACE
#define CLOG(fmt, ...) KERR("CRUNTRACE " fmt, ##__VA_ARGS__)
#else
#define CLOG(fmt, ...) do {} while(0)
#endif

static t_crun *find_crun(char *name)
{ 
  t_crun *cur = g_head_crun;
  while (cur)
    {
    if (!strcmp(cur->name, name))
      break;
    cur = cur->next;
    }
  return cur;
}

static char *random_str(void)
{
  static char str[8];
  int num, i;
  for (i=0; i<7; i++)
    {
    num = rand();
    num %= 26;
    str[i] = 'A'+ num;
    }
  str[7] = 0;
  return str;
}

static void alloc_crun(char *name, char *bulk, char *image, int uid_image,
                       int nb_vwif, unsigned char *mac_vwif,
                       int nb_eth, char *nspacecrun, int cloonix_rank,
                       int vm_id, char *cnt_dir,
                       int is_persistent, int is_privileged,
                       char *brandtype, char *sbin_init, char *sh_starter,
                       int subuid_start, int subuid_qty,
                       int subgid_start, int subgid_qty)
{
  int i;
  char *rnd = random_str();
  t_crun *cur = (t_crun *) malloc(sizeof(t_crun));
  memset(cur, 0, sizeof(t_crun));
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  strncpy(cur->bulk, bulk, MAX_PATH_LEN-1);
  strncpy(cur->image, image, MAX_NAME_LEN-1);
  strncpy(cur->sbin_init, sbin_init, MAX_PATH_LEN-1);
  strncpy(cur->sh_starter, sh_starter, MAX_PATH_LEN-1);
  cur->uid_image = uid_image;
  strncpy(cur->cnt_dir, cnt_dir, MAX_PATH_LEN-1);
  strncpy(cur->nspacecrun, nspacecrun, MAX_PATH_LEN-1);
  snprintf(cur->nspacecrun_path, 2*MAX_PATH_LEN-1, "%s%s",
           PATH_NAMESPACE, nspacecrun);
  snprintf(cur->mountbear, MAX_PATH_LEN-1,"%s/%s/mnt%s", cnt_dir, name, rnd);
  snprintf(cur->mounttmp, MAX_PATH_LEN-1,"%s/%s/tmp%s", cnt_dir, name, rnd);
  cur->nb_vwif = nb_vwif;
  for (i=0; i<6; i++)
    cur->mac_vwif[i] = mac_vwif[i];
  cur->nb_eth = nb_eth;
  cur->cloonix_rank = cloonix_rank;
  cur->is_persistent = is_persistent;
  cur->is_privileged = is_privileged;
  cur->subuid_start = subuid_start;
  cur->subuid_qty = subuid_qty;
  cur->subgid_start = subgid_start;
  cur->subgid_qty = subgid_qty;
  strncpy(cur->brandtype, brandtype, MAX_NAME_LEN-1);
  cur->vm_id = vm_id; 
  if (g_head_crun)
    g_head_crun->prev = cur;
  cur->next = g_head_crun;
  g_head_crun = cur;
}

static void free_crun(t_crun *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_crun)
    g_head_crun = cur->next;
  free(cur);
}

#define DELETE_SETTLE_BEATS 2

static void teardown_start(t_crun *cur)
{
  cur->deleting = 1;
  vwifi_radios_delete(cur->name);
  crun_utils_delete_crun_stop(cur->name, cur->crun_pid, cur->uid_image,
                              cur->is_privileged, cur->mountbear);
  cur->crun_pid = 0;
  cur->delete_first_finalization_countdown = DELETE_SETTLE_BEATS;
}

static void delete_first_finalization(t_crun *cur)
{   
  char cmd[MAX_PATH_LEN];
  CLOG("%s step 1: umount %s/tmp then crun delete", cur->name,
       cur->mountbear);
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
           "%s %s/tmp", pthexec_umount_bin(), cur->mountbear);
  execute_cmd(cmd, 0, 0);
  crun_utils_delete_crun_delete(cur->name, cur->nspacecrun,
                                cur->vm_id, cur->nb_eth,
                                cur->uid_image, cur->is_privileged);
  cur->delete_second_finalization_countdown = 1;
}

static void delete_second_finalization(t_crun *cur)
{
  CLOG("%s step 2: overlay1 (persistent=%d)", cur->name, cur->is_persistent);
  if (cur->is_persistent == 0)
    crun_utils_delete_overlay1(cur->name, cur->cnt_dir);
  cur->delete_third_finalization_countdown = 1;
}

static void delete_third_finalization(t_crun *cur, int llid)
{ 
  char pth[MAX_PATH_LEN];
  char found[MAX_PATH_LEN];
  char resp[MAX_PATH_LEN];
  int nb;
  CLOG("%s step 3: overlay2 then the mount check", cur->name);
  /* Persistent too: its lower is the idmapped mount of the image. */
  crun_utils_delete_overlay2(cur->name, cur->cnt_dir);

  memset(pth, 0, MAX_PATH_LEN);
  snprintf(pth, MAX_PATH_LEN-1, "%s/%s", cur->cnt_dir, cur->name);

  nb = crun_utils_mount_exists(pth, found, sizeof(found));
  if (nb)
    {
    cur->delete_mount_retry += 1;
    if (cur->delete_mount_retry <= DELETE_MOUNT_RETRY_MAX)
      {
      if (cur->delete_mount_retry == 1)
        KERR("WARNING %s: %d mount(s) still there after the delete, first "
             "is %s, unmounting them", cur->name, nb, found);
      crun_utils_umount_tree(pth);
      cur->delete_third_finalization_countdown = 5;
      return;
      }
    KERR("ERROR %s: giving up after %d tries, %d mount(s) STILL under %s, "
         "first is %s. That tree cannot be removed until they are gone: "
         "umount them deepest first",
         cur->name, cur->delete_mount_retry, nb, pth, found);
    }
  else if (cur->delete_mount_retry)
    KERR("WARNING %s: mounts cleared after %d extra tries",
         cur->name, cur->delete_mount_retry);

  if (tar_img_del(cur->name, cur->bulk, cur->image, cur->cnt_dir,
                  cur->is_persistent, cur->brandtype))
    KTRC("%s: no image %s registered, the create never got that far",
         cur->name, cur->image);
  crun_utils_unlink_sub_dir_files(pth);
  CLOG("%s step 3: done, the entry is freed", cur->name);
  /* The delete is answered here, not when it was asked: the name is free
     for a create only now. */
  memset(resp, 0, MAX_PATH_LEN);
  snprintf(resp, MAX_PATH_LEN-1,
           "cloonsuid_crun_delete_resp_ok name=%s", cur->name);
  rpct_send_sigdiag_msg(llid, type_hop_suid_power, resp);
  free_crun(cur);
} 

void crun_beat(int llid)
{
  char resp[MAX_PATH_LEN];
  int pid;
  t_crun *next, *cur = g_head_crun;
  while (cur)
    {
    next = cur->next;
    if (cur->delete_first_finalization_countdown)
      {
      cur->delete_first_finalization_countdown -= 1;
      if (cur->delete_first_finalization_countdown == 0)
        delete_first_finalization(cur);
      }
    else if (cur->delete_second_finalization_countdown)
      {
      cur->delete_second_finalization_countdown -= 1;
      if (cur->delete_second_finalization_countdown == 0)
        delete_second_finalization(cur);
      }
    else if (cur->delete_third_finalization_countdown)
      {
      cur->delete_third_finalization_countdown -= 1;
      if (cur->delete_third_finalization_countdown == 0)
        delete_third_finalization(cur, llid);
      }
    else if (cur->crun_pid != 0)
      {
      if (kill(cur->crun_pid, 0) == 0)
        cur->pid_missing = 0;
      else if (++cur->pid_missing >= 3)
        {
        pid = cur->crun_pid;
        KERR("ERROR %s: its process pid %d is gone, tearing it down",
             cur->name, pid);
        memset(resp, 0, MAX_PATH_LEN);
        snprintf(resp, MAX_PATH_LEN-1,
                 "cloonsuid_crun_killed name=%s crun_pid=%d",
                 cur->name, pid);
        rpct_send_sigdiag_msg(llid, type_hop_suid_power, resp);
        CLOG("%s TEARDOWN from the watchdog", cur->name);
        cur->crun_pid = 0;
        teardown_start(cur);
        }
      }
    cur = next;
    }
}

void crun_recv_poldiag_msg(int llid, int tid, char *line)
{
  KERR("ERROR %s", line);
}

static void create_alloc(char *line, char *resp, char *name, char *bulk,
                         char *image, char *nspacecrun,
                         int uid_image, int nb_vwif, unsigned char *mac_vwif,
                         int nb_eth, int cloonix_rank, int vm_id,
                         char *cnt_dir, int is_persistent,
                         int is_privileged, char *brandtype, char *sbin_init,
                         char *sh_starter, int subuid_start, int subuid_qty,
                         int subgid_start, int subgid_qty)
{
  t_crun *cur = find_crun(name);
  memset(resp, 0, MAX_PATH_LEN);
  snprintf(resp, MAX_PATH_LEN-1,
           "cloonsuid_crun_create_net_resp_ko name=%s", name);
  if ((cur != NULL) && (cur->deleting))
    {
    KERR("ERROR %s: create_net while the previous one is torn down", name);
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_crun_create_net_resp_ko name=%s reason=the previous "
             "%s is still being torn down, retry in a moment", name, name);
    }
  else if (cur != NULL)
    {
    KERR("ERROR %s: create_net for a cnt that already has a record", name);
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_crun_create_net_resp_ko name=%s reason=already_exists",
             name);
    }
  else if (nb_eth > MAX_ETH_VM)
    {
    KERR("ERROR %s too big nb_eth:%d", name, nb_eth);
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_crun_create_net_resp_ko name=%s reason=too_many_eth",
             name);
    }
  else
    {
    alloc_crun(name, bulk, image, uid_image, nb_vwif, mac_vwif, nb_eth,
               nspacecrun, cloonix_rank, vm_id, cnt_dir,
               is_persistent, is_privileged, brandtype, sbin_init,
               sh_starter, subuid_start, subuid_qty, subgid_start,
               subgid_qty);
    memset(resp, 0, MAX_PATH_LEN);
    }
}

/* One message per interface, the last one (or num=-1 for none) triggers the
   directory creation. */
static void create_net_eth(char *line, char *resp, t_crun *cur,
                           char *name, int num, unsigned char *mac) 
{
  int call_crun_utils_create_net = 0;
  if (((num == -1) && (cur->nb_eth != 0)) || (num >= cur->nb_eth))
    {
    KERR("ERROR %s: eth num %d does not fit a cnt of %d eth", name, num,
         cur->nb_eth);
    snprintf(resp, MAX_PATH_LEN-1,
    "cloonsuid_crun_create_net_resp_ko name=%s reason=bad_eth_num", name);
    return;
    }
  if (num == -1)
    call_crun_utils_create_net = 1;
  else
    {
    memcpy(cur->eth_mac[num].mac, mac, 6);
    if (num+1 < cur->nb_eth) 
      memset(resp, 0, MAX_PATH_LEN);
    else
      call_crun_utils_create_net = 1;
    }
  if (call_crun_utils_create_net)
    {
    if (crun_utils_create_net(cur->brandtype, cur->is_persistent,
                              cur->is_privileged, cur->mountbear,
                              cur->image, name, cur->cnt_dir,
                              cur->nspacecrun, cur->cloonix_rank,
                              cur->vm_id, cur->nb_eth, cur->eth_mac))
      {
      KERR("ERROR %s: its directories under %s could not be made", name,
           cur->cnt_dir);
      snprintf(resp, MAX_PATH_LEN-1,
      "cloonsuid_crun_create_net_resp_ko name=%s reason=cnt_dir_creation",
      name);
      }
    else
      {
      snprintf(resp, MAX_PATH_LEN-1,
      "cloonsuid_crun_create_net_resp_ok name=%s", name);
      }
    }
}

static void create_config_json(char *line, char *resp, t_crun *cur,
                               char *startup_env, char *vmount)
{
  int ovspid;
  char path[MAX_PATH_LEN];
  char *name = cur->name;
  char *cnt_dir, *image;
  int is_persistent = cur->is_persistent;
  int is_privileged = cur->is_privileged;
  char *brandtype = cur->brandtype;
  cnt_dir = cur->cnt_dir;
  image = cur->image;
  if (!strcmp(image, "self_rootfs"))
    snprintf(cur->rootfs_path, MAX_PATH_LEN-1, "%s/%s", cur->bulk, image);
  else if (!is_persistent)
    snprintf(cur->rootfs_path, MAX_PATH_LEN-1,"%s/%s/%s",
             cnt_dir, name, ROOTFS);
  else if (is_privileged)
    snprintf(cur->rootfs_path, MAX_PATH_LEN-1, "%s/%s", cur->bulk, image);
  else
    snprintf(cur->rootfs_path, MAX_PATH_LEN-1,"%s/%s/%s",
             cnt_dir, name, LOWER);
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "%s/%s", cnt_dir, cur->name);

  ovspid = crun_utils_get_vswitchd_pid(get_net_name());
  cur->ovspid = ovspid;
  if (ovspid == 0)
    {
    KERR("ERROR %s NO VSWITCHD FOR NET %s", line, get_net_name());
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_crun_create_config_json_resp_ko name=%s "
             "reason=no_vswitchd_for_net_%s", name, get_net_name());
    }
  else if (crun_utils_create_config_json(cur->image, path, cur->rootfs_path,
                                    cur->nspacecrun_path, cur->mountbear,
                                    cur->mounttmp, is_persistent,
                                    is_privileged, brandtype, startup_env,
                                    vmount, cur->name, ovspid,
                                    cur->uid_image,
                                    cur->sbin_init, cur->sh_starter,
                                    cur->subuid_start, cur->subuid_qty,
                                    cur->subgid_start, cur->subgid_qty))
    {
    KERR("ERROR %s: config.json could not be written under %s "
         "(image %s, rootfs %s)", line, path, cur->image, cur->rootfs_path);
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_crun_create_config_json_resp_ko name=%s "
             "reason=config_json_write_failed_under_%s", name, path);
    }
  else
    {
    crun_utils_startup_env(cur->mountbear, startup_env,
                           cur->nb_vwif, cur->mac_vwif, cur->nb_eth);
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_crun_create_config_json_resp_ok name=%s", name);
    }
}

static void create_tar_img(char *line, char *resp, t_crun *cur)
{
  char *name = cur->name;
  if (tar_img_add(name, cur->bulk, cur->image, cur->uid_image,
                  cur->cnt_dir, cur->is_persistent, cur->brandtype))
    {
    KERR("ERROR %s: image %s/%s refused", name, cur->bulk, cur->image);
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_crun_create_tar_img_resp_ko name=%s "
             "reason=image_%s_refused", name, cur->image);
    }
  else
    {
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_crun_create_tar_img_resp_ok name=%s", name);
    }
}

static void create_overlay(char *line, char *resp, t_crun *cur)
{
  char mnt_tar_img[MAX_PATH_LEN];
  char *name = cur->name;
  if (tar_img_rootfs_get(name, cur->bulk, cur->image, cur->brandtype,
                         mnt_tar_img))
    {
    KERR("ERROR %s: image %s/%s is not registered", name, cur->bulk,
         cur->image);
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_crun_create_overlay_resp_ko name=%s "
             "reason=image_not_registered", name);
    }
  else
    { 
    CLOG("%s CREATE overlay: mounting now", cur->name);
    if (crun_utils_create_overlay(cur->mountbear, cur->mounttmp,
                                  cur->name, cur->cnt_dir, mnt_tar_img,
                                  cur->image, cur->uid_image,
                                  cur->is_persistent, cur->is_privileged,
                                  cur->brandtype,
                                  cur->subuid_start, cur->subuid_qty,
                                  cur->subgid_start, cur->subgid_qty))
      {
      KERR("ERROR %s: overlay of %s on %s could not be mounted", name,
           mnt_tar_img, cur->cnt_dir);
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_crun_create_overlay_resp_ko name=%s "
               "reason=overlay_mount_failed", name);
      }
    else
      {
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_crun_create_overlay_resp_ok name=%s", name);
      }
    }
}

/* The start is three turns of the loop: crun create, the eths, then the
   radios and crun start; in one turn a 32 eth container held it 2 s. */
static void start_answer(t_crun *cur, char *resp)
{
  if (msg_exist_channel(cur->start_llid))
    rpct_send_sigdiag_msg(cur->start_llid, cur->start_tid, resp);
  else
    KERR("WARNING %s: the switch went before the answer: %s", cur->name,
         resp);
  cur->start_stage = 0;
}

static void start_ko(t_crun *cur, char *reason)
{
  char resp[MAX_PATH_LEN];
  memset(resp, 0, MAX_PATH_LEN);
  snprintf(resp, MAX_PATH_LEN-1, "cloonsuid_crun_create_crun_start_resp_ko "
           "name=%s reason=%s", cur->name, reason);
  start_answer(cur, resp);
}

static void timer_start_stage(void *data)
{
  char *nm = (char *) data;
  t_crun *cur = find_crun(nm);
  char resp[MAX_PATH_LEN];
  free(nm);
  if ((!cur) || (!cur->start_stage))
    return;
  if (cur->deleting)
    {
    start_ko(cur, "deleted_while_starting");
    return;
    }
  if (cur->start_stage == 1)
    {
    if (crun_utils_user_eth_create(cur->name, cur->nspacecrun,
                                   cur->cloonix_rank, cur->vm_id,
                                   cur->nb_eth, cur->eth_mac))
      {
      KERR("ERROR %s: the %d eth(s) of this container could not be created "
           "in namespace %s", cur->name, cur->nb_eth, cur->nspacecrun);
      start_ko(cur, "veth_creation");
      return;
      }
    cur->start_stage = 2;
    nm = strdup(cur->name);
    clownix_timeout_add(1, timer_start_stage, (void *) nm, NULL, NULL);
    return;
    }
  if ((cur->nb_vwif) &&
      vwifi_radios_create(cur->name, cur->vm_id, cur->nspacecrun,
                          cur->nb_vwif, (uint8_t *) cur->mac_vwif))
    {
    KERR("ERROR %s: no radio for wlan=%d, refused", cur->name,
         cur->nb_vwif);
    start_ko(cur, vwifi_why_no_radio());
    }
  else if (crun_utils_create_crun_start(cur->name, cur->ovspid,
                                        cur->uid_image, cur->is_privileged))
    {
    KERR("ERROR %s: crun start refused, pid %d", cur->name, cur->crun_pid);
    start_ko(cur, "crun_start_failed");
    }
  else
    {
    memset(resp, 0, MAX_PATH_LEN);
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_crun_create_crun_start_resp_ok "
             "name=%s crun_pid=%d mountbear=%s",
             cur->name, cur->crun_pid, cur->mountbear);
    start_answer(cur, resp);
    }
}

static void create_crun_start(int llid, int tid, char *resp, t_crun *cur)
{
  char *name = cur->name;
  char *nm;
  CLOG("%s CREATE start: the last step of the create", name);
  crun_utils_prepare_with_delete(name, cur->nspacecrun, cur->vm_id,
                                 cur->nb_eth, cur->uid_image,
                                 cur->is_privileged);
  cur->crun_pid = crun_utils_create_crun_create(cur->cnt_dir, name,
                                          cur->ovspid, cur->uid_image,
                                          cur->is_privileged);
  if (cur->crun_pid == 0)
    {
    KERR("ERROR %s: crun create gave no pid, the container cannot start",
         name);
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_crun_create_crun_start_resp_ko name=%s "
             "reason=crun_create_gave_no_pid", name);
    }
  else if (crun_utils_user_nspacecrun_init(name, cur->nspacecrun,
                                           cur->crun_pid))
    {
    KERR("ERROR %s: the network namespace of crun pid %d could not be "
         "set up", name, cur->crun_pid);
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_crun_create_crun_start_resp_ko name=%s "
             "reason=net_namespace", name);
    }
  else
    {
    cur->start_llid = llid;
    cur->start_tid = tid;
    cur->start_stage = 1;
    nm = strdup(name);
    clownix_timeout_add(1, timer_start_stage, (void *) nm, NULL, NULL);
    }
}

static void delete_crun(char *line, char *resp, char *name)
{
  t_crun *cur;
  cur = find_crun(name);
  if (cur == NULL)
    {
    KERR("ERROR %s: delete of a cnt that has no record", name);
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_crun_delete_resp_ko name=%s reason=no_record", name);
    }
  else
    {
    if (cur->deleting)
      KTRC("%s: delete asked while its teardown is running", name);
    else
      {
      CLOG("%s DELETE asked, crun_pid=%d", name, cur->crun_pid);
      teardown_start(cur);
      }
    /* Answered by delete_third_finalization, once the record is freed. */
    resp[0] = 0;
    }
}

static char g_last_refused[MAX_NAME_LEN];

static t_crun *step_target(char *line, char *resp, char *name, char *step)
{
  t_crun *cur = find_crun(name);
  if (cur == NULL)
    {
    if (!strcmp(name, g_last_refused))
      KTRC("%s: %s after its create_net was refused", name, step);
    else
      KERR("ERROR %s: %s for a cnt that has no record", name, step);
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_crun_%s_resp_ko name=%s reason=no_record", step, name);
    }
  else if (cur->deleting)
    {
    KTRC("%s: %s arrived while it is being torn down", name, step);
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_crun_%s_resp_ko name=%s reason=being_deleted",
             step, name);
    cur = NULL;
    }
  return cur;
}

static void extract_env_vmount(char *line, char *startup_env, char *vmount)
{
  int len, lenv;
  char  *ptr1, *ptr2, *ptrv1, *ptrv2;
  memset(startup_env, 0, MAX_PATH_LEN);
  memset(vmount, 0, 4*MAX_PATH_LEN);
  ptr1 = strstr(line, "<startup_env_keyid>");
  if(!ptr1)
    KOUT("%s", line);
  ptr2 = strstr(line, "</startup_env_keyid>");
  if(!ptr2)
    KOUT("%s", line);
  len = strlen("<startup_env_keyid>");
  ptrv1 = strstr(line, "<startup_vmount>");
  if(!ptrv1)
    KOUT("%s", line);
  ptrv2 = strstr(line, "</startup_vmount>");
  if(!ptrv2)
    KOUT("%s", line);
  lenv = strlen("<startup_vmount>");
  *ptr2 = 0;
  *ptrv2 = 0;
  if (strlen(ptr1) - len >= MAX_PATH_LEN)
    KOUT("%s", line);
  if (strlen(ptrv1) - lenv >= 4*MAX_PATH_LEN)
    KOUT("%s", line);
  strncpy(startup_env, ptr1+len, MAX_PATH_LEN-1);
  strncpy(vmount, ptrv1+lenv, 4*MAX_PATH_LEN-1);
  *ptr2 = '<';
  *ptrv2 = '<';
}

void crun_recv_sigdiag_msg(int llid, int tid, char *line)
{
  t_crun *cur;
  char name[MAX_NAME_LEN];
  char brandtype[MAX_NAME_LEN];
  char bulk[MAX_PATH_LEN];
  char image[MAX_NAME_LEN];
  char resp[MAX_PATH_LEN];
  char cnt_dir[MAX_PATH_LEN];
  unsigned char mac[6];
  int  uid_image, num, cpu, ram, nb_vwif, nb_eth, cloonix_rank, vm_id;
  int  is_persistent, is_privileged;
  char startup_env[MAX_PATH_LEN];
  char sbin_init[MAX_PATH_LEN];
  char sh_starter[MAX_PATH_LEN];
  char why[MAX_PATH_LEN];
  char vmount[4*MAX_PATH_LEN];
  int subuid_start, subuid_qty, subgid_start, subgid_qty;
  char nspacecrun[MAX_PATH_LEN];

  cloonix_rank = get_cloonix_rank();
  memset(resp, 0, MAX_PATH_LEN);
  memset(name, 0, MAX_NAME_LEN);
  if ((sscanf(line, "cloonsuid_crun_%*s name=%63s", name) == 1) &&
      (!name_is_sane(name)))
    {
    KERR("ERROR refused request, bad guest name: %s", line);
    return;
    }
  if (sscanf(line, 

  "cloonsuid_crun_create_net name=%63s "
  "bulk=%299s image=%63s nb_vwif=%d "
  "mac_vwif=0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx "
  "nb=%d vm_id=%d cnt_dir=%299s "
  "is_persistent=%d is_privileged=%d brandtype=%63s",
  name, bulk, image, &nb_vwif,
  &(mac[0]),&(mac[1]),&(mac[2]),&(mac[3]),&(mac[4]),&(mac[5]),
  &nb_eth, &vm_id,
  cnt_dir, &is_persistent, &is_privileged, brandtype) == 16)
    {
    if (tar_img_check(bulk, image, &uid_image, is_persistent,
                      brandtype, sbin_init, sh_starter,
                      &subuid_start, &subuid_qty,
                      &subgid_start, &subgid_qty, why))
      {
      strncpy(g_last_refused, name, MAX_NAME_LEN-1);
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_crun_create_net_resp_ko name=%s "
               "reason=%s", name, why);
      } 
    else
      {
      memset(nspacecrun, 0, MAX_PATH_LEN);
      snprintf(nspacecrun, MAX_PATH_LEN-1,
               "%s_%s_%s_%d_%d",
               BASE_NAMESPACE, get_net_name(),
               name, cloonix_rank, vm_id);
      if ((uid_image == 0) && (is_privileged == 0))
        is_privileged = 1;
      if ((is_privileged) && (!pthexec_running_in_crun(NULL)))
        {
        KERR("ERROR REFUSED privileged container %s: the net runs as root",
             name);
        strncpy(g_last_refused, name, MAX_NAME_LEN-1);
        snprintf(resp, MAX_PATH_LEN-1,
                 "cloonsuid_crun_create_net_resp_ko name=%s "
                 "reason=net_run_by_root", name);
        }
      else
        {
      create_alloc(line, resp, name, bulk, image, nspacecrun,
                   uid_image, nb_vwif, mac, nb_eth, cloonix_rank,
                   vm_id, cnt_dir, is_persistent,
                   is_privileged, brandtype, sbin_init, sh_starter,
                   subuid_start, subuid_qty, subgid_start, subgid_qty);
      cur = find_crun(name);
      if ((cur != NULL) &&
          (dirs_agent_create_mnt_tmp(cur->mountbear, cur->mounttmp)))
        {
        KERR("ERROR %s: %s or %s could not be made", name, cur->mountbear,
             cur->mounttmp);
        free_crun(cur);
        snprintf(resp, MAX_PATH_LEN-1,
                 "cloonsuid_crun_create_net_resp_ko name=%s "
                 "reason=mnt_dirs_creation", name);
        }
        }
      }
    }
  else if (sscanf(line,
  "cloonsuid_crun_create_eth name=%63s num=%d", name, &num) == 2)
    {
    memset(mac, 0, 6);
    cur = step_target(line, resp, name, "create_net");
    if (cur == NULL)
      {
      }
    else if ((num >= 0) && (sscanf(line,
    "cloonsuid_crun_create_eth name=%63s num=%d "
    "mac=0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx",
    name, &num, &(mac[0]),&(mac[1]),&(mac[2]),
    &(mac[3]),&(mac[4]),&(mac[5])) != 8))
      {
      KERR("ERROR %s: bad mac in %s", name, line);
      snprintf(resp, MAX_PATH_LEN-1,
      "cloonsuid_crun_create_net_resp_ko name=%s reason=bad_mac", name);
      }
    else
      {
      create_net_eth(line, resp, cur, name, num, mac);
      }
    }
  else if (sscanf(line,
  "cloonsuid_crun_create_config_json name=%63s", name) == 1)
    {
    cur = step_target(line, resp, name, "create_config_json");
    if (cur != NULL)
      {
      extract_env_vmount(line, startup_env, vmount);
      create_config_json(line, resp, cur, startup_env, vmount);
      }
    }
  else if (sscanf(line,
  "cloonsuid_crun_create_tar_img name=%63s", name) == 1)
    {
    cur = step_target(line, resp, name, "create_tar_img");
    if (cur != NULL)
      {
      create_tar_img(line, resp, cur);
      }
    }
  else if (sscanf(line,
  "cloonsuid_crun_create_overlay name=%63s", name) == 1)
    {
    cur = step_target(line, resp, name, "create_overlay");
    if (cur != NULL)
      { 
      create_overlay(line, resp, cur);
      }
    }
  else if (sscanf(line,
  "cloonsuid_crun_create_crun_start name=%63s", name) == 1)
    {
    cur = step_target(line, resp, name, "create_crun_start");
    if (cur != NULL)
      create_crun_start(llid, tid, resp, cur);
    }
  else if (sscanf(line, 
  "cloonsuid_crun_delete name=%63s", name) == 1)
    {
    delete_crun(line, resp, name);
    }
  else if (sscanf(line,
  "cloonsuid_crun_update name=%63s cpu=%d ram=%d", name, &cpu, &ram) == 3)
    {
    char why[MAX_PATH_LEN];
    cur = find_crun(name);
    if ((cur == NULL) || (cur->deleting) || (cur->crun_pid == 0))
      {
      KERR("ERROR %s: update asked, no running container of that name", name);
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_crun_update_resp_ko name=%s reason=no_record", name);
      }
    else if (crun_utils_update(name, cur->crun_pid, cur->is_privileged,
                               cpu, ram, why, MAX_PATH_LEN))
      {
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_crun_update_resp_ko name=%s reason=%s", name, why);
      }
    else
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_crun_update_resp_ok name=%s", name);
    }
  else if (sscanf(line,
  "cloonsuid_crun_ERROR name=%63s", name) == 1)
    {
    cur = find_crun(name);
    if ((cur != NULL) && (cur->deleting == 0))
      {
      CLOG("%s TEARDOWN from an error reported by the switch", name);
      KERR("WARNING %s: torn down after the create step that failed above",
           name);
      teardown_start(cur);
      }
    }
  else
    {
    KERR("ERROR unknown message, answering ko: %s", line);
    memset(name, 0, MAX_NAME_LEN);
    if (sscanf(line, "cloonsuid_crun_%*s name=%63s", name) != 1)
      memset(name, 0, MAX_NAME_LEN);
    if (strlen(name))
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsuid_crun_create_net_resp_ko name=%s", name);
    }
  if (strlen(resp))
    rpct_send_sigdiag_msg(llid, tid, resp);
}

void crun_kill_all(void)
{
  char resp[MAX_PATH_LEN];
  char pth[MAX_PATH_LEN];
  char found[MAX_PATH_LEN];
  int nb, left = 0, done = 0;
  t_crun *next, *cur = g_head_crun;
  while (cur)
    {
    next = cur->next;
    memset(pth, 0, MAX_PATH_LEN);
    snprintf(pth, MAX_PATH_LEN-1, "%s/%s", cur->cnt_dir, cur->name);
    delete_crun("kill all crun procedure", resp, cur->name);

    /* Synchronous: delete_crun only arms a three beat chain and the process
       may exit before those beats run. */
    crun_utils_umount_tree(pth);
    nb = crun_utils_mount_exists(pth, found, sizeof(found));
    if (nb)
      {
      KERR("ERROR %s: %d mount(s) still under %s after kill all, first is "
           "%s", cur->name, nb, pth, found);
      left += nb;
      }
    done += 1;
    cur = next;
    }
  if (left)
    KERR("ERROR kill all: %d container(s) done and %d mount(s) left behind. "
         "That tree cannot be removed until they are gone: unmount them "
         "deepest first", done, left);
}

int crun_get_is_privileged(char *name, int *is_privileged)
{
  int result = -1;
  t_crun *cur = find_crun(name);
  if (cur)
    {
    *is_privileged = cur->is_privileged;
    result = 0;
    }
  return result;
}

void crun_init(char *var_root)
{
  g_head_crun = NULL;
  utils_init(var_root);
  tar_img_init();
}
