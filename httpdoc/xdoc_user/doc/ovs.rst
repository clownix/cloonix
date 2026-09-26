=============================
Details on ovs ports creation
=============================

The guests are connected by an openvswitch instance in the namespace
*cloonix_<net>*. An interface of a guest is vhost based, born not
mirrored; *sniff=on* mirrors it.


Naming
======

Every endpoint receives an *obj_id* at creation, and every net a *rank*, its
place in *cloonix_config.py* (1 for the first net, nemo). A port is::

    _k_<rank>_<obj_id>_<num_eth>     an interface of a guest; 0 for a tap, a nat, a c2c
    _b_<lan>                         the bridge of a lan
    s_k_<rank>_<obj_id>_<num_eth>    the mirror port of a capturable interface

The rank keeps apart the ports of two nets of one host.

.. image:: /svg/ovs_ports.svg
   :align: center
   :width: 100%

One bridge per lan; each interface of a guest is one port of it, and an
interface made capturable has a second port, its mirror, read by one
*cloonix-ovs-snf* each.


Looking at it
=============

::

    cloonix cli nemo add kvm Cloon1 ram=2000 cpu=2 eth=3 trixie.qcow2
    cloonix cli nemo add kvm Cloon2 ram=2000 cpu=2 eth=3 trixie.qcow2
    cloonix cli nemo plug Cloon1 eth0 lan01
    cloonix cli nemo plug Cloon2 eth0 lan01
    cloonix cli nemo cnf Cloon1 eth0 sniff=on
    cloonix cli nemo cnf Cloon2 eth0 sniff=on

::

    ~$ ip netns list
    cloonix_nemo

    ~$ cloonix cli nemo dump ovs-vsctl show
    099c0660-6111-4b0f-b3fc-6dae7138bf19
        Bridge _b_lan01
            Port s_k_1_2_0
                Interface s_k_1_2_0
            Port _b_lan01
                Interface _b_lan01
                    type: internal
            Port _k_1_2_0
                Interface _k_1_2_0
            Port s_k_1_1_0
                Interface s_k_1_1_0
            Port _k_1_1_0
                Interface _k_1_1_0

*_k_1_1_0* is eth0 of Cloon1 on nemo, *s_k_1_1_0* its mirror. The interfaces from the
host, by the name of the namespace::

    sudo ip netns exec cloonix_nemo ip link
    sudo ip netns exec cloonix_nemo ip addr

or by the pid of a process inside it::

    pgrep -f "cloonix-ovsdb-server.*nemo"
    sudo nsenter --net=/proc/<pid>/ns/net ip link

The ovs database is reached by its socket in the work directory; *dump
ovs-vsctl* fills it in, a *cloonix-ovs-vsctl* typed by hand does not.

In the toy the ovs binaries are inside the container, which *./nemocmd
shell* enters.


When a capture is empty
=======================

::

    cloonix cli nemo dump ovs-vsctl list Mirror        no row: the snf port receives floods only
