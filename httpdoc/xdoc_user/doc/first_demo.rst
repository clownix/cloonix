=================
The first network
=================

The chapter *First run* builds two machines and a ping in nine typed lines.
The same two networks are demos, for a first run with nothing to type after
the server is started.

.. image:: /svg/first_net.svg
   :align: center
   :width: 100%


Two containers
==============

::

    cloonix start nemo
    cloonix cli nemo demo cloonix/cnt.py

*cloonix start* stays in the foreground: the demo goes in a second terminal.
It needs the container image *trixie* in */var/lib/cloonix/bulk*; without
it, its first line says so and how to get it. It prints the nine lines of
*First run* as it runs them, two notes that write the addresses beside the
containers on the canvas, and the answer of the ping::

    $ cloonix cli nemo add cnt cnt1 eth=1 trixie
    $ cloonix cli nemo add cnt cnt2 eth=1 trixie
    $ cloonix cli nemo plug cnt1 eth0 lan1
    $ cloonix cli nemo plug cnt2 eth0 lan1
    $ cloonix cli nemo ssh cnt1 "ip addr add dev eth0 1.1.1.1/24"
    $ cloonix cli nemo ssh cnt1 "ip link set dev eth0 up"
    $ cloonix cli nemo ssh cnt2 "ip addr add dev eth0 1.1.1.2/24"
    $ cloonix cli nemo ssh cnt2 "ip link set dev eth0 up"
    $ cloonix cli nemo cnf cnt1 note="eth0 1.1.1.1/24"
    $ cloonix cli nemo cnf cnt2 note="eth0 1.1.1.2/24"
    $ cloonix cli nemo ssh cnt1 "ping -c 5 1.1.1.2"
    64 bytes from 1.1.1.2: icmp_seq=1 ttl=64 time=0.1 ms
    ...
    cnt1 pings cnt2. Erase everything, the server stays:
    $ cloonix cli nemo del all

Each line after a *$* can be typed as it is. Between the *add* and the
*ssh* lines the demo waits for the two containers to answer, as a hand-typed
run does by retrying the first *ssh*.


Two virtual machines
====================

::

    cloonix cli nemo demo cloonix/kvm.py

The same with *kvm1* and *kvm2* on the image *trixie.qcow2*, 1000 MB of ram
and two cpus each, then the same four *ip* lines, notes and ping::

    $ cloonix cli nemo add kvm kvm1 ram=1000 cpu=2 eth=1 trixie.qcow2
    $ cloonix cli nemo add kvm kvm2 ram=1000 cpu=2 eth=1 trixie.qcow2
    $ cloonix cli nemo plug kvm1 eth0 lan1
    $ cloonix cli nemo plug kvm2 eth0 lan1

A virtual machine boots a kernel: the wait before its first *ssh* is longer
than for a container.


From the canvas
===============

The Demo panel of the canvas lists both files under *cloonix*, and Play runs
them on the machine of the server: the commands appear in the log of the
panel as they run, the Comments panel tells the four steps, why each is
there and what the ping proved, and its *About* view holds the drawing of
the network. In the toy, this panel is the way to play them.

Both demos leave their machines in place: the canvas shows them, and a
double-click on one opens its shell. *cloonix cli nemo del all* clears the
net for the next one.

The two files are */usr/share/cloonix/demos/cloonix/cnt.py* and *kvm.py*,
short python files with the standard library only; the chapter *The demos of
cloonix* describes every demo of the tree.
