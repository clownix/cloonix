/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_daemon.h"
#include "cnt.h"
#include "crun.h"
#include "hop_event.h"
#include "cfg_store.h"
#include "utils_cmd_line_maker.h"
#include "uml_clownix_switch.h"
#include "file_read_write.h"

static void cnt_gone(char *name, char *line)
{
  char step[MAX_NAME_LEN];
  int i;
  memset(step, 0, MAX_NAME_LEN);
  for (i=0; (i<MAX_NAME_LEN-1) && line[i] && (line[i] != ' '); i++)
    step[i] = line[i];
  KTRC("%s: %s arrived after the container was deleted", name, step);
}

void crun_sigdiag_resp(int llid, char *line)
{
  int crun_pid, idx;
  char air[MAX_NAME_LEN];
  t_cnt *cur;
  char name[MAX_NAME_LEN];
  char req[5*MAX_PATH_LEN];
  char mountbear[2*MAX_PATH_LEN];
  memset(req, 0, 5*MAX_PATH_LEN);
  memset(mountbear, 0, 2*MAX_PATH_LEN);
  if (sscanf(line, 
  "cloonsuid_crun_create_net_resp_ok name=%63s", name) == 1)
    {
    cur = find_cnt(name);
    if (cur == NULL) 
      cnt_gone(name, line);
    else
      {
      snprintf(req, 5*MAX_PATH_LEN-1, 
               "cloonsuid_crun_create_config_json name=%s "
               "<startup_env_keyid>%s</startup_env_keyid> "
               "<startup_vmount>%s</startup_vmount>\n",
               name, cur->cnt.startup_env, cur->cnt.vmount);
      if (send_sig_suid_power(llid, req))
        KERR("ERROR %d %s", llid, name);
      }
    }
  else if (sscanf(line,
  "cloonsuid_crun_create_config_json_resp_ok name=%63s", name) == 1)
    {
    cur = find_cnt(name);
    if (cur == NULL)
      cnt_gone(name, line);
    else
      {
      snprintf(req, 5*MAX_PATH_LEN-1,
               "cloonsuid_crun_create_tar_img name=%s", name);
      if (send_sig_suid_power(llid, req))
        KERR("ERROR %d %s", llid, name);
      }
    }
  else if (sscanf(line,
  "cloonsuid_crun_create_tar_img_resp_ok name=%63s", name) == 1)
    {
    cur = find_cnt(name);
    if (cur == NULL)
      cnt_gone(name, line);
    else
      {
      snprintf(req, 5*MAX_PATH_LEN-1,
               "cloonsuid_crun_create_overlay name=%s", name);
      if (send_sig_suid_power(llid, req))
        KERR("ERROR %d %s", llid, name);
      }
    }
  else if (sscanf(line,
  "cloonsuid_crun_create_overlay_resp_ok name=%63s", name) == 1)
    {
    cur = find_cnt(name);
    if (cur == NULL)
      cnt_gone(name, line);
    else
      {
      snprintf(req, 5*MAX_PATH_LEN-1,
               "cloonsuid_crun_create_crun_start name=%s", name);
      if (send_sig_suid_power(llid, req))
        KERR("ERROR %d %s", llid, name);
      }
    }
  else if (sscanf(line,
  "cloonsuid_crun_create_crun_start_resp_ok name=%63s crun_pid=%d mountbear=%599s",
  name, &crun_pid, mountbear) == 3)
    {
    cur = find_cnt(name);
    if (cur == NULL)
      cnt_gone(name, line);
    else
      {
      strncpy(cur->mountbear, mountbear, MAX_PATH_LEN-1);
      cur->cnt_pid = crun_pid;
      utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
      cnt_vhost_and_doors_begin(cur);
      }
    }
  else if (sscanf(line,
  "cloonsuid_air_cnt_resp_ok name=%63s idx=%d air=%63s", name, &idx,
  air) == 3)
    {
    cur = find_cnt(name);
    if (cur == NULL)
      cnt_gone(name, line);
    else if ((idx < 0) || (idx >= cur->cnt.nb_tot_nb_vwif))
      KERR("ERROR %s: no wlan%d, %s", name, idx, line);
    else
      {
      memset(cur->cnt.vwif_air[idx], 0, MAX_NAME_LEN);
      if (strcmp(air, "-"))
        strncpy(cur->cnt.vwif_air[idx], air, MAX_NAME_LEN-1);
      cfg_hysteresis_send_topo_info();
      utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
      }
    }
  else if (sscanf(line,
  "cloonsuid_air_cnt_resp_ko name=%63s idx=%d", name, &idx) == 2)
    {
    char *why = strstr(line, "reason=");
    char txt[MAX_PATH_LEN];
    KERR("ERROR %s", line);
    cur = find_cnt(name);
    if (cur == NULL)
      cnt_gone(name, line);
    else
      {
      snprintf(txt, MAX_PATH_LEN-1, "%s wlan%d: %s", name, idx,
               why ? why+7 : "ko");
      KERR("ERROR %s", txt);
      utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), txt);
      }
    }
  else if (sscanf(line,
  "cloonsuid_crun_update_resp_ok name=%63s", name) == 1)
    {
    cur = find_cnt(name);
    if (cur == NULL)
      cnt_gone(name, line);
    else
      {
      cur->lim_cpu = cur->lim_cpu_asked;
      cur->lim_ram = cur->lim_ram_asked;
      utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
      }
    }
  else if (sscanf(line,
  "cloonsuid_crun_update_resp_ko name=%63s", name) == 1)
    {
    char *why = strstr(line, "reason=");
    KERR("ERROR %s: cpu= ram= refused: %s", name, why ? why+7 : line);
    cur = find_cnt(name);
    if (cur == NULL)
      cnt_gone(name, line);
    else
      {
      char txt[MAX_PATH_LEN];
      snprintf(txt, MAX_PATH_LEN-1, "CNT LIMIT %s: %s", name,
               why ? why+7 : "ko");
      KERR("ERROR %s", txt);
      utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), txt);
      }
    }
  else if (sscanf(line, 
  "cloonsuid_crun_delete_resp_ok name=%63s", name) == 1)
    {
    cur = find_cnt(name);
    if (cur == NULL) 
      cnt_gone(name, line);
    else
      {
      utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
      cnt_free_cnt(name);
      }
    }
  else if (sscanf(line,
  "cloonsuid_crun_killed name=%63s crun_pid=%d", name, &crun_pid) == 2)
    {
    cur = find_cnt(name);
    if (cur == NULL) 
      cnt_gone(name, line);
    else
      {
      KERR("ERROR %s: its process pid %d died, deleting it", name, crun_pid);
      utils_send_status_ko(&(cur->cli_llid),&(cur->cli_tid),"CNT KILLED");
      cur->process_gone = 1;
      if (cur->poweroff_done == 0)
        cnt_delete(llid, 0, 0, name);
      }
    }
  else if ((sscanf(line,
  "cloonsuid_crun_create_net_resp_ko name=%63s", name) == 1) ||
           (sscanf(line,
  "cloonsuid_crun_create_config_json_resp_ko name=%63s", name) == 1) ||
           (sscanf(line,
  "cloonsuid_crun_create_tar_img_resp_ko name=%63s", name) == 1) ||
           (sscanf(line,
  "cloonsuid_crun_create_overlay_resp_ko name=%63s", name) == 1) ||
           (sscanf(line,
  "cloonsuid_crun_create_crun_start_resp_ko name=%63s", name) == 1) ||
           (sscanf(line,
  "cloonsuid_crun_delete_resp_ko name=%63s", name) == 1))
    {
    char *why = strstr(line, "reason=");
    cur = find_cnt(name);
    /* The steps are sent in a row: after a refusal the next ones answer
       ko for a cnt already freed here, and that is not a second fault. */
    if (cur == NULL)
      KTRC("%s", line);
    else
      {
      /* One refusal, one line: the reason of suid_power, or its whole answer
         when it gave none, for the log and for the client. */
      char txt[MAX_PATH_LEN];
      snprintf(txt, MAX_PATH_LEN-1, "CNT ERROR %s: %s", name,
               why ? why+7 : line);
      KERR("ERROR %s refused: %s", name, why ? why+7 : line);
      snprintf(req, MAX_PATH_LEN-1,
      "cloonsuid_crun_ERROR name=%s", name);
      if (send_sig_suid_power(llid, req))
        KERR("ERROR %d %s", llid, name);
      utils_send_status_ko(&(cur->cli_llid),&(cur->cli_tid), txt);
      cnt_free_cnt(name);
      }
    }
  else if (sscanf(line, 
  "cloonsuid_crun_ERROR name=%63s", name) == 1)
    {
    KERR("ERROR %s: suid_power reported the container failed, dropping it",
         name);
    cur = find_cnt(name);
    if (cur != NULL)
      {
      KERR("ERROR %s: %s", name, line);
      utils_send_status_ko(&(cur->cli_llid),&(cur->cli_tid),"ERROR CNT");
      cnt_free_cnt(name);
      }
    }
  else
    KERR("ERROR %s", line);
}

int crun_create(int llid, int vm_id, t_topo_cnt *cnt)
{
  int i, result = -1;
  char req[2*MAX_PATH_LEN];
  char *image;
  char *cnt_dir = utils_get_cnt_dir();
  unsigned char *mac;
  unsigned char mac_vwif[6];
  char option1[MAX_PATH_LEN];
  char option2[MAX_PATH_LEN];
  char *bulk = cfg_get_bulk();

  image = cnt->image;
  if (get_running_in_crun())
    {
    memset(option1, 0, MAX_PATH_LEN);
    memset(option2, 0, MAX_PATH_LEN);
    snprintf(option1, MAX_PATH_LEN-1, "%s/%s", cfg_get_bulk(), image);
    snprintf(option2, MAX_PATH_LEN-1, "%s/%s", cfg_get_bulk_host(), image);
    if (!file_exists(option1, R_OK))
      {
      if (file_exists(option2, R_OK))
        bulk = cfg_get_bulk_host();
      else
        KERR("ERROR %s and %s do not exist", option1, option2);
      }
    }

  for (i=0; i<6; i++)
    mac_vwif[i] = cnt->eth_table[0].mac_addr[i];
  mac_vwif[5] = (unsigned char) cnt->nb_tot_eth; 
  mac_vwif[3] += 1;

  memset(req, 0, 2*MAX_PATH_LEN);
  snprintf(req, 2*MAX_PATH_LEN-1, 
  "cloonsuid_crun_create_net name=%s "
  "bulk=%s image=%s nb_vwif=%d "
  "mac_vwif=0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx "
  "nb=%d vm_id=%d cnt_dir=%s "
  "is_persistent=%d is_privileged=%d brandtype=%s",
  cnt->name, bulk, image, cnt->nb_tot_nb_vwif, mac_vwif[0], mac_vwif[1],
  mac_vwif[2], mac_vwif[3], mac_vwif[4], mac_vwif[5], cnt->nb_tot_eth,
  vm_id, cnt_dir, cnt->is_persistent, cnt->is_privileged,
  cnt->brandtype);
  /* The record is the caller's to free: nothing is released here. */
  if (send_sig_suid_power(llid, req))
    KERR("ERROR %s: create_net could not be sent to suid_power", cnt->name);
  else
    {
    result = 0;
    if (cnt->nb_tot_eth == 0)
      {
      memset(req, 0, 2*MAX_PATH_LEN);
      snprintf(req, 2*MAX_PATH_LEN-1,
      "cloonsuid_crun_create_eth name=%s num=-1 mac=00:00:00:00:00:00",
      cnt->name);
      if (send_sig_suid_power(llid, req))
        {
        KERR("ERROR %s: create_eth could not be sent to suid_power",
             cnt->name);
        result = -1;
        }
      }
    else
      {
      for (i=0; (result == 0) && (i<cnt->nb_tot_eth); i++)
        {
        memset(req, 0, 2*MAX_PATH_LEN);
        mac = cnt->eth_table[i].mac_addr;
        snprintf(req, 2*MAX_PATH_LEN-1,
        "cloonsuid_crun_create_eth name=%s num=%d "
        "mac=0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx",
        cnt->name, i, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        if (send_sig_suid_power(llid, req))
          {
          KERR("ERROR %s: create_eth %d could not be sent to suid_power",
               cnt->name, i);
          result = -1;
          }
        }
      }
    }
  return result;
}

void crun_init(void)
{
}

