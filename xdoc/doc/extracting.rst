.. image:: /png/cloonix128.png 
   :align: right

===================
Cloonix extraction
===================

At https://makeself.io/, there is a tool called makeself.sh that is 
usefull to help in the installation of a new directory followed by
the running of a script.
Thanks to this tool, the **self_extracting_cloonix.sh** creates the
**self_extracting_rootfs_dir** directory and prints the commands that
the user has to type to run the demo.


===================================
self_extracting_rootfs_dir content
===================================

**rootfs**:
This is the root file-system for the container that embeds the cloonix software.

**config**:
config.json: this file is used by crun, this file has the Open Container Initiative (OCI) format.
readme.sh: it is the script that is run at the end of extraction to give the commands to be run by the user.
cloonix-init-starter-crun: process one of the crun container. 

**crun_container_startup.sh**:
this script is the start launched on the host, it will run the proxy cloonix-proxy-crun-access
on the host and run the crun container.

**bin**:
binaries and libraries for the autonomous run on the host which can have unadequate libraries.


===================
Case of the KVM
===================

For the cloonix running in a container to be able to use /dev/kvm,
/dev/vhost-net and /dev/net/tun, some commands must be done as root,
here is the way to check that your user can read/write those devices
and the way to authorise their use ::

    getfacl /dev/kvm
    getfacl /dev/vhost-net
    getfacl /dev/net/tun
    
    sudo setfacl -R -m u:${USER}:rw /dev/kvm
    sudo setfacl -R -m u:${USER}:rw /dev/vhost-net
    sudo setfacl -R -m u:${USER}:rw /dev/net/tun


In case of problems, the following should work::

    sudo chmod 0666 /dev/kvm
    sudo chmod 0666 /dev/vhost-net
    sudo chmod 0666 /dev/net/tun

