.. image:: /png/cloonix128.png 
   :align: right

============================
Software Release Information
============================



v53-00
======

This version includes vwifi to have wlan interfaces for wifi
simulation. It is based on: https://github.com/Raizo62/vwifi
This release is again a beta version.
If anybody knows how to connect the wlan0 interface of a kvm
to a wlan0 interface of a container, send a message.

As usual no time for real doc update ... progress is slow!


v51-00
======

Specialization of the self-extracting to frr open source.
extractor now named frr-toy.
Use of the cloonix root file-system to be also a container.
With monts inside the crun cloonix container, self_rootfs can
be used as a container in the self-extracting case.

As usual no time for real doc update ...




v50-00
======

The self-extracting user namespace container used to host cloonix in
the rootless version is now started with systemd, it is a customized
bebian trixie.
No time for doc for this version, work in progress...


v49-00
======

As usual, not enough time for descriptions, it will come eventualy:)
The main goal was to get the cvm directory belonging to a user 
(generally uid 1000) to keep this user for any new file.

It is the complexity of the user namespace, in a container, a user that
is not root must have a translated uid (generaly + 100000).

This is why is very hard to keep a root file-system with the same uid
for each file...


v48-00
======

No update of doc, work on cvm, not yet completed.
rootless self-extracting works again...

v47-00
======

Creation of a different brandtype of container, the only on was zip, now
there is the cvm brandtype thait is a root file system directory in bulk
that has the file "<bulk>/<rootfs_dir>/sbin/init" within the root fs.
The goal of this new container brand is to create a container that has
the equivalent of a desktop, launched with "Xephyr".
The code for this is fresh, the v48 will be safer...
For this delivery, the rootless self-extracting version has not been
updated, it will be back in v48-00.


v46-00
======

Mostly debug


v45-00
======

Some debug of the c2c link, based on tcp signaling between 2 cloonix
networks and udp to carry traffic.
Debug and tests before a communication to linuxfr.com.
The cisco part was not working anymore due to proxymous dev, it is repaired. 

The ultimate goal was to create a software not needing the root password,
for the moment it may not be a total success because the SELINUX of fedora41
prevents the qemu-guest-agent to work for cloonix, this work is a creation
of the seed script that launches the cloonix-agent necessary for cloonix_scp
and cloonix_ssh.
And for the ubuntu24 the error: "unshare: Operation not permitted" occured
and to avoid it /etc/sysctl.conf has to have the 2 following lines:
"kernel.unprivileged_userns_clone=1"
"kernel.apparmor_restrict_unprivileged_userns=0"



v44-00
======

Creation of a very important process: called "cloonix-proxymous-<net>".
This was a big step that took 2 months to finalize.
The goal of this process is to concentrate all the flows that have to run
in the main machine to transmit those flows to a crun-contenarised instance
that runs all of the cloonix meccanisms.
Icmp, udp, tcp, X11 flows come from the main namespace of the real running
machine and are transmitted by this process into the crun instance through
a shared space called "/tmp/<net>_proxymous_<user>" in the main machine and
called "cloonix_proxymous_<net>" in the crun instance isolated from the
main machine.
It was not an easy task and probably still not perfect, but the rootless
installation and rootless run must have this isolation process so as to
be root inside the crun and still be inside the main namespace to access
to all IP and graphic X11 display.
Note that for the sound, if you have a "$XDG_RUNTIME_DIR/pulse" directory,
it is shared by the crun process and the sound flow works through it.

For the software to be the same in the self-extracting rootless case and
the usual classical /usr/libexec/cloonix/ installation, the proxymous process
exists in both run, rootless or classical. In the rootless case it is
launched in the namespace before the crun that launches the rest of cloonix,
in the classical case, it is launched by the main cloonix server.

v43-00
======

Version done between the 24/12/2024 and 01/01/2025, sadly no time to finish
the goals: for the self extract version the nat component of cloonix
has not been implemented.
For this workload, the main objective has been on the "cloonix-proxymous-<net-name>"
process (for the extract_nemo.sh self-extracting executable found on
the clownix.net website, the process is named cloonix-proxymous-nemo.

Work done by this new proxy, bridge between host and crun isolated file-system:

 * X11 forwarding from /tmp/.X11-unix/X713 inside the crun to /tmp/.X11-unix/Xdisplay
 * Cloonix main access port (45211 for nemo) see the cloonix_port in cloonix.cfg
 * Cloonix web access port (54521 for nemo) see the novnc_port in cloonix.cfg
 * Not much tested, c2c udp connections between cloonix nets
 * And finaly not done the nat to host communication, will be done if I get time.

Sorry for lack of added doc but it is midnight and delivery is not finished!


v42-00
======

Note: you can now do Ctrl-C in a kvm console without breaking the qemu.

The self-extracting installation has now a proxy software running in the host
main namespace that transmits 2 tcp ports from host to within the running crun.
cloonix-proxymous is the name for this proxy, it is launched upon the
call to crun_container_startup.sh. 

This gives access from distant machines to the cloonix commands gui and also
to the web interface.

Note that for the nemo network given in the example in self_extracting_cloonix.sh,
in your browser you can hit http://<ip>:54521 and if you get the grey panel,
click on mouse and choose cloonix to get the usual gui but inside the web.
Note that the above works only if ports 45211 and 54521 (nemo ports) are free.
/var/log/syslog will show the error if the ports are used.

This self-extracting version does not need the root privileges nor any libraries.
The self-extracting does not support udp proxy and because of that, c2c cannot
work.

v41-00
======

One of the goals for this version was to get the cloonix binaries lighter while
keeping the same functions.
There is probably a lot of work to be done to take out useless binaries or
libraries to get cloonix slimmer, but as for a human, the first fat is the
easiest to lose:)

The self-extracting binary has only the zipfrr.zip in bulk to make it lighter.
 
Wireshark now can only save the .pcap file, to study the exchanges you have to
save the file and open it with a complete wireshark.



v40-00
======

Creation of the self-extracting cloonix to test as an user without any root privilege.
To test cloonix without ever having the root password for your host machine,
you have to use the self_extracting_cloonix.sh.

This script creates the self_extracting_rootfs_dir directory which countains
a root file-system equiped with the cloonix software, the openwrt qcow2
virtual machine, the zipbasic container, the frr container, demo scripts
for the cloonix run and a self-contained crun executable::

    wget http://clownix.net/downloads/cloonix-40/self_extracting_cloonix.sh
    ./self_extracting_cloonix.sh
    cd self_extracting_rootfs_dir
    ./crun_container_startup.sh
    ./ping_demo.sh




