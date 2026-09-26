=================
The first network
=================

The chapter *First run* builds two machines and a ping by typing nine
lines. The same two networks exist as demos, for a first run without a
single line to type after the server is started.

.. image:: /svg/first_net.svg
   :align: center
   :width: 100%


Two containers
==============

::

    cloonix start nemo
    cloonix cli nemo demo cloonix/cnt.py

*cloonix start* stays in the foreground: the demo goes in a second
terminal. The demo needs the container image *trixie* in
*/var/lib/cloonix/bulk*;
without it, its first line says so and how to build it. What it prints is
the nine lines of the chapter *First run*, run one after the other, and the
answer of the ping::

    $ cloonix cli nemo add cnt cnt1 eth=1 trixie
    $ cloonix cli nemo add cnt cnt2 eth=1 trixie
    $ cloonix cli nemo plug cnt1 eth0 lan1
    $ cloonix cli nemo plug cnt2 eth0 lan1
    $ cloonix cli nemo ssh cnt1 "ip addr add dev eth0 1.1.1.1/24"
    $ cloonix cli nemo ssh cnt1 "ip link set dev eth0 up"
    $ cloonix cli nemo ssh cnt2 "ip addr add dev eth0 1.1.1.2/24"
    $ cloonix cli nemo ssh cnt2 "ip link set dev eth0 up"
    $ cloonix cli nemo ssh cnt1 "ping -c 5 1.1.1.2"
    64 bytes from 1.1.1.2: icmp_seq=1 ttl=64 time=0.1 ms
    ...
    cnt1 pings cnt2. Erase everything, the server stays:
    $ cloonix cli nemo del all

Each line after a *$* can be typed as it is. Between the *add* lines and
the *ssh* lines the demo waits for the two containers to answer, which is
what a hand-typed run does by retrying the first *ssh*.


Two virtual machines
====================

::

    cloonix cli nemo demo cloonix/kvm.py

Same shape, with *kvm1* and *kvm2* on image *trixie.qcow2*, 1000 MB of ram
and two cpus each::

    $ cloonix cli nemo add kvm kvm1 ram=1000 cpu=2 eth=1 trixie.qcow2
    $ cloonix cli nemo add kvm kvm2 ram=1000 cpu=2 eth=1 trixie.qcow2
    $ cloonix cli nemo plug kvm1 eth0 lan1
    $ cloonix cli nemo plug kvm2 eth0 lan1

then the same four *ip* lines and the same ping. A virtual machine boots a
kernel, so the wait before the first *ssh* is longer than for a container.


From the canvas
===============

The Demo panel of the canvas lists the same two files under *cloonix*, and
Play runs them on the machine of the server. The commands appear in the
log of the panel as they run; beside it the History popup tells the four
steps, why each is there and what the ping proved, and its *About* view
holds the drawing of the network. In the toy::

    ./nemocmd demo cloonix/cnt.py
    ./nemocmd demo cloonix/kvm.py

Both demos leave their machines in place, so the canvas shows them and a
double-click on one opens its shell. *cloonix cli nemo del all* clears the
net for the next one.


Where they are
==============

::

    /usr/share/cloonix/demos/cloonix/cnt.py
    /usr/share/cloonix/demos/cloonix/kvm.py

Each is a short python file using the standard library only; the chapter
*Demos* describes what every demo of the tree does.
