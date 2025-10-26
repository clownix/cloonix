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
char *pthexec_erase_workdir(void);
char *pthexec_agent_iso_amd64(void);
char *pthexec_lsmod_bin(void);
char *pthexec_modprobe_bin(void);
char *pthexec_iw_bin(void);
char *pthexec_xephyr_bin(void);
char *pthexec_xwycli_bin(void);
char *pthexec_urxvt_bin(void);
char *pthexec_wireshark_bin(void);
char *pthexec_dumpcap_bin(void);
char *pthexec_crun_bin(void);
char *pthexec_ovsdb_server_bin(void);
char *pthexec_ovs_vswitchd_bin(void);
char *pthexec_ovsdb_tool_bin(void);
char *pthexec_fusezip_bin(void);
char *pthexec_osirrox_bin(void);
char *pthexec_umount_bin(void);
char *pthexec_mount_bin(void);
char *pthexec_chown_bin(void);
char *pthexec_xauth_bin(void);
char *pthexec_bash_bin(void);
char *pthexec_touch_bin(void);
char *pthexec_nsenter_bin(void);
char *pthexec_ip_bin(void);
char *pthexec_awk_bin(void);
char *pthexec_grep_bin(void);
char *pthexec_ps_bin(void);
char *pthexec_mknod_bin(void);
char *pthexec_chmod_bin(void);
char *pthexec_sleep_bin(void);
char *pthexec_cloonix_cfg(void);
char *pthexec_bin_xvfb(void);
char *pthexec_bin_xsetroot(void);
char *pthexec_bin_wm2(void);
char *pthexec_bin_x11vnc(void);
char *pthexec_bin_nginx(void);
char *pthexec_bin_websockify(void);
char *pthexec_web(void);
char *pthexec_cert(void);
char *pthexec_dropbear_ssh(void);
char *pthexec_dropbear_scp(void);
char *pthexec_qt6_plugins(void);
char *pthexec_cloonfs_path(void);
char *pthexec_cloonfs_share(void);
char *pthexec_pipewire_module_dir(void);
char *pthexec_gst_plugin_scanner(void);
char *pthexec_gst_plugin_path(void);
char *pthexec_u2i_scp_bin(void);
char *pthexec_u2i_ssh_bin(void);
char *pthexec_ctrl_bin(void);
char *pthexec_gui_bin(void);
char *pthexec_dtach_bin(void);
char *pthexec_spicy_bin(void);
char *pthexec_ovs_vsctl_bin(void);
char *pthexec_png_clownix64(void);
char *pthexec_cloonfs_share_novnc(void);
char *pthexec_usr_libexec_cloonix(void);
char *pthexec_cloonfs_lib64(void);
char *pthexec_cloonfs_etc(void);
char *pthexec_cloonfs_ovs_vswitchd(void);
char *pthexec_cloonfs_ovs_drv(void);
char *pthexec_cloonfs_ovs_ovsbd(void);
char *pthexec_cloonfs_qemu_img(void);
char *pthexec_cloonfs_suid_power(void);
char *pthexec_cloonfs_doorways(void);
char *pthexec_cloonfs_vwifi_server(void);

int pthexec_running_in_crun(char *name);

void pthexec_init(void);
/*--------------------------------------------------------------------------*/



