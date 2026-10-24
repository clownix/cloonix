===============
A windows guest
===============

A windows machine is a **kvm**: a qcow2 in bulk, a circle on the canvas,
ports cabled to a lan. Three things a microsoft disk needs that a linux
image does not, ide rootfs, the virtio driver disc and OVMF, are set by one
word::

    cloonix cli nemo add win Win1 ram=4000 cpu=2 eth=1 win.qcow2

*dump commands* prints it back as *add win ...
added_cdrom=virtio-win.iso*. The preparation below is done once per qcow2.


Getting a disk
==============

Microsoft publishes evaluation images that need no installation; the
Windows Server evaluation VHD/VHDX runs 180 days and asks for an
administrator password at first boot:

   * https://www.microsoft.com/en-us/evalcenter/download-windows-server-2025

Convert it with the *qemu-img* of the distribution, package *qemu-utils* on
debian, since cloonix ships none::

    cd /var/lib/cloonix/bulk
    qemu-img convert -p -O qcow2 <downloaded>.vhdx win.qcow2

Windows 11 asks for TPM 2.0 and secure boot; the server images boot on a
plain q35. *ram=4000 cpu=2* is the working minimum; the converted disk is
about 10 GB and grows.


GPT and OVMF
============

A microsoft image is GPT with an EFI loader; seabios finds a zero boot
area and stays black after *Booting from Hard Disk*. Two sectors say which
kind of disk you have::

    qemu-img dd if=win.qcow2 of=/tmp/s01.bin bs=512 count=2
    dd if=/tmp/s01.bin bs=512 skip=1 count=1 2>/dev/null | head -c 8 | od -c
    od -A d -t x1 -j 450 -N 1 /tmp/s01.bin

*E F I   P A R T* on the first and *ee* on the second mean GPT, for *add
win*, which always boots OVMF; an MBR disk does not boot that way. The
firmware is in the qemu data directory of cloonfs, *bin/qemu*: the code,
*edk2-x86_64-code.fd*, read only, and the variables template
*edk2-i386-vars.fd* when the qemu tree it was built from has one. qemu trees
since 2025 have no x86 template and build OVMF for stateless use: qemu gets
the code alone, nothing is kept between boots, and the boot manager starts
*EFI/BOOT/BOOTX64.EFI* of the disk, present on every microsoft image. With a
template, each machine gets a writable copy of it as a second pflash unit,
and the boot entry the firmware writes survives a reboot.


The driver disc
===============

::

    cd /var/lib/cloonix/bulk
    wget https://fedorapeople.org/groups/virt/virtio-win/direct-downloads/stable-virtio/virtio-win.iso

*add win* attaches *virtio-win.iso* by name from bulk; another name goes in
*added_cdrom=*. One installer on it, *virtio-win-guest-tools.exe*, carries
the four things needed::

    NetKVM      the nic
    vioscsi     the cloonix agent disc becomes visible
    vioserial   the port the agent talks on
    qemu-ga     the guest agent cloonix drives to start its own


The C-A-D button
================

A windows machine shows *Press Ctrl+Alt+Del to sign in*, a combination the
browser or the host desktop takes before a browser panel. **C-A-D**, the one
button at the top right of the desktop panel, shown while the vnc session is
connected, sends it and hands the keyboard back to the canvas: sign-in,
lock, Task Manager.


Preparing the image, once
=========================

Keep a copy; *--persistent* writes into the bulk file with no overlay::

    cd /var/lib/cloonix/bulk
    cp win.qcow2 win.qcow2.pristine
    cloonix cli nemo add win Win1 ram=4000 cpu=2 eth=1 --persistent win.qcow2

Give the run the interfaces the image is meant to have: windows writes one
registry entry per interface and keeps it (see below). Then:

  1. Right-click Win1, *Desktop*. The TianoCore logo, then windows.
  2. Click **C-A-D**, type the administrator password.
  3. File Explorer, *This PC*, the CD drive *virtio-win-0.1.xxx*, run
     **virtio-win-guest-tools.exe**, *Yes* to the elevation, *Next*, the
     licence, *Install*, *Install* on each *Windows Security* box (tick
     *Always trust software from Red Hat, Inc.* on the first), *Finish*.
  4. Restart from inside windows: Start, Power, *Restart*.
  5. The circle turns blue when the agent answers: cloonix wrote
     *C:\agent_cloonix_launcher.bat* through qemu-ga and it started
     *cloonix-agent.exe* from the insider disc.
  6. Shut windows down cleanly, then delete the machine::

        cloonix cli nemo del Win1

*win.qcow2* holds the drivers; with a firmware variables template in the
delivery, *win.qcow2.vars.fd* beside it holds the boot entry the firmware
wrote, and the two are copied or renamed together. Without one there is no
second file.


Everyday use
============

.. image:: /svg/windows_files.svg
   :align: center
   :width: 100%

::

    cloonix cli nemo add win Win1 ram=4000 cpu=2 eth=1 win.qcow2
    cloonix cli nemo add win Win2 ram=4000 cpu=2 eth=1 win.qcow2

Each machine gets its own overlay, and its own firmware variables when the
delivery has a template. If OVMF ever lands in its shell::

    fs0:
    \EFI\Microsoft\Boot\bootmgfw.efi


The interfaces windows remembers
================================

.. image:: /svg/windows_eth_slots.svg
   :align: center
   :width: 100%

Windows names an adapter after its pci slot and keeps the name. Cloonix
pins one slot per interface::

    eth0 -> 00:05.0    eth1 -> 00:06.0    eth2 -> 00:07.0

An interface that was there once stays as a hidden device holding its name
and its configuration; an image prepared with three and run with one
carries *Ethernet 2* and *Ethernet 3* for ever and names a new one *Ethernet
4*. Prepare with the interfaces the image is meant to have.

To clear an image, start it with no interface and *--persistent*::

    cloonix cli nemo add win Win1 ram=4000 cpu=2 --persistent win.qcow2
    cloonix cli nemo ssh Win1 "pnputil /enum-devices /disconnected /class Net"
    cloonix cli nemo ssh Win1 "pnputil /remove-device PCI\\VEN_1AF4&DEV_1041&...&0&30"

then shut down and delete. The last byte of the mac is the cloonix
interface number, the rest derived from the net name and the machine name::

    cloonix cli nemo ssh Win1 "powershell -NoProfile -Command Get-NetAdapter | ft Name,MacAddress"

    Name        MacAddress
    Ethernet    02-3F-A1-90-2C-00     <- eth0
    Ethernet 2  02-3F-A1-90-2C-01     <- eth1


Checking the four drivers
=========================

::

    NetKVM      Device Manager, Network adapters, Red Hat VirtIO Ethernet Adapter
    vioscsi     File Explorer, a second CD drive beside virtio-win: the insider disc
    vioserial   Device Manager, System devices, VirtIO Serial Driver
    qemu-ga     Services, QEMU Guest Agent, running


ssh and scp into windows
========================

::

    cloonix cli nemo ssh Win1
    cloonix cli nemo ssh Win1 "ipconfig /all"
    cloonix cli nemo ssh Win1 "dir C:\\Users"
    cloonix cli nemo scp report.txt Win1:C:/Users/Public/
    cloonix cli nemo scp Win1:C:/Windows/win.ini .
    cloonix cli nemo scp -r Win1:C:/logs ./here

The shell is cmd.exe; a command is run through *cmd.exe /c*. Quote the whole
command as one argument. Paths take either slash. A file pulled out lands
as 0644. The clipboard, both ways, with what windows ships::

    echo "some text" | cloonix cli nemo ssh Win1 clip
    cloonix cli nemo ssh Win1 "powershell -NoProfile -Command Get-Clipboard"
    cloonix cli nemo ssh Win1 clip < notes.txt


The agent and the sessions
==========================

*cloonix-agent.exe* answers the ping and relays the byte pipes to
*cloonix-cxp-srv.exe*
listening on 127.0.0.1:4441 inside the guest. Both are started by the
launcher through qemu-ga, a service, so they begin in session 0::

    -nopty    cmd.exe on pipes, line at a time, nothing full screen; served in session 0
    -pty      a pseudoconsole; served once the cxp server has moved into the session
              of whoever is logged in, which it does by itself

The move is printed in *C:\cloonix-cxp-srv.log*::

    cloonix windows cxp server in session 0, terminals on pipes
    watching for a logged-in session, to move there and serve a real terminal
    relaunched in session 1 as pid 4242, this copy leaves
    cloonix windows cxp server in session 1, terminals on a pseudoconsole

Logging out kills that copy; the agent notices the closed port and starts a
fresh one, *no cxp server on 127.0.0.1:4441, starting one*. The clipboard
reached is the one of the session the server is in. Either mode by hand::

    taskkill /f /im cloonix-cxp-srv.exe
    <letter>:\cloonix-cxp-srv.exe -pty


When it does not work
=====================

::

    the sign-in screen ignores the keyboard   the C-A-D button
    black screen after Booting from Hard Disk  seabios on a GPT disk: added with add kvm,
                                               not add win
    the log names OVMF                        no edk2-x86_64-code.fd in cloonfs/bin/qemu: a
                                              delivery built before the firmware was added
    no nic in windows                          the installer has not run, or no reboot since
    the adapter is Ethernet 4, and there is one   hidden devices; clear them as above
    the circle stays red                       run <letter>:\cloonix-agent.exe by hand from the
                                               insider disc; read %TEMP%\cloonix-agent.log
    no second CD drive at all                  vioscsi missing
    blue, but ssh refused with NO_CXP_SERVER   run <letter>:\cloonix-cxp-srv.exe by hand;
                                               read C:\cloonix-cxp-srv.log
    the terminal opens and stays blank         session 0 with -pty; the log names the session
