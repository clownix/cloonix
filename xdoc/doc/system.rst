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


Wireshark
=========

Make wireshark usable by non-root user, on debian the way to go is::

    echo "wireshark-common wireshark-common/install-setuid boolean true" > preseed
    sudo debconf-set-selections preseed
    sudo dpkg-reconfigure  wireshark
    sudo adduser ${USER} wireshark

Or just set the suid bit of /usr/bin/dumpcap::

    chown root:root /usr/bin/dumpcap
    chmod +s /usr/bin/dumpcap
    chmod +x /usr/bin/dumpcap


Docker and Podman
=================

Your system must have Docker or Podman or both to be able to use docker or podman containers.
The container image creation must be done outside of cloonix, cloonix can only launch existing images.
