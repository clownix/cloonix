.. image:: /png/cloonix128.png 
   :align: right

==================
Cloonix frr toy
==================

With the targz installation, the cloonix bundle is packed so that
its installation is made easy, juste call ::

  tar xvf cloonix-toy-__LAST__.tar.gz
  cd cloonix-toy-__LAST__
  ./setup.sh

To use cloonix-toy, a script named **nemocmd** is used, the prefix
nemo here can be another cloonix network found in cloonixr.cfg.
is created by the setup.sh, at the end of the setup you have the
following text for my host's USER which is perrier ::

    	/dev/kvm usable as perrier
    	/dev/vhost-net usable as perrier
    	/dev/net/tun usable as perrier
    	perrier should be able to use kvm (qemu)
    	trixie.qcow exists in bulk, kvm demos should work
    	mac80211_hwsim loaded wifi emulation should work
    
    	CHOICE:
    ./nemocmd start
    ./nemocmd stop
    ./nemocmd clean
    ./nemocmd shell
    ./nemocmd canvas
    ./nemocmd web_on
    ./nemocmd web_off
    ./nemocmd demo <param>


If the user is not in the kvm group to be able to use the kvm modules,
and if the mac80211_hwsim is not loaded, warnings can be seen ::

	/dev/kvm usable as user
	/dev/vhost-net not usable, to use qemu try:
	usermod -a -G kvm user
	mac80211_hwsim not loaded wifi emulation will not work


Amongs the commands, here are the advised commands for your first try ::

  ./nemocmd start
  ./nemocmd canvas
  ./nemocmd demo


Upon the command **./nemocmd demo** you will get the following ::

  ~$ ./nemocmd demo
  ./nemocmd demo frrline
  ./nemocmd demo frrspider
  ./nemocmd demo frrsquare
  ./nemocmd demo hwsim_cnt.sh
  ./nemocmd demo hwsim_kvm.sh
  ./nemocmd demo ping_cnt.sh
  ./nemocmd demo ping_kvm.sh

Amongs the commands, here is the advised command for your first try ::

  ./nemocmd demo ping_cnt.sh


Note:
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


content for the toy directory
=============================


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
incompatible libraries.
An important shell file in this directory is *cloonix-proxymous-nemo*, this
is the equivalent of the init file for a linux machine or an entrypoint for
a docker.

**worktmp**:
--------
For debug, logs of the crun messages are stored there.


Case of the qemu-kvm
====================

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

Case of the wifi emulation
==========================

To have the support for wifi emulation, the module mac80211_hwsim ::

    sudo modprobe mac80211_hwsim
