======================
The switching binaries
======================

Everything that touches a packet lives in the namespace *cloonix_<net>*.

.. image:: /svg/ovs_satellites.svg
   :align: center
   :width: 100%


The openvswitch four
====================

*cloonsrc/ovs*: a *cmd*, a *patched_create* and *ovs.patch* around the
vendored openvswitch; the patch lets several instances run side by side
under cloonix's names, and *cmd* skips the probe of conntrack timeout
policies, whose dummy policy the kernel logs at every start::

    cloonix-ovsdb-server  cloonix-ovsdb-tool    the configuration database
    cloonix-ovs-vswitchd                        the switch
    cloonix-ovs-vsctl                           the client, unchanged but the name

**cloonix-ovs-drv** is cloonix's own wrapper, one per net: creates the
namespace, starts the database and the switch inside it, and runs every
change of the switch the main server asks for, as text on a unix socket:
lans, ports, mirrors, taps, phy.


*ovs_popen.c* runs each ovs-vsctl and ip command of the drv in a forked
worker and reads back what it printed from *ovs_cmd_out_<pid>.txt* of the
ovs directory, one file per process: the drv itself runs the ip commands
that move interfaces into the namespace, its netns child every ovs-vsctl,
and the two run at the same time. A snf mirror is taken out of the bridge
of its lan first, out of the other bridges only when a row is left.


The satellites
==============

All started by suid-power, none setuid::

    cloonix-ovs-drv    one per net: the switch itself, above; netns.c, ovs_cmd.c, action.c
    cloonix-ovs-snf    one per sniffed interface: pcap_record.c writes, tun_tap.c reads the
                       mirror, inotify_trigger.c wakes webshark without polling
    cloonix-ovs-a2b    the impairment: circle.c the delay ring, replace.c loss, sched.c
    cloonix-ovs-c2c    a lan carried over udp: udp.c, rxtx.c, packet_arp_mangle.c
    cloonix-ovs-nat    the nat item: machine.c per guest, passt_shuttle.c to the host side

The four other than drv share *ctrl_link.c* of *lib_io_clownix*: the
listening unix socket the main server connects to, the one live connection
(a second one replaces the first, the listener is kept), the watchdog and
the stop. The main server sends a *pid_req* about every second (a2b, nat
and snf once per second, c2c on every pass of its heartbeat, 0.5 or 1.5 s);
the first
must arrive within 5 s, then one per 20 s, or the satellite stops itself
with one warning line, both deadlines measured on the monotonic clock with
the credit of *stall_detect.h* for the time the satellite itself was not
running. The 20 s are the same silence proxymous gives the switch: a main
server late by a few seconds is a host held by something else, a broadcast
storm in a guest for one, and a satellite stopping on it cuts a link that
was sound; a c2c cut that way is rebuilt by its master, minutes later. Every stop, watchdog, closed connection or *kil_req*, is
the same *ctrl_link_stop*: the paths the satellite registered are unlinked,
its tap children killed, exit 0. A path found at start means the previous
holder was killed rather than stopped; it is erased with a warning. The
nat keeps a second watchdog of the same period on its proxymous leg. The
tap child of a2b, c2c and nat shares *tap_child_from_parent* and
*tap_child_to_parent* of *intf_flags.c*; only the tap write policy differs
and stays in each *tun_tap.c*. The a2b and nat parents read the pipe a
batch at a time, at most *SEQTAP_PIPE_BATCH* (32) frames per event while
*fct_seqtap_pipe_more* finds the next header there: a turn of the loop
costs more than a frame, and one frame per turn let a tcp burst from a
guest overflow the 1 MB pipe of the nat. The c2c parent reads one frame
per turn: its next hop, the unix datagram queue of proxymous, holds ten,
and a batch overflowed it.

**cloonix-passt** is not cloonix's code, *cloonsrc/passt* a *cmd* around
the vendored passt; it runs on the host, which is why the shuttle exists.
The nat item and the nat code inside proxymous are two things: the first
serves the guests, the second cloonix's own traffic.
