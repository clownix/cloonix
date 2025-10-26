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
#include <stdlib.h>
#include <stdio.h>

#include "glob_common.h"



static int g_init_done=0;
static int g_is_in_crun;

/****************************************************************************/
int sock_header_get_size(void)
{
  return 16;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void sock_header_set_info(char *tx, int llid_dido,
                          int vwifi_base, int vwifi_cid,
                          int type, int val, int len, char **ntx)
{
  tx[0] = ((llid_dido & 0xFF00) >> 8) & 0xFF;
  tx[1] = llid_dido & 0xFF;
  tx[2] = ((vwifi_base & 0xFF00) >> 8) & 0xFF;
  tx[3] = vwifi_base & 0xFF;
  tx[4] = ((vwifi_cid & 0xFF00) >> 8) & 0xFF;
  tx[5] = vwifi_cid & 0xFF;
  tx[6] = ((len & 0xFF00) >> 8) & 0xFF;
  tx[7] = len & 0xFF;
  tx[8] = ((type & 0xFF00) >> 8) & 0xFF;
  tx[9] = type & 0xFF;
  tx[10] = ((val & 0xFF00) >> 8) & 0xFF;
  tx[11] = val & 0xFF;
  tx[12] = 0xCA;
  tx[13] = 0xFE;
  tx[14] = 0xDE;
  tx[15] = 0xCA;
  *ntx = &(tx[16]);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int sock_header_get_info(char *rx, int *llid_dido,
                         int *vwifi_base, int *vwifi_cid,
                         int *type, int *val, int *len, char **nrx)
{
  int result  = -1;
  *llid_dido  = ((rx[0] & 0xFF) << 8) + (rx[1] & 0xFF);
  *vwifi_base = ((rx[2] & 0xFF) << 8) + (rx[3] & 0xFF);
  *vwifi_cid  = ((rx[4] & 0xFF) << 8) + (rx[5] & 0xFF);
  *len        = ((rx[6] & 0xFF) << 8) + (rx[7] & 0xFF);
  *type       = ((rx[8] & 0xFF) << 8) + (rx[9] & 0xFF);
  *val        = ((rx[10] & 0xFF) << 8) + (rx[11] & 0xFF);
  if (((rx[12] & 0xFF) == 0xCA) &&
      ((rx[13] & 0xFF) == 0xFE) &&
      ((rx[14] & 0xFF) == 0xDE) &&
      ((rx[15] & 0xFF) == 0xCA))
    {
    result = 0;
    }
  else
    {
    *llid_dido = 0;
    *vwifi_base = 0;
    *vwifi_cid = 0;
    *len  = 0;
    *type = 0;
    *val  = 0;
    KERR("%02X %02X %02X %02X %02X %02X",
         (unsigned int) (rx[10] & 0xFF), (unsigned int) (rx[11] & 0xFF),
         (unsigned int) (rx[12] & 0xFF), (unsigned int) (rx[13] & 0xFF),
         (unsigned int) (rx[14] & 0xFF), (unsigned int) (rx[15] & 0xFF));
    }
  *nrx  = &(rx[16]);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int pthexec_running_in_crun(char *name)
{
  int result = 0;
  char *file_name = "/etc/systemd/system/cloonix_server.service";
  char line[MAX_PATH_LEN];
  char *ptr, *ptr_start, *ptr_end;
  FILE *fp = fopen(file_name, "r");
  if (fp != NULL)
    {
    while(fgets(line, MAX_PATH_LEN-1, fp))
      {
      ptr = strstr(line, "ExecStart=/usr/bin/cloonix_net");
      if (ptr)
        {
        if (!strstr(line, "__IDENT__"))
          {
          result = 1;
          ptr_start = strchr(ptr, ' ');
          if (!ptr_start)
            KERR("ERROR %s", line);
          else if (name)
            {
            ptr_start = ptr_start+1;
            ptr_end = ptr_start + strcspn(ptr_start, " \r\n\t");
            *ptr_end = 0;
            memset(name, 0, MAX_NAME_LEN);
            strncpy(name, ptr_start, MAX_NAME_LEN-1);
            }
          }
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/


/****************************************************************************/
char *pthexec_erase_workdir(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-suid-erase-workdir");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-suid-erase-workdir");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_agent_iso_amd64(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/insider_agents/insider_agent_x86_64.iso");
  else
    return("/usr/libexec/cloonix/insider_agents/insider_agent_x86_64.iso");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_lsmod_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/sbin/lsmod");
  else
    return("/usr/libexec/cloonix/cloonfs/sbin/lsmod");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_modprobe_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/sbin/modprobe");
  else
    return("/usr/libexec/cloonix/cloonfs/sbin/modprobe");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_iw_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/sbin/iw");
  else
    return("/usr/libexec/cloonix/cloonfs/sbin/iw");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_xephyr_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-novnc-xephyr");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-novnc-xephyr");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_xwycli_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-xwycli");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-xwycli");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_urxvt_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-urxvt");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-urxvt");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_wireshark_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-wireshark");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-wireshark");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_dumpcap_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/dumpcap");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/dumpcap");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_crun_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-crun");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-crun");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_ovsdb_server_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-ovsdb-server");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-ovsdb-server");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_ovs_vswitchd_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-ovs-vswitchd");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-ovs-vswitchd");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_ovsdb_tool_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-ovsdb-tool");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-ovsdb-tool");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_fusezip_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-fuse-zip");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-fuse-zip");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_osirrox_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/xorriso");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/xorriso");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_umount_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/umount");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/umount");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_mount_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/mount");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/mount");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_chown_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/chown");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/chown");
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
char *pthexec_xauth_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/xauth");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/xauth");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_bash_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/bash");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/bash");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_touch_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/touch");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/touch");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_nsenter_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/nsenter");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/nsenter");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_ip_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/ip");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/ip");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_awk_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/awk");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/awk");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_grep_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/grep");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/grep");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_ps_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/ps");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/ps");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_mknod_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/mknod");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/mknod");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_chmod_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/chmod");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/chmod");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_sleep_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/sleep");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/sleep");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_cloonix_cfg(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/etc/cloonix.cfg");
  else
    return("/usr/libexec/cloonix/cloonfs/etc/cloonix.cfg");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_bin_xvfb(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-novnc-Xvfb");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-novnc-Xvfb");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_bin_xsetroot(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-novnc-xsetroot");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-novnc-xsetroot");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_bin_wm2(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-novnc-wm2");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-novnc-wm2");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_bin_x11vnc(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-novnc-x11vnc");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-novnc-x11vnc");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_bin_nginx(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-novnc-nginx");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-novnc-nginx");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_bin_websockify(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-novnc-websockify");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-novnc-websockify");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_web(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("--web=/cloonix/cloonfs/share/noVNC/");
  else
    return("--web=/usr/libexec/cloonix/cloonfs/share/noVNC/");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_cert(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("--cert=/cloonix/cloonfs/bin/novnc.pem");
  else
    return("--cert=/usr/libexec/cloonix/cloonfs/bin/novnc.pem");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_dropbear_ssh(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-dropbear-ssh");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-dropbear-ssh");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_dropbear_scp(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-dropbear-scp");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-dropbear-scp");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_qt6_plugins(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/lib/x86_64-linux-gnu/qt6/plugins");
  else
    return("/usr/libexec/cloonix/cloonfs/lib/x86_64-linux-gnu/qt6/plugins");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_cloonfs_path(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin:/usr/libexec/cloonix/cloonfs/sbin");
  else
    return("/usr/libexec/cloonix/cloonfs/bin:/usr/libexec/cloonix/cloonfs/sbin");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_cloonfs_share(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/share");
  else
    return("/usr/libexec/cloonix/cloonfs/share");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_pipewire_module_dir(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/lib/x86_64-linux-gnu/pipewire-0.3");
  else
    return("/usr/libexec/cloonix/cloonfs/lib/x86_64-linux-gnu/pipewire-0.3");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_gst_plugin_scanner(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/lib/x86_64-linux-gnu/gstreamer1.0/gstreamer-1.0/gst-plugin-scanner");
  else
    return("/usr/libexec/cloonix/cloonfs/lib/x86_64-linux-gnu/gstreamer1.0/gstreamer-1.0/gst-plugin-scanner");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_gst_plugin_path(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/lib/x86_64-linux-gnu/gstreamer-1.0");
  else
    return("/usr/libexec/cloonix/cloonfs/lib/x86_64-linux-gnu/gstreamer-1.0");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_u2i_scp_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-u2i-scp");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-u2i-scp");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_u2i_ssh_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-u2i-ssh");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-u2i-ssh");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_ctrl_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-ctrl");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-ctrl");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_gui_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT gui_bin");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-gui");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-gui");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_dtach_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-dtach");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-dtach");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_spicy_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-spicy");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-spicy");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_ovs_vsctl_bin(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-ovs-vsctl");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-ovs-vsctl");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_png_clownix64(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/etc/clownix64.png");
  else
    return("/usr/libexec/cloonix/cloonfs/etc/clownix64.png");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_cloonfs_share_novnc(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/share/noVNC");
  else
    return("/usr/libexec/cloonix/cloonfs/share/noVNC");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_usr_libexec_cloonix(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix");
  else
    return("/usr/libexec/cloonix");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_cloonfs_lib64(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/lib64");
  else
    return("/usr/libexec/cloonix/cloonfs/lib64");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_cloonfs_etc(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/etc");
  else
    return("/usr/libexec/cloonix/cloonfs/etc");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_cloonfs_ovs_vswitchd(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-ovs-vswitchd");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-ovs-vswitchd");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_cloonfs_ovs_drv(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-ovs-drv");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-ovs-drv");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_cloonfs_ovs_ovsbd(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-ovsdb-server");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-ovsdb-server");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_cloonfs_qemu_img(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-qemu-img");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-qemu-img");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_cloonfs_suid_power(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-suid-power");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-suid-power");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_cloonfs_doorways(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-doorways");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-doorways");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *pthexec_cloonfs_vwifi_server(void)
{
  if (g_init_done != 1)
    KOUT("ERROR NO INIT");
  if (g_is_in_crun)
    return("/cloonix/cloonfs/bin/cloonix-vwifi-server");
  else
    return("/usr/libexec/cloonix/cloonfs/bin/cloonix-vwifi-server");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void pthexec_init(void)
{
  if (g_init_done == 0)
    {
    g_init_done = 1;
    if (pthexec_running_in_crun(NULL))
      g_is_in_crun = 1;
    else
      g_is_in_crun = 0;
    }
}
/*--------------------------------------------------------------------------*/


