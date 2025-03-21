.. image:: /png/cloonix128.png 
   :align: right

============================
Software Release Information
============================


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

For the software to be the same in the self-extracting rootless
cloonix-extractor.sh case and the usual classical /usr/libexec/cloonix/
installation, the proxymous process exists in both run, rootless or classical.
In the rootless case it is launched in the namespace before the crun that
launches the rest of cloonix, in the classical case, it is launched by the
main cloonix server.

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


v39-00
======

Update and debug...

v38-00
======

Broadway web server has been erased, in its place Xvfb, wm2, x11vnc and websockify
are used to transmit the gui for cloonix to the web.
For nemo the http link for the browser is::

   http://127.0.0.1:54521/vnc_lite.html

Note that the 54521 port can be changed in::

  /usr/libexec/cloonix/common/etc/cloonix.cfg

Principle: Xvfb creates a virtual framebuffer on a defined DISPLAY, this display can be seen through its X11 socket at /tmp/.X11-unix/. Then on this display, wm2 is a windows manager, it is the simplest existing one. When the wm2 representation gets to be visible in your browser, click on the main mouse button, and choose the only choice which is cloonix.
x11vnx carries the equivalent of a desktop to the vnc server anw finaly websockify puts this desktop inside a web browser.


v37-00
======

1 May 2024, the compilation to get the cloonix bundle was only 64bits,
now there are 2 cloonix bundle, one for the 64 bits and one for the 32 bits.
Go to the end of **Instructions for building** to get the procedure
that creates the bundles.

Products for this version are:

Sources::

  cloonix-37-00.tar.gz

Associated open-sources from external providers necessary for the building 
of this version::

  targz_store.tar.gz

Binaries::

  cloonix-bundle-37-00-amd64.tar.gz
  cloonix-bundle-37-00-i386.tar.gz

Virtual machines for kvm in cloonix::

  fedora40.qcow2
  noble.qcow2
  bookworm.qcow2

Containers from distributions for crun in cloonix::

  fedora40.zip
  noble.zip
  bookworm.zip

Containers from personnal customisation for crun in cloonix::

  busybox.zip
  ovswitch.zip
  
Podman to test cloonix and frr::

  podman_cloonix.tar
  podman_frr_cloonix.tar


v36-01
======

Sunday 21 April, the broadway takes cpu and I do not use it, it is now
disabled by default, you have to do "cloonix_cli nemo cnf web on" to
have broadway server.


v36-00
======

Sunday 14th of April, end of my one week holidays, late, must deliver
cloonix as it is now. Very fresh version... Some problems to solve...

This version uses the broadwayd daemon to transmit the gtk3 gui
to your web browser.

The config file at **/usr/libexec/cloonix/common/etc/cloonix.cfg**
has a field added to give the broadway_port to put in your browser.

To test, launch the nemo: **cloonix_net nemo** and put in your browser:
**http://127.0.0.1:54521** because the broadway_port for nemo is 54521.

Big thanks to broadwayd, the daemon associated to gdb that permits
gtk3 to be accessible in a web browser!

v35-00
======

In this version, efforts have been targeted into the run of cloonix
in a podman container that is not run as root.
Cloonix containers can be used to create test networks in the podman
with no admin rights. For the KVM machines, you need to give rights
to the user to use /dev/kvm /dev/vhost-net and /dev/net/tun.

The startup of the zip containers launched by crun has changed:
/usr/bin/cloonix_startup_script.sh is launched at container startup
if this file exists.

Cloonix does not run podman containers anymore, it is easy to produce a
crun zip from a podman, it was too complex to mix different brands.


v34-00
======

In this version, the zip containers can be made by service instead
of by distro. See at quickstart/cnt_create the way to create some
of the service-oriented zip containers.
For the startup of the containers, I use the --startup_env="NODE_ID=x" to
differentiate nodes coming from the same container, inside the container
I usualy add a monit service that uses the NODE_ID env variable to
configure the node.


v33-00
======

For this version ZIP files for the crun replace EXT4 files for the
file-systems.

The phy ethernet interface of the host can be used in cloonix
without total disapearance from the host, thanks to the macvlan
type.

The client gui is locked in a non-root envirronment, if you need
the spice usb absorption in the vm desktop, you must use spice
through cloonix_ice and not with a right click on the cloonix_gui.

Docker is not supported anymore, it was redondant with podman
which I personaly prefer..


v30-00 and v30-01
=================

Note that there is no v29, the v30 has a very big change in its delivery
method. Earlier version were given source only with the constraint of
compilation by the user.
The compilation was a drawback to use cloonix because the target host had
to be a developer distribution.
This version has binary delivery, the binaries included are all self-contained,
these binaries do not open any file comming from the host, all is included
within the bundle. This makes the binary compatible with any distribution.

Also, the locations of the binaries and qcow data have changed, it is now
*/usr/libexec/cloonix* and */var/lib/cloonix* and of course, the handle
scripts at */usr/bin/cloonix_xxx*.

If you have an old version of cloonix, then you must erase it with:
*rm -rf /usr/local/bin/cloonix*. If you do not, the old version will
probably take precedence on the new version because the path has the
/usr/local/bin before /usr/bin. 

v28-00
======

The **phy** item has been added, this permits to act on the real physical 
interface of the host.
When you add a phy interface to the canvas, this interface disapears from
the list of interfaces of the host and appears in the namespace of the
cloonix network.

For example on my host I have an unsused physical interface named enp6s0,
if I use the phy item to use this interface within the cloonix net nemo with
the following command::

    cloonix_cli nemo add phy enp6s0

Then the enp6s0 interface disapears from the host ifconfig list, but it is not
lost for every namespace, it can be visualised in the cloonix_nemo
namespace through the following command::

    ip netns exec cloonix_nemo ip address

Then you can link this new interface to a lan of cloonix and through this
lan to amy other item of cloonix.

Beware that cloonix does not check that the interface is not used, if it is
then you will lose the connectivity from this interface.




v27-02
======

Docker and Podman have integrated into their images ENTRYPOINT and CMD
which are predefined startup commands that must be taken into account
when cloonix starts the container.

In order to achieve this goal, cloonix requests the ENTRYPOINT and CMD
through the commands::

    docker/podman inspect -f '{{.Config.Entrypoint}}' <image_id>
    docker/podman inspect -f '{{.Config.Cmd}}' <image_id>

And if lines returned show an empty startup command, then cloonix puts::

    sleep 7777d

as new entrypoint command to have a remanent container even if no input command
is setup.

The version v27-02 also adds the possibility to give some environment variables
at container startup, this is done with the option::

    --startup_env="<env_name=env_val env2_name=env2_val...>"

Tor the help in docker/podman container creation, do ::

  cloonix_cli nemo add doc
