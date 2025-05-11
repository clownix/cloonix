#!/bin/bash
#----------------------------------------------------------------------#
ALPINE0="/var/lib/cloonix/bulk/alpine0"
APK_STATIC="/usr/libexec/cloonix/cloonfs/cloonix-apk-static"
#----------------------------------------------------------------------#
if [ -e ${ALPINE0} ]; then
  echo ERROR ${ALPINE0} exists, please erase
  exit 1
fi
#----------------------------------------------------------------------#
wget --delete-after http://127.0.0.1/alpine/main 1>/dev/null 2>&1
if [ $? -ne 0 ]; then
  echo http://127.0.0.1/alpine/main not reachable
  exit 1
fi
#----------------------------------------------------------------------#
fct_unshare_chroot() {
  unshare --user --fork --pid --mount --mount-proc \
          --map-users=100000,0,10000 --map-groups=100000,0,10000 \
          --setuid 0 --setgid 0 --wd "${ALPINE0}" -- \
          /bin/bash -c " \
                   mount -t proc none proc ; \
                   mount -o rw,bind /dev/zero dev/zero ; \
                   mount -o rw,bind /dev/null dev/null ; \
                   /sbin/chroot . /bin/sh -c '$1'"
}
#----------------------------------------------------------------------#
unshare --user --fork --pid --mount --mount-proc \
        --map-users=100000,0,10000 --map-groups=100000,0,10000 \
        --setuid 0 --setgid 0 -- \
        /bin/bash -c "\
                      mkdir -p ${ALPINE0}/dev ;\
                      mkdir -p ${ALPINE0}/proc"
#----------------------------------------------------------------------#
unshare --user --fork --pid --mount --mount-proc \
        --map-users=100000,0,10000 --map-groups=100000,0,10000 \
        --setuid 0 --setgid 0 --wd "${ALPINE0}" -- \
        /bin/bash -c "\
                      touch dev/zero ; \
                      touch dev/null ; \
                      mount -t proc none proc ; \
                      mount -o rw,bind /dev/zero dev/zero ; \
                      mount -o rw,bind /dev/null dev/null ; \
                      ${APK_STATIC} -X http://127.0.0.1/alpine/main \
                                    --allow-untrusted -p ${ALPINE0} \
                                    --initdb add alpine-base"
#-----------------------------------------------------------------------#
cmd="cat > /etc/apk/repositories <<EOF
http://127.0.0.1/alpine/main
http://127.0.0.1/alpine/community
EOF"
fct_unshare_chroot "${cmd}"
#-----------------------------------------------------------------------#
for i in "musl" \
         "musl-utils" \
         "musl-locales" \
         "util-linux" \
         "tzdata" \
         "qemu-guest-agent" \
         "libcap" \
         "openrc" \
         "alpine-base" \
         "kbd" \
         "kbd-bkeymaps" \
         "dhcpcd" \
         "file" \
         "stress-ng" \
         "strace" \
         "rsyslog"; do
  cmd="/sbin/apk --allow-untrusted --no-cache --update add ${i}"
  fct_unshare_chroot "${cmd}"
done
#-----------------------------------------------------------------------#
for i in "killprocs" \
         "mount-ro" \
         "savecache" ; do
  cmd="/sbin/rc-update add ${i} shutdown"
  fct_unshare_chroot "${cmd}"
done

for i in "hostname" \
         "hwclock" \
         "loopback" \
         "procfs" \
         "sysctl" \
         "devfs" \
         "sysfs" \
         "dmesg" \
         "mdev" \
         "qemu-guest-agent" \
         "cgroups"; do
  cmd="/sbin/rc-update add ${i} boot"
  fct_unshare_chroot "${cmd}"
done
fct_unshare_chroot "/usr/sbin/setup-hostname alpine"
#-----------------------------------------------------------------------#
cmd="passwd root <<EOF
root
root
EOF"
fct_unshare_chroot "${cmd}"
#-----------------------------------------------------------------------#
cmd='cat > /etc/init.d/cloonix_startup << EOF
#!/sbin/openrc-run
name="Cloonix Backdoor"
description="Permits cloonix_ssh and cloonix_scp"
command="/mnt/cloonix_config_fs/init_cloonix_startup_script.sh"
command_background=true
pidfile="/run/cloonix_startup.pid"
EOF'
fct_unshare_chroot "${cmd}"
cmd="chmod +x /etc/init.d/cloonix_startup"
fct_unshare_chroot "${cmd}"
cmd="/sbin/rc-update add cloonix_startup sysinit"
fct_unshare_chroot "${cmd}"
#-----------------------------------------------------------------------#
cmd='echo "noipv4ll" >> /etc/dhcpcd.conf'
fct_unshare_chroot "${cmd}"
#-----------------------------------------------------------------------#
cmd="cat > /etc/apk/repositories << EOF
http://172.17.0.2/alpine/main
http://172.17.0.2/alpine/community
EOF"
fct_unshare_chroot "${cmd}"
#-----------------------------------------------------------------------#

