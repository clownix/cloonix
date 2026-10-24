/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <time.h>

#include "io_clownix.h"
#include "ovs_execv.h"
#include "ovs_cmd.h"
#include "intf_flags.h"

int ovs_cmd_vhost_up(char *ovs_bin, char *ovs_dir,
                     char *name, int num, char *vhost)
{
  int result;
  result = set_intf_flags_iff_up_down(vhost, 1);
  return result;
}

static char *mirror_name(char *vhost)
{
  static char nm[MAX_NAME_LEN];
  memset(nm, 0, MAX_NAME_LEN);
  if (snprintf(nm, MAX_NAME_LEN, "mir%s", vhost) >= MAX_NAME_LEN)
    KERR("ERROR the mirror name for %s does not fit", vhost);
  return nm;
}

#define MAX_MIR_ROWS 8
#define MAX_BR_ROWS 4096
#define REMOVE_PER_VSCTL 6

static int mirror_rows(char *bin, char *dir, char *mir,
                       char uuid[][MAX_NAME_LEN], int max)
{
  char cmd[MAX_ARG_LEN];
  char lines[MAX_MIR_ROWS*2][MAX_NAME_LEN];
  int i, nb, n = 0;
  char *ptr;
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1, "-- --columns=_uuid find Mirror name=%s", mir);
  if (ovs_vsctl_quiet(bin, dir, cmd))
    {
    KERR("ERROR OVSCMD: cannot list the mirror rows named %s", mir);
    return -1;
    }
  nb = ovs_popen_read_lines(dir, lines, MAX_MIR_ROWS*2);
  for (i=0; (i<nb) && (n<max); i++)
    {
    if (strncmp(lines[i], "_uuid", 5))
      continue;
    ptr = strchr(lines[i], ':');
    if (!ptr)
      continue;
    ptr += 1;
    while (*ptr == ' ')
      ptr += 1;
    if (strlen(ptr) != 36)
      {
      KERR("ERROR OVSCMD: unreadable mirror uuid line: %s", lines[i]);
      continue;
      }
    strncpy(uuid[n], ptr, MAX_NAME_LEN-1);
    n += 1;
    }
  return n;
}

/* Rows of that name taken out of the bridge of the request, then out of every
   other bridge only if a row is left. */
static int mirror_clear(char *bin, char *dir, char *mir, char *bvhost)
{
  char uuid[MAX_MIR_ROWS][MAX_NAME_LEN];
  char (*br)[MAX_NAME_LEN];
  char cmd[MAX_ARG_LEN];
  int i, j, k, len, nbu, nbr, left;
  nbu = mirror_rows(bin, dir, mir, uuid, MAX_MIR_ROWS);
  if (nbu <= 0)
    return nbu;
  /* Quiet: the lan may have gone with its bridge, the count below decides. */
  for (i=0; i<nbu; i++)
    {
    memset(cmd, 0, MAX_ARG_LEN);
    snprintf(cmd, MAX_ARG_LEN-1, "-- remove Bridge %s mirrors %s",
             bvhost, uuid[i]);
    ovs_vsctl_quiet(bin, dir, cmd);
    }
  left = mirror_rows(bin, dir, mir, uuid, MAX_MIR_ROWS);
  if (left <= 0)
    return (left < 0) ? -1 : nbu;
  if (ovs_vsctl_quiet(bin, dir, "list-br"))
    {
    KERR("ERROR OVSCMD: list-br failed, mirror %s cannot be cleared", mir);
    return -1;
    }
  br = malloc(MAX_BR_ROWS * MAX_NAME_LEN);
  if (!br)
    KOUT("ERROR malloc");
  nbr = ovs_popen_read_lines(dir, br, MAX_BR_ROWS);
  for (i=0; i<left; i++)
    {
    for (j=0; j<nbr; j+=REMOVE_PER_VSCTL)
      {
      memset(cmd, 0, MAX_ARG_LEN);
      len = 0;
      for (k=j; (k<nbr) && (k<j+REMOVE_PER_VSCTL); k++)
        len += snprintf(cmd+len, MAX_ARG_LEN-1-len,
                        "%s-- remove Bridge %s mirrors %s",
                        len ? " " : "", br[k], uuid[i]);
      if (ovs_vsctl(bin, dir, cmd))
        KERR("ERROR OVSCMD: mirror %s row %s could not be removed from "
             "the bridges", mir, uuid[i]);
      }
    }
  free(br);
  left = mirror_rows(bin, dir, mir, uuid, MAX_MIR_ROWS);
  if (left)
    {
    KERR("ERROR OVSCMD: %d mirror row(s) named %s are STILL there after "
         "being removed from %s and from all %d bridges", left, mir,
         bvhost, nbr);
    return -1;
    }
  return nbu;
}

int ovs_cmd_add_snf_lan(char *ovs_bin, char *ovs_dir, char *name, int num,
                        char *vhost, char *lan)
{
  int nb, result = 0;
  char cmd[MAX_ARG_LEN];
  char bvhost[MAX_NAME_LEN];
  char mir[MAX_NAME_LEN];
  memset(bvhost, 0, MAX_NAME_LEN);
  memset(cmd, 0, MAX_ARG_LEN);
  memset(mir, 0, MAX_NAME_LEN);
  snprintf(bvhost, MAX_NAME_LEN-1, "%s%s", OVS_BRIDGE, lan);
  strncpy(mir, mirror_name(vhost), MAX_NAME_LEN-1);

  if (set_intf_flags_iff_up_down(vhost, 1))
    {
    KERR("ERROR OVSCMD: SNF ADD %s: the interface cannot be set up", vhost);
    return -1;
    }
  nb = mirror_clear(ovs_bin, ovs_dir, mir, bvhost);
  if (nb < 0)
    return -1;
  if (nb > 0)
    KERR("WARNING OVSCMD: SNF ADD %s on %s: %d stale mirror row(s) named "
         "%s cleared first, an earlier mirror of this interface was never "
         "removed", vhost, bvhost, nb, mir);
  snprintf(cmd, MAX_ARG_LEN-1,
  "-- --may-exist add-port %s s%s "
  "-- --id=@m create mirror name=%s "
  "-- add bridge %s mirrors @m "
  "-- --id=@%s get port %s "
  "-- set mirror %s select_src_port=@%s select_dst_port=@%s "
  "-- --id=@s%s get port s%s -- set mirror %s output-port=@s%s",
                bvhost, vhost, mir,
                bvhost, vhost, vhost,
                mir, vhost, vhost,
                vhost, vhost, mir, vhost); 
  if (ovs_vsctl(ovs_bin, ovs_dir, cmd))
    {
    if (ovs_popen_last_said(ovs_dir, "could not open network device"))
      KERR("ERROR OVSCMD: SNF ADD %s on %s: ovs cannot open s%s yet, the "
           "capture is not cabled; its mirror row and port are removed "
           "again", vhost, bvhost, vhost);
    else
      KERR("ERROR OVSCMD: SNF ADD %s on %s refused -- see what ovs said "
           "above; whatever it left is removed again", vhost, bvhost);
    mirror_clear(ovs_bin, ovs_dir, mir, bvhost);
    memset(cmd, 0, MAX_ARG_LEN);
    snprintf(cmd, MAX_ARG_LEN-1, "-- --if-exists del-port s%s", vhost);
    if (ovs_vsctl(ovs_bin, ovs_dir, cmd))
      KERR("ERROR OVSCMD: SNF ADD %s: port s%s left behind by the failed "
           "add could not be removed", vhost, vhost);
    result = -1;
    }
  return result;
}

int ovs_cmd_del_snf_lan(char *ovs_bin, char *ovs_dir, char *name, int num,
                        char *vhost, char *lan)
{
  int nb, result = 0;
  char cmd[MAX_ARG_LEN];
  char bvhost[MAX_NAME_LEN];
  char mir[MAX_NAME_LEN];
  memset(bvhost, 0, MAX_NAME_LEN);
  memset(cmd, 0, MAX_ARG_LEN);
  memset(mir, 0, MAX_NAME_LEN);
  snprintf(bvhost, MAX_NAME_LEN-1, "%s%s", OVS_BRIDGE, lan);
  strncpy(mir, mirror_name(vhost), MAX_NAME_LEN-1);
  nb = mirror_clear(ovs_bin, ovs_dir, mir, bvhost);
  if (nb < 0)
    result = -1;
  else if (nb == 0)
    KERR("WARNING OVSCMD: SNF DEL %s on %s: no mirror row named %s to "
         "remove, the mirror had already gone", vhost, bvhost, mir);
  else if (nb > 1)
    KERR("WARNING OVSCMD: SNF DEL %s on %s: %d mirror rows named %s "
         "removed, one had outlived an earlier del", vhost, bvhost, nb, mir);
  snprintf(cmd, MAX_ARG_LEN-1, "-- --if-exists del-port s%s", vhost);
  if (ovs_vsctl(ovs_bin, ovs_dir, cmd))
    {
    KERR("ERROR OVSCMD: SNF DEL %s on %s: port s%s could not be removed",
         vhost, bvhost, vhost);
    result = -1;
    }
  return result;
}

int ovs_cmd_add_lan_rstp(char *ovs_bin, char *ovs_dir, char *lan, int val)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];
  char bvhost[MAX_NAME_LEN];
  memset(bvhost, 0, MAX_NAME_LEN);
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(bvhost, MAX_NAME_LEN-1, "%s%s", OVS_BRIDGE, lan);
  snprintf(cmd, MAX_ARG_LEN-1, "-- set Bridge %s rstp_enable=%s", bvhost,
           val ? "true" : "false");
  if (ovs_vsctl(ovs_bin, ovs_dir, cmd))
    {
    KERR("ERROR OVSCMD: RSTP %s LAN %s", val ? "ON" : "OFF", lan);
    result = -1;
    }
  return result;
}

int ovs_cmd_add_lan(char *ovs_bin, char *ovs_dir, char *lan)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];
  char vhost[MAX_NAME_LEN];
  memset(vhost, 0, MAX_NAME_LEN);
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(vhost, MAX_NAME_LEN-1, "%s%s", OVS_BRIDGE, lan);
  /* forward-bpdu: a lan is a wire, 01:80:c2:00:00:0x frames cross it; with
       rstp on, ovs handles them. */
  snprintf(cmd, MAX_ARG_LEN-1, "-- add-br %s -- set Bridge %s "
           "other-config:forward-bpdu=true", vhost, vhost);
  if (ovs_vsctl(ovs_bin, ovs_dir, cmd))
    {
    if (ovs_popen_last_said(ovs_dir, "already exists"))
      {
      /* A bridge of that name the switch does not know: left by a del that
         did not take. */
      KERR("WARNING OVSCMD: ADD LAN %s: bridge %s already existed in ovs, "
           "deleting the leftover and making it again", lan, vhost);
      memset(cmd, 0, MAX_ARG_LEN);
      snprintf(cmd, MAX_ARG_LEN-1, "-- del-br %s -- add-br %s -- set Bridge "
               "%s other-config:forward-bpdu=true", vhost, vhost, vhost);
      if (ovs_vsctl(ovs_bin, ovs_dir, cmd))
        {
        KERR("ERROR OVSCMD: ADD LAN %s: the leftover bridge %s could not "
             "be remade", lan, vhost);
        result = -1;
        }
      }
    else
      {
      KERR("ERROR OVSCMD: ADD LAN %s", lan);
      result = -1;
      }
    }
  return result;
}

int ovs_cmd_del_lan(char *ovs_bin, char *ovs_dir, char *lan)
{
  int result = 0;
  char cmd[MAX_ARG_LEN]; 
  char bvhost[MAX_NAME_LEN];
  memset(cmd, 0, MAX_ARG_LEN);
  memset(bvhost, 0, MAX_NAME_LEN);
  snprintf(bvhost, MAX_NAME_LEN-1, "%s%s", OVS_BRIDGE, lan);
  snprintf(cmd, MAX_ARG_LEN-1, "-- del-br %s", bvhost);
  if (ovs_vsctl(ovs_bin, ovs_dir, cmd))
    {
    memset(cmd, 0, MAX_ARG_LEN);
    snprintf(cmd, MAX_ARG_LEN-1, "-- br-exists %s", bvhost);
    if (ovs_vsctl_quiet(ovs_bin, ovs_dir, cmd))
      KERR("ERROR OVSCMD: DEL LAN %s: bridge %s was already gone", lan,
           bvhost);
    else
      {
      usleep(100000);
      memset(cmd, 0, MAX_ARG_LEN);
      snprintf(cmd, MAX_ARG_LEN-1, "-- del-br %s", bvhost);
      if (ovs_vsctl(ovs_bin, ovs_dir, cmd))
        KERR("ERROR OVSCMD: DEL LAN %s: bridge %s is STUCK in ovs after two "
             "del-br, nothing will delete it now", lan, bvhost);
      else
        KERR("WARNING OVSCMD: DEL LAN %s: bridge %s went on the second "
             "del-br", lan, bvhost);
      }
    result = -1;
    }
  return result;
}

/* vswitchd makes the interface of a bridge as it reconfigures: waited for
   1 s; a bridge ovs does not hold was deleted with its lan meanwhile. */
static int bridge_intf_up(char *ovs_bin, char *ovs_dir, char *lan,
                          char *bvhost)
{
  char cmd[MAX_ARG_LEN];
  int i;
  for (i=0; (i<20) && (!if_nametoindex(bvhost)); i++)
    usleep(50000);
  if (if_nametoindex(bvhost))
    {
    /* a short wait is vswitchd's pace; from half the limit, a warning */
    if (i >= 10)
      KERR("WARNING OVSCMD: ADD LAN ETH %s: the interface of bridge %s came "
           "%d ms after the add-br", lan, bvhost, i * 50);
    else if (i)
      KTRC("ADD LAN ETH %s: bridge interface %s after %d ms", lan, bvhost,
           i * 50);
    if (!set_intf_flags_iff_up_down(bvhost, 1))
      return 0;
    KERR("ERROR OVSCMD: bridge interface %s cannot be set up", bvhost);
    return -1;
    }
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1, "-- br-exists %s", bvhost);
  if (ovs_vsctl_quiet(ovs_bin, ovs_dir, cmd))
    KERR("WARNING OVSCMD: ADD LAN ETH %s: bridge %s is not in ovs, its lan "
         "was deleted before this endpoint came", lan, bvhost);
  else
    KERR("ERROR OVSCMD: ADD LAN ETH %s: bridge %s is in ovs and its "
         "interface did not appear in 1 s", lan, bvhost);
  return -1;
}

int ovs_cmd_add_lan_endp(char *ovs_bin, char *ovs_dir, char *lan, char *vhost)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];
  char bvhost[MAX_NAME_LEN];
  memset(bvhost, 0, MAX_NAME_LEN);
  snprintf(bvhost, MAX_NAME_LEN-1, "%s%s", OVS_BRIDGE, lan);
  /* Refused before add-port: ovs accepts a port without interface. */
  if (set_intf_flags_iff_up_down(vhost, 1))
    {
    KERR("ERROR OVSCMD: %s cannot be set up, no such interface?", vhost);
    result = -1;
    }
  else if (bridge_intf_up(ovs_bin, ovs_dir, lan, bvhost))
    result = -1;
  else
    {
    memset(cmd, 0, MAX_ARG_LEN);
    snprintf(cmd, MAX_ARG_LEN-1, "-- add-port %s %s", bvhost, vhost);
    if (ovs_vsctl(ovs_bin, ovs_dir, cmd))
      {
      if (ovs_popen_last_said(ovs_dir, "already exists on bridge"))
        {
        KERR("WARNING OVSCMD: ADD LAN ETH %s: port %s was still on a bridge, "
             "removing the leftover and adding it to %s", lan, vhost, bvhost);
        memset(cmd, 0, MAX_ARG_LEN);
        snprintf(cmd, MAX_ARG_LEN-1, "-- --if-exists del-port %s "
                 "-- add-port %s %s", vhost, bvhost, vhost);
        if (ovs_vsctl(ovs_bin, ovs_dir, cmd))
          {
          KERR("ERROR OVSCMD: ADD LAN ETH %s %s %s after clearing the "
               "leftover port", lan, vhost, bvhost);
          result = -1;
          }
        }
      else
        {
        KERR("ERROR OVSCMD: ADD LAN ETH %s %s %s", lan, vhost, bvhost);
        result = -1;
        }
      }
    }
  return result;
}

int ovs_cmd_del_lan_endp(char *ovs_bin, char *ovs_dir, char *lan, char *vhost)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];
  char bvhost[MAX_NAME_LEN];
  memset(bvhost, 0, MAX_NAME_LEN);
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(bvhost, MAX_NAME_LEN-1, "%s%s", OVS_BRIDGE, lan);
  snprintf(cmd, MAX_ARG_LEN-1, "-- del-port %s %s", bvhost, vhost);
  if (ovs_vsctl(ovs_bin, ovs_dir, cmd))
    {
    KERR("ERROR OVSCMD: DEL LAN ETH %s %s %s", lan, vhost, bvhost);
    result = -1;
    }
  return result;
}

