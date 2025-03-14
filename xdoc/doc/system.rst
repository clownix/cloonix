.. image:: /png/cloonix128.png 
   :align: right

==================================
Host system required customisation
==================================

Intel VT-x & AMD-V Virtualisation support
=========================================

You can use KVM, which is necessary for cloonix if the result of the 
following is non-zero::

    egrep -c '(vmx|svm)' /proc/cpuinfo

This can be activated from bios.


KVM module auto insertion
=========================

A process that wants to use the /dev/kvm may need some priviledges.
Here is how those priviledges can be given this way::

    chmod 666 /dev/kvm

For a more secure way, here are the commands if you want to allow
the existing user bob access to /dev/kvm::

    addgroup --system kvm
    adduser bob kvm
    chown root:kvm /dev/kvm
    chmod 0660 /dev/kvm

If the group kvm and bob both exist, you can do::

    usermod -a -G kvm bob

The goal is to see the kvm group in the id::

    id bob
    uid=1001(bob) gid=1001(bob) groupes=1001(bob),125(kvm)

After this modification, you must relog and relaunch cloonix_net.


If problems, a bug in the acl of this file exists you can try::

    getfacl /dev/kvm
    setfacl --remove-all /dev/kvm
    chown root:kvm /dev/kvm
    chmod 0660 /dev/kvm


Automatic insert at startup::

    echo kvm_intel >> /etc/modules 

    cat > /etc/udev/rules.d/65-kvm.rules << EOF
    KERNEL=="kvm", NAME="%k", GROUP="kvm", MODE="0666"
    EOF 

Sometimes, in case of containers, the group kvm does not work to allow the use
of the /dev/vhost-net by the container. If <USER> is the user that launches the
crun container, then the following commands on the host can be used::

    sudo setfacl -m u:<USER>:rw /dev/kvm
    sudo setfacl -m u:<USER>:rw /dev/vhost-net
    sudo setfacl -m u:<USER>:rw /dev/net/tun

For me::

    sudo setfacl -m u:perrier:rw /dev/vhost-net

Ther is a problem with the access to /dev/vhost-net even though I am in kvm group...

In case of problems, the following should work::

    sudo chmod 0666 /dev/kvm
    sudo chmod 0666 /dev/vhost-net
    sudo chmod 0666 /dev/net/tun

Nested guests customisation
===========================

Nested KVM works only if the KVM module is inserted with the "nested" 
parameter.  
To check that your system is configured to be able to have nested kvm::

    cat /sys/module/kvm_intel/parameters/nested

If N is the response, then do:: 

  lsmod | grep kvm
 
Then if you have the kvm_intel (or kvm_amd) and the kvm modules inserted,
remove them::

    sudo rmmod kvm_intel
    sudo rmmod kvm

Then insert the module with a parameter forcing the nested function::

    sudo modprobe kvm_intel nested=1
    lsmod |grep kvm
    cat /sys/module/kvm_intel/parameters/nested

You should get a Y now.

Cloonix is "nested-aware", it detects that it is inside another cloonix.

Commands to have nested KVM in intel automaticaly from start of machine::

   * update the ‘/etc/default/grub‘ file and append ‘kvm-intel.nested=1‘ to the ‘GRUB_CMDLINE_LINUX‘ line. 
   * sudo grub-mkconfig -o /boot/grub/grub.cfg
   * grep nested /boot/grub/grub.cfg
   * reboot 
  
Note that for grub2, it is: grub2-mkconfig --output=/boot/grub2/grub.cfg



Podman and Crun startup
=======================

If you want to customize the startup of the containers, you can create
the following files::

  /usr/bin/cloonix_startup_script.sh

Podman
=======

Your system must have Podman to be able to use podman containers.
The container image creation must be done outside of cloonix, cloonix can
only launch existing podman images.


Crun console
============

The cloonix_ssh and cloonix_scp should work but in case of a bug, if you
think that the container is running, the following commands list the crun
container running and launch a shell in the Cnt1 if it exists::

  sudo /usr/libexec/cloonix/server/cloonix-crun --root=/var/lib/cloonix/nemo/crun/ list
  sudo /usr/libexec/cloonix/server/cloonix-crun --root=/var/lib/cloonix/nemo/crun/ exec Cnt1 sh 


Mounts and namespaces
=====================

The file system seen by the crun is private but you can get to see it
with the following commands::

  ps aux | grep "cloonix-suid-power nemo" | grep -v grep | awk "{print \$2}"
  14022
  sudo nsenter --mount=/proc/14022/ns/mnt
  ls /var/lib/cloonix/nemo/mnt/busybox.zip

qemu-guest-agent
================

The launch in the kvm machines of the cloonix agent uses the qemu-guest-agent
also named qemu-ga.
For both fedora and centos, the selinux linux prevents the work of qemu-ga
set SELINUX=disabled in /etc/sysconfig/selinux.
For centos, in /etc/sysconfig/qemu-ga, in FILTER_RPC_ARGS I had to add:
guest-file-open,guest-file-close,guest-file-read,guest-file-write,
guest-exec-status,guest-exe.


