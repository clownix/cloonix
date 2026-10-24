=====================
Cloonix configuration
=====================

One machine runs several nets at once, each with a name, an ip and ports.
The server and the client share one text, *cloonix/cloonix_config.py* in the
source tree, installed as one file::

    /usr/libexec/cloonix/cloonfs/etc/cloonix.cfg       read as text by the daemons
    /usr/bin/cloonix_config.py                         a link to it, imported by the client
    <toy>/rootfs/cloonix/cloonfs/etc/cloonix.cfg       the toy, linked as <toy>/bin/cloonix_config.py

Editing an installed cloonix is editing that file. A client copied elsewhere
keeps its own *cloonix_config.py*; *cloonix list* names the nets on which
that copy and the installed file disagree.

.. image:: /svg/net_ports.svg
   :align: center
   :width: 100%

::

    CLOONIX_VERSION=__LAST__

    CLOONIX_NET: nemo {
      cloonix_ip       127.0.0.1
      cloonix_port     45211
      cloonix_passwd   nemoclown
      cloonix_web      4444
      c2c_udp_ip       127.0.0.1
      c2c_udp_port_low 51713
    }

::

    cloonix_ip         the address the clients use to reach the server
    cloonix_port       doorways: the cli, the ssh, the scp and the topology events
    cloonix_passwd     the hmac key of every doors frame, and the web password
    cloonix_web        the canvas over https, on every address; optional, headless without it
    c2c_udp_ip         the address a distant cloonix sends its c2c udp to
    c2c_udp_port_low   the bottom of a range of a hundred udp ports for the c2c of this net

The shipped passwords are not secret: the user of the web page is the name
of the net, the password the name of the net followed by *clown*. For the
net *nemo*: user *nemo*, password *nemoclown*. The section *Hardening after the install* of the safety
chapter changes them.

The keywords of a block are parsed by name. The delivered file declares
nemo, mito and fido on the loopback and cucu, lolo and endu on real
addresses. Two nets on one machine differ by *cloonix_port*, *cloonix_web*
and *c2c_udp_port_low*; the web ports go by ten::

    nemo 45211 4444 51713
    mito 45212 4454 52713
    fido 45213 4464 53713
    cucu 45217 4474 57713
    lolo 45218 4484 58713
    endu 45219 4494 59713

A c2c takes the first free port at or above *c2c_udp_port_low* and tells it
to the far side over tcp; a net serves five c2c relays at a time. A firewall
between two machines lets the whole range through.

To add a net, copy a block, change every number, install::

    CLOONIX_NET: <n> {
      cloonix_ip       <ip>
      cloonix_port     <port>
      cloonix_passwd   <password>
      cloonix_web      <port>
      c2c_udp_ip       <ip>
      c2c_udp_port_low <port>
    }
