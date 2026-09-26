=====================================
Other guests: freebsd, windows, cisco
=====================================

freebsd.py
==========

.. image:: /svg/demo_freebsd.svg
   :align: center
   :width: 100%

BSD is the Berkeley Software Distribution, begun by Bill Joy at the University
of California in 1977 as additions to AT&T's Unix. 4.2BSD of 1983, by Joy, Sam
Leffler and Mike Karels, carried the first TCP/IP stack in wide use and the
socket API every operating system still copies; ifconfig, netstat and route
are its words. FreeBSD 1.0 was released in November 1993 by Jordan Hubbard,
Nate Williams and Rod Grimes, from the 386BSD of Bill and Lynne Jolitz, and
the project continues under the FreeBSD Foundation with Marshall Kirk McKusick
as its historian.

Today: FreeBSD serves the video of Netflix, runs inside the PlayStation,
Juniper routers and NetApp filers, and is the base of pfSense and OPNsense
firewalls.

One freebsd kvm and two containers on one lan, *freebsd.qcow2* built by
*vm_create/cnt_kvm/kvm_freebsd.sh*::

    cloonix cli nemo demo freebsd.py
    cloonix cli nemo ssh bsd1 "ifconfig vtnet0 10.0.0.1/24 up"
    cloonix cli nemo ssh bsd1 "netstat -rn"

The interface is *vtnet0*, named after its driver. The agent comes on
*insider_agent_bsd_x86_64.iso* and the guest turns blue like a linux one.


windows.py
==========

.. image:: /svg/demo_windows.svg
   :align: center
   :width: 100%

One windows kvm and two containers on one lan, *win.qcow2* prepared as the
windows chapter describes::

    cloonix cli nemo demo windows.py
    cloonix cli nemo demo windows.py --no-fw

The two containers ping first, so the lan is known good. Then four commands
from the host::

    cloonix cli nemo ssh Win1 "getmac /v /fo csv /nh"
    cloonix cli nemo ssh Win1 'netsh interface set interface name="Ethernet" admin=enable'
    cloonix cli nemo ssh Win1 'netsh interface ip set address name="Ethernet" static 10.0.0.1 255.255.255.0'
    cloonix cli nemo ssh Win1 'netsh advfirewall firewall add rule name="cloonix icmp in" protocol=icmpv4:8,any dir=in action=allow'

Without the last rule windows pings out and answers nothing in. The capture
says which::

    cloonix cli nemo cnf Win1 eth0 sniff=on
    cloonix cli nemo sniff Win1 eth0 --dump icmp


cisco.py
========

.. image:: /svg/demo_cisco.svg
   :align: center
   :width: 100%

Three ios-xe routers in a triangle, a container on two corners, *c8000v.qcow2*
built from a Catalyst 8000V iso with *vm_create/cisco/doitall.sh*, and the
*trixie* image::

    cd vm_create/cisco && ./doitall.sh --all /path/to/c8000v-universalk9.iso
    cloonix cli nemo demo cisco.py

A cisco has no agent. The demo drives the serial console through the dtach
socket of the guest, */var/lib/cloonix/<net>/dtach/<vm>*, raw pty, carriage
return, one line at a time waiting for the prompt. The routers stay red on
the canvas. The interfaces are *GigabitEthernet1*, *2*, *3*, in order, up
in *shutdown*::

    ./cisco.py nemo --conf cisco1 my.cfg
    ./cisco.py nemo --show cisco1 "show ip route"
    cloonix cli nemo cnf cisco1 eth1 sniff=on
    cloonix cli nemo sniff cisco1 eth1 --dump "ip and not udp and not tcp"
