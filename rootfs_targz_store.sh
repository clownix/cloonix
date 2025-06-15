#!/bin/bash
HERE=`pwd`
TARGZ=${HERE}/targz_store
WORK=${HERE}/work_targz_store
DISTRO="trixie"
REPO="http://127.0.0.1/debian/trixie"
MMDEBSTRAP="/usr/bin/mmdebstrap"
#----------------------------------------------------------------------#
if [ ! -x ${MMDEBSTRAP} ]; then
  echo ${MMDEBSTRAP} not installed
  exit 1
fi
#----------------------------------------------------------------------#
wget --delete-after ${REPO} 1>/dev/null 2>&1
if [ $? -ne 0 ]; then
  echo ${REPO} not reachable
  exit 1
fi
#----------------------------------------------------------------------#
set -e
rm -rf ${WORK}
mkdir -vp ${WORK}
mkdir -p ${TARGZ}
set +e
#----------------------------------------------------------------------#
unshare --map-root-user --user --map-auto --mount -- /bin/bash -c \
        "mount --rbind /proc proc ; \
         mount --rbind /sys sys ; \
         ${MMDEBSTRAP} --variant=minbase \
         --include=systemd,rsyslog,bash-completion \
         ${DISTRO} ${WORK}/trixie_base_rootfs ${REPO}"
#----------------------------------------------------------------------#
set -e
cd ${WORK}
mv trixie_base_rootfs/etc/systemd trixie_base_rootfs
mv trixie_base_rootfs/etc/rsyslog.conf trixie_base_rootfs
mv trixie_base_rootfs/usr/share/bash-completion trixie_base_rootfs
rm -rf trixie_base_rootfs/usr/share
mkdir -p trixie_base_rootfs/share
mv trixie_base_rootfs/bash-completion trixie_base_rootfs/share
rm -rf trixie_base_rootfs/etc
mkdir trixie_base_rootfs/etc
mv trixie_base_rootfs/systemd trixie_base_rootfs/etc
mv trixie_base_rootfs/rsyslog.conf trixie_base_rootfs/etc
#----------------------------------------------------------------------#
rm -f  trixie_base_rootfs/bin
rm -f  trixie_base_rootfs/lib
rm -f  trixie_base_rootfs/lib64
rm -f  trixie_base_rootfs/sbin
for i in "getty" "rmt" "vigr" ; do
  rm -f trixie_base_rootfs/usr/sbin/${i}
done
for i in "awk" "editor" "ex" "nawk" "pager" "rview" "rvim" \
         "vi" "view" "vim" "vimdiff" "which" "captoinfo" \
         "dnsdomainname" "domainname" "i386" "infotocap" \
         "ld.so" "linux32" "linux64" "nisdomainname" "pidof" \
         "rbash" "reset" "run0" "sg" "sh" "systemd-confext" \
         "systemd-umount" "uncompress" "x86_64" \
         "ypdomainname" ; do
  rm -f  trixie_base_rootfs/usr/bin/${i}
done
rm -f  trixie_base_rootfs/usr/lib/os-release

for i in "dpkg-db-backup.timer" "apt-daily-upgrade.timer" \
         "fstrim.timer" "apt-daily.timer" ; do
  rm -f trixie_base_rootfs/etc/systemd/system/timers.target.wants/${i}
done
rm -f trixie_base_rootfs/usr/lib/systemd/systemd-user-sessions
for i in "ssh_config.d" "user.conf.d" "network" \
         "profile.d" "user-environment-generators" \
         "system-preset" "catalog" ; do
  rm -rf trixie_base_rootfs/usr/lib/systemd/${i}
done

for i in "systemd-boot-check-no-failures" "systemd-pstore" \
         "systemd-tpm2-setup" "systemd-reply-password" \
         "systemd-ssh-proxy" "systemd-bsod" \
         "systemd-sleep" "systemd-random-seed" ; do
  rm -f trixie_base_rootfs/usr/lib/systemd/${i}
done

for i in "engines-3" "gconv" "ossl-modules" "perl-base" ; do
  rm -rf trixie_base_rootfs/usr/lib/x86_64-linux-gnu/${i}
done
rm -rf trixie_base_rootfs/usr/libexec/dpkg
rm -rf trixie_base_rootfs/usr/lib/sysusers.d
rm -rf trixie_base_rootfs/usr/lib/pam.d


#----------------------------------------------------------------------#
for i in "dpkg" "init" "kernel" "lsb" "mime" "modprobe.d" \
         "os-releasea" "pcrlock.d" "sysctl.d"  \
         "tmpfiles.d" ; do
  rm -rf  trixie_base_rootfs/usr/lib/${i}
done
rm -rf trixie_base_rootfs/var
rm -rf trixie_base_rootfs/run
rm -rf trixie_base_rootfs/usr/lib/apt
rm -rf trixie_base_rootfs/usr/lib/environment.d
rm -rf trixie_base_rootfs/dev
rm -rf trixie_base_rootfs/boot
rm -rf trixie_base_rootfs/root
rm -rf trixie_base_rootfs/usr/local/*
#----------------------------------------------------------------------#
for i in "system-generators" "user" "user-generators" "user-preset" \
         "system.conf.d" ; do
  rm -rf trixie_base_rootfs/usr/lib/systemd/${i}
done
#----------------------------------------------------------------------#
for i in "rc-local.service.d" "systemd-fsck-root.service.d" \
         "initrd.target.wants" "getty.target.wants" \
         "local-fs.target.wants" "systemd-localed.service.d" \
         "user@0.service.d" "user@.service.d" "user-.slice.d" ; do
  rm -rf trixie_base_rootfs/usr/lib/systemd/system/${i}
done
#----------------------------------------------------------------------#
for i in "systemd-fsck" "systemd-makefs" "systemd-quotacheck" \
         "systemd-sysroot-fstab-check"  "systemd-growfs" \
         "systemd-measure" "systemd-remount-fs" \
         "systemd-sysv-install" "systemd-hibernate-resume" \
         "systemd-modules-load" "systemd-rfkill" "systemd-timedated" \
         "systemd-backlight" "systemd-hostnamed" "systemd-networkd" \
         "systemd-update-done" "systemd-battery-check" \
         "systemd-networkd-wait-online" "systemd-socket-proxyd" \
         "systemd-user-runtime-dir" "systemd-binfmt" \
         "systemd-network-generator" "systemd-storagetm" \
         "systemd-volatile-root" "systemd-cgroups-agent" \
         "systemd-localed" "systemd-pcrextend" \
         "systemd-sulogin-shell" "systemd-xdg-autostart-condition" \
         "systemd-logind" "systemd-pcrlock" ; do
  rm -f trixie_base_rootfs/usr/lib/systemd/${i}
done
#----------------------------------------------------------------------#
for i in  \
"initrd-cleanup.service" \
"initrd-fs.target" \
"initrd-parse-etc.service" \
"initrd-root-device.target" \
"initrd-root-fs.target" \
"initrd-switch-root.service" \
"initrd-switch-root.target" \
"initrd.target" \
"initrd-udevadm-cleanup-db.service" \
"initrd-usr-fs.target" \
"systemd-pcrlock-file-system.service" \
"systemd-pcrlock-machine-id.service" \
"systemd-pcrlock-secureboot-policy.service" \
"systemd-pcrlock-firmware-code.service" \
"systemd-pcrlock-make-policy.service" \
"systemd-pcrlock@.service" \
"systemd-pcrlock-firmware-config.service" \
"systemd-pcrlock-secureboot-authority.service" \
"systemd-pcrlock.socket" \
"runlevel0.target" \
"runlevel1.target" \
"runlevel2.target" \
"runlevel3.target" \
"runlevel4.target" \
"runlevel5.target" \
"runlevel6.target" \
"systemd-networkd-persistent-storage.service" \
"systemd-networkd.socket" \
"systemd-networkd-wait-online@.service" \
"systemd-networkd.service" \
"systemd-networkd-wait-online.service" ; do
  rm -f trixie_base_rootfs/usr/lib/systemd/system/${i}
done
#----------------------------------------------------------------------#
for i in  \
"getty.target.wants" \
"sysinit.target.wants" \
"timers.target.wants" ; do
  rm -rf trixie_base_rootfs/etc/systemd/system/${i}
done
for i in  \
  "multi-user.target.wants" \
  ; do
  rm -rf trixie_base_rootfs/usr/lib/systemd/system/${i}
done
#----------------------------------------------------------------------#
for i in  \
"dev-hugepages.mount" \
"kmod-static-nodes.service" \
"ldconfig.service" \
"systemd-hibernate-clear.service" \
"systemd-machine-id-commit.service" \
"systemd-modules-load.service" \
"systemd-pcrmachine.service" \
"systemd-pcrphase.service" \
"systemd-pcrphase-sysinit.service" \
  ; do
  rm -f trixie_base_rootfs/usr/lib/systemd/system/sysinit.target.wants/${i}
done

for i in  \
"ctrl-alt-del.target" "kmod.service" \
"dbus-org.freedesktop.login1.service" \
"dbus-org.freedesktop.locale1.service" \
"dbus-org.freedesktop.timedate1.service" \
"dbus-org.freedesktop.hostname1.service" \
"hwclock.service" "cryptdisks-early.service" \
"cryptdisks.service" "x11-common.service" "autovt@.service" \
  ; do
rm -f trixie_base_rootfs/usr/lib/systemd/system/${i}
done

for i in  \
"systemd-journald-dev-log.socket" \
"systemd-pcrextend.socket" \
"systemd-sysext.socket" \
"systemd-initctl.socket" \
"systemd-pcrlock.socket" \
"systemd-hostnamed.socket" \
"systemd-creds.socket" \
  ; do
rm -f trixie_base_rootfs/usr/lib/systemd/system/sockets.target.wants/${i}
done


for i in  \
"sys-kernel-config.mount" \
"systemd-random-seed.service" \
"proc-sys-fs-binfmt_misc.automount" \
"sys-kernel-tracing.mount" \
"sys-kernel-debug.mount" \
"systemd-sysctl.service" \
"systemd-binfmt.service" \
"systemd-journal-catalog-update.service" \
"systemd-sysusers.service" \
"sys-fs-fuse-connections.mount" \
"systemd-update-done.service" \
"systemd-tmpfiles-setup-dev.service" \
"systemd-tmpfiles-setup-dev-early.service" \
"systemd-journal-flush.service" \
"systemd-firstboot.service" \
"systemd-tpm2-setup.service" \
"systemd-ask-password-console.path" \
"systemd-tmpfiles-setup.service" \
"systemd-tpm2-setup-early.service" \
  ; do
rm -f trixie_base_rootfs/usr/lib/systemd/system/sysinit.target.wants/${i}
done



#----------------------------------------------------------------------#
for i in  \
"apt-daily.service" \
"apt-daily.timer" \
"apt-daily-upgrade.service" \
"apt-daily-upgrade.timer" \
"autovt@.service" \
"blockdev@.target" \
"bluetooth.target" \
"capsule@.service" \
"capsule.slice" \
"console-getty.service" \
"container-getty@.service" \
"cryptdisks-early.service" \
"cryptdisks.service" \
"ctrl-alt-del.target" \
"dbus-org.freedesktop.hostname1.service" \
"dbus-org.freedesktop.locale1.service" \
"dbus-org.freedesktop.login1.service" \
"dbus-org.freedesktop.timedate1.service" \
"debug-shell.service" \
"dev-hugepages.mount" \
"dpkg-db-backup.service" \
"dpkg-db-backup.timer" \
; do

  rm -f trixie_base_rootfs/usr/lib/systemd/system/${i}
done
#----------------------------------------------------------------------#
for i in  \
"systemd-pcrextend@.service" \
"systemd-pcrfs-root.service" \
"systemd-pcrmachine.service" \
"systemd-pcrphase.service" \
"systemd-pcrextend.socket" \
"systemd-pcrfs@.service" \
"systemd-pcrphase-initrd.service" \
"systemd-pcrphase-sysinit.service" \
"systemd-ask-password-console.path" \
"systemd-ask-password-console.service" \
"systemd-ask-password-wall.path" \
"systemd-ask-password-wall.service" \
"systemd-backlight" \
"systemd-battery-check.service" \
"systemd-bsod.service" \
"systemd-creds@.service" \
"systemd-creds.socket" \
"systemd-fsck-root.service" \
"systemd-fsck@.service" \
"systemd-growfs-root.service" \
"systemd-growfs@.service" \
"systemd-hibernate-clear.service" \
"systemd-hibernate-resume.service" \
"systemd-hibernate.service" \
"systemd-hybrid-sleep.service" \
; do
  rm -f trixie_base_rootfs/usr/lib/systemd/system/${i}
done
#----------------------------------------------------------------------#
for i in  \
"emergency.service" \
"emergency.target" \
"factory-reset.target" \
"fstrim.service" \
"fstrim.timer" \
"getty-pre.target" \
"getty@.service" \
"getty-static.service" \
"hibernate.target" \
"hwclock.service" \
"hybrid-sleep.target" \
"kmod.service" \
"kmod-static-nodes.service" \
"quotaon-root.service" \
"quotaon@.service" \
"rescue.service" \
"rescue.target" \
"rpcbind.target" \
"sigpwr.target" \
"smartcard.target" \
"sound.target" \
"ssh-access.target" \
"storage-target-mode.target" \
"suspend.target" \
"suspend-then-hibernate.target" \
"swap.target" \
"systemd-machine-id-commit.service" \
"systemd-modules-load.service" \
"systemd-network-generator.service" \
"systemd-pstore.service" \
"systemd-quotacheck-root.service" \
"systemd-quotacheck@.service" \
"systemd-rfkill.service" \
"systemd-rfkill.socket" \
"systemd-storagetm.service" \
"systemd-suspend-then-hibernate.service" \
"systemd-suspend.service" \
"systemd-timedated.service" \
"systemd-volatile-root.service" \
"system-update-cleanup.service" \
"system-update-pre.target" \
"system-update.target" \
"usb-gadget.target" \
; do
  rm -f trixie_base_rootfs/usr/lib/systemd/system/${i}
done
#----------------------------------------------------------------------#
for i in  \
"ldconfig.service" \
"kexec.target" \
"modprobe@.service" \
"network-online.target" \
"network-pre.target" \
"network.target" \
"nss-lookup.target" \
"nss-user-lookup.target" \
"pam_namespace.service" \
"printer.target" \
"serial-getty@.service" \
"sys-fs-fuse-connections.mount" \
"systemd-backlight@.service" \
"systemd-hostnamed.service" \
"systemd-hostnamed.socket" \
"systemd-journal-catalog-update.service" \
"systemd-journald-audit.socket" \
"systemd-journald-dev-log.socket" \
"systemd-journald-varlink@.socket" \
"systemd-kexec.service" \
"systemd-sysext.service" \
"systemd-sysext@.service" \
"systemd-sysext.socket" \
"systemd-tpm2-setup-early.service" \
"systemd-tpm2-setup.service" \
"systemd-update-done.service" \
"tpm2.target" \
; do
  rm -f trixie_base_rootfs/usr/lib/systemd/system/${i}
done
#----------------------------------------------------------------------#




for i in "media" "mnt" "opt" "proc" "srv" "sys" "tmp" \
         "usr/games" "usr/local" "usr/src" "home" \
         "usr/include" ; do
  rmdir trixie_base_rootfs/${i}
done

for i in "bin" "sbin" "libexec" "lib" "lib64" ; do
  mv trixie_base_rootfs/usr/${i} trixie_base_rootfs
done

rmdir trixie_base_rootfs/usr

cat > trixie_base_rootfs/etc/rsyslog.conf << "EOF"
module(load="imuxsock") 
$FileOwner root
$FileGroup root
$FileCreateMode 0640
$DirCreateMode 0755
$Umask 0022
$WorkDirectory /var/spool/rsyslog
*.*;auth,authpriv.none          -/var/log/syslog
EOF
cat > trixie_base_rootfs/lib/systemd/system/rsyslog.service << "EOF"
[Unit]
Description=System Logging Service
Requires=syslog.socket

[Service]
Type=notify
ExecStart=/usr/sbin/rsyslogd -n -iNONE
StandardOutput=null
Restart=on-failure
LimitNOFILE=16384

[Install]
WantedBy=multi-user.target
Alias=syslog.service
EOF

cat > trixie_base_rootfs/etc/systemd/system/cloonix.service << "EOF"
[Unit]
Description=cloonix
After=rsyslog

[Service]
Type=forking
ExecStart=/usr/bin/cloonix_net __IDENT__

[Install]
WantedBy=multi-user.target
EOF


cd ${WORK}/trixie_base_rootfs/etc/systemd/system/multi-user.target.wants
ln -s /etc/systemd/system/cloonix.service cloonix.service
cd ${WORK}

rm -f ${WORK}/trixie_base_rootfs/lib64/ld-linux-x86-64.so.2
mv ${WORK}/trixie_base_rootfs/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 \
   ${WORK}/trixie_base_rootfs/lib64
cd ${WORK}/trixie_base_rootfs

#----------------------------------------------------------------------#
unshare -rm tar zcvf ${TARGZ}/trixie_base_rootfs.tar.gz .
rm -rf ${WORK}/trixie_base_rootfs
rmdir ${WORK}
#----------------------------------------------------------------------#

