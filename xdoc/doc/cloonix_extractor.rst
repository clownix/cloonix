.. image:: /png/cloonix128.png 
   :align: right

==================
Cloonix extractor
==================

With the tool called makeself.sh, the cloonix bundle is packed so that
its installation is made easy, juste call the self-extracting script ::

  ./cloonix_extractor.sh

At the end of this installation, a new directory is created, named
**dir_self_extracted** filled with all the necessary cloonix tools and
binaries. To use it, some sort of user handle script named **nemocmd**
is created with the directory. This script gives the following choices ::

  ./nemocmd start
  ./nemocmd stop
  ./nemocmd shell
  ./nemocmd canvas
  ./nemocmd demo_ping
  ./nemocmd demo_frr
  ./nemocmd web_on
  ./nemocmd web_off

Amongs these commands, here are the advised commands for your first try ::

  ./nemocmd start
  ./nemocmd canvas
  ./nemocmd demo_ping

Warning in case of failure: For fedora I had to change the selinux
configuration, set SELINUX=disabled in /etc/sysconfig/selinux.
For ubuntu, I had to add the following lines to /etc/sysctl.conf:
kernel.unprivileged_userns_clone=1 and
kernel.apparmor_restrict_unprivileged_userns=0.


When the cloonix is started inside the crun container, you can dive inside
this container with ::

  ./nemocmd shell

In this shell, you can do commands like "ps -ef" to see all the processes
running for cloonix.


dir_self_extracted content
==========================


**rootfs**:
-----------

This is the root file-system given to the crun container, it has all the
features of a small linux system and it embeds the cloonix binaries and
libraries.


**config**:
-----------

*config.json:* this file is used by crun, this file has the Open Container
Initiative (OCI) format. It gives the directives to create the container
to the crun binary.

*readme.sh:* it is the script that is run at the end of the directory
extraction, ip creates the ./nemocmd that gives the commands to be run by
the user.


**bin**:
--------
binaries and libraries for the autonomous run on the host which can have
uncompatible libraries.
An important shell file in this directory is *cloonix-proxymous-nemo*, this
is the equivalent of the init file for a linux machine or an entrypoint for
a docker.

**log**:
--------
For debug, logs of the crun messages are stored there.


Case of the KVM
===================

For the cloonix running in a container to be able to use */dev/kvm*,
*/dev/vhost-net* and */dev/net/tun*, some commands must be done as root,
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

