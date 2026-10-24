==========================
The command line, one page
==========================

Everything cloonix does is asked with one command, *cloonix*. The canvas of
the browser sends the same requests; the demos are python programs that
call this command. This page is the whole command line, one figure and one
example per idea. The chapter *The cloonix client* has every option.

.. image:: /svg/cli_grammar.svg
   :width: 100%

A line has at most seven kinds of words, always in this order::

    cloonix cli nemo cnf cnt1 eth0 sniff=on
    |       |   |    |   |    |    |
    |       |   |    |   |    |    +-- a setting: key=value
    |       |   |    |   |    +------- the interface, when the object has several
    |       |   |    |   +------------ the object: a machine, a lan, an air, a cable
    |       |   |    +---------------- the verb
    |       |   +--------------------- the net
    |       +------------------------- speak to one net
    +--------------------------------- the command

Three rules hold on every line:

* a value is *key=value*: *ram=2000*, *publish=8080:80*, *x=300*;
* a yes is a *--flag*: *--persistent*, *--macvlan*, *--no_reboot*;
* an interface is *eth<i>*, *wlan<i>*, or *A* and *B* for the two sides of
  an a2b; an object of one interface (tap, nat, c2c, phy, an air) takes
  none. No lan and no air may be named like an interface, so the word
  after a name is read one way only.


A net
=====

::

    cloonix list                  every net of the config: up or not, its version
    cloonix start nemo            the server of nemo on this machine
    cloonix stop nemo             every object of nemo destroyed, the server stopped

A net is one server and the objects it holds. *cloonix list* asks every
net of the configuration at once and prints the version of this client
and of the cloonix installed here.


The verbs of a net
==================

.. image:: /svg/cli_map.svg
   :width: 100%

Six families of verbs, all after *cloonix cli <net>*:

======================  ================================================
build                   add, del, plug, unplug
configure               cnf
read                    dump
watch                   sniff, trace
inside the guests       ssh, scp, files, save
learn and debug         demo, rpc
======================  ================================================

A verb may be written short, three letters at least, where they fit one
verb only: *cloonix cli nemo unp cnt1 eth0* is an *unplug*.


Build
=====

::

    cloonix cli nemo add cnt cnt1 eth=2 trixie                 a container, two interfaces
    cloonix cli nemo add kvm vm1 ram=2G cpu=2 eth=1 trixie.qcow2   a virtual machine
    cloonix cli nemo add tap tap1                              an interface of the host
    cloonix cli nemo add nat nat1 publish=8080:80              the internet, a port back in
    cloonix cli nemo add a2b cable                             a cable with delay and loss
    cloonix cli nemo add phy enp6s0 --macvlan                  a real interface of the host
    cloonix cli nemo del cnt1                                  one object
    cloonix cli nemo del all                                   every object, the server stays

A lan is never added: it appears with its first cable and disappears with
its last one.

.. image:: /svg/cli_plug.svg
   :width: 100%

::

    cloonix cli nemo plug cnt1 eth0 lan1                       a cable from eth0 to lan1
    cloonix cli nemo plug cnt1 eth0 lan2                       moved: taken off lan1 first
    cloonix cli nemo plug cnt1 eth0 lan2                       already there: nothing to do
    cloonix cli nemo unplug cnt1 eth0                          off its lan, found by cloonix
    cloonix cli nemo plug tap1 lan1                            an object of one interface
    cloonix cli nemo plug cable A lan1                         side A of an a2b
    cloonix cli nemo plug cnt1 wlan0 air1                      a radio onto an air

*plug* of an interface cabled elsewhere moves it and says so on stderr; a
*plug* or an *unplug* of an interface still being cabled by an earlier
request waits for it and is then done. A
radio is a *wlan*: it goes on an *air*, the lan of the radios, and an eth
goes on a lan. The radios of the containers start on the air *cntwifi*,
those of the kvm on *kvmwifi*.


Configure
=========

*cnf* takes the name, the interface or the direction when there is one,
and settings. The key says what kind of object is meant: no two kinds share
a key.

::

    cloonix cli nemo cnf cnt1 eth0 sniff=on                    this interface can be captured
    cloonix cli nemo cnf cntwifi sniff=on                      this air can be captured
    cloonix cli nemo cnf sta1 wlan0 sniff=on                   the same, by a radio on it
    cloonix cli nemo cnf cnt1 x=300 y=200                      the place of cnt1 on the canvas
    cloonix cli nemo cnf cnt1 eth0 angle=90                    the place of eth0 around cnt1
    cloonix cli nemo cnf cnt1 note="eth0 1.1.1.1/24"           a comment under cnt1 on the canvas
    cloonix cli nemo cnf cable A2B delay=100                   100 ms from side A to side B
    cloonix cli nemo cnf cable B2A loss=500                    5% lost from B to A
    cloonix cli nemo cnf nat1 publish=2222:22                  host port 2222 to guest port 22
    cloonix cli nemo cnf cnt1 cpu=25 ram=100                   a quarter of a core, 100 MB
    cloonix cli nemo cnf cnt1 cpu=0                            the cpu cap lifted, the ram cap kept
    cloonix cli nemo cnf lan1 rstp=on                          the lan runs rapid spanning tree

======================  ================================================
key                     object
======================  ================================================
sniff=on|off            any interface, any air
x= y=                   any item of the canvas
angle=                  an interface of any item
note=                   any item of the canvas, the rest of the line
A2B B2A + delay= ...    an a2b
publish=                a nat
cpu= ram=               a container
rstp=on|off             a lan
mac_mangle=             a c2c
======================  ================================================


Read
====

::

    cloonix cli nemo dump commands          the command lines that rebuild the net
    cloonix cli nemo dump topo              every object on one line, then the lans
    cloonix cli nemo dump drawing           the net drawn in the terminal
    cloonix cli nemo dump json              the whole topology as json
    cloonix cli nemo dump ip nat1 vm1       the address the nat gave to vm1
    cloonix cli nemo dump images            what bulk holds, and whose it is
    cloonix cli nemo dump pid               the processes of the net

*dump commands* is a script: each line can be given to a shell, and the
whole list rebuilds the net as it is, settings and places included. A c2c
is made by the net that added it; on the far net its line is a comment::

    cloonix cli nemo dump commands > mynet.sh


Watch
=====

::

    cloonix cli nemo sniff cnt1 eth0 --line        one line per packet of eth0
    cloonix cli nemo sniff cntwifi --line          one line per frame of the air
    cloonix cli nemo sniff cnt1 eth0 icmp          only the icmp of eth0
    cloonix cli nemo trace qemu                    the command line of every kvm created

A *sniff* of an interface not yet capturable makes it capturable first and
says so on stderr. A *sniff* of a wlan is a sniff of its air: a radio has
no cable of its own, its frames are in the air.


Inside the guests
=================

::

    cloonix cli nemo ssh cnt1                      a shell in cnt1
    cloonix cli nemo ssh cnt1 "ip -br addr"        one command in cnt1
    cloonix cli nemo scp cnt1:/etc/hosts /tmp      a file out of cnt1
    cloonix cli nemo files cnt1                    two panes: this machine, cnt1
    cloonix cli nemo save vm1 /tmp/vm1.qcow2       the disk of vm1, kept

No address is needed in the guest: the path goes through cloonix, not
through the network of the guest.


Learn
=====

::

    cloonix cli nemo demo                          the list of the demos
    cloonix cli nemo demo cloonix/cnt.py           two containers, a ping

Every demo prints each command it gives, so a demo is also a lesson of this
command line.
