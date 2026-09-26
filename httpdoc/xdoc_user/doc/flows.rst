=======================================
The flows, from the client into a guest
=======================================

Every client of a net, the command line, a shell, a file transfer, the web
canvas, reaches it through one tcp port. This chapter follows what travels
on it.


One port for everything
=======================

.. image:: /svg/net_ports.svg
   :align: center
   :width: 100%

A net declares one port in *cloonix.cfg*, 45211 for nemo.
**cloonix-proxymous** listens on it, ends the tls, and passes each
connection over the unix socket *proxy_pmain.sock* to **cloonix-doorways**,
which checks the hmac. Every client opens its own connection;
each conversation on it is a *leg*, and the frames carry a *type* and a
*val* that say what it is for, so a shell, a file transfer and the topology
feed run at the same time.

A frame is a 48 byte header and a payload::

    CA FE                  start magic
    tid len type val nb    2+4+2+2+2 bytes
    hmac-sha256            32 bytes, keyed by cloonix_passwd padded to 32
    DE CA                  end magic

The hmac is checked on every frame in both directions; a mismatch closes
the connection. The tcp connection carrying the frames is tls, ended in
proxymous (safety chapter). Two types matter to a user: *doors_type_switch* carries the
json rpc of the command line and of the canvas, *doors_type_dbssh* carries
the bytes of a shell or a transfer into one named guest.


A cli command
=============

The client turns the words of the line into one json message, the same one
the C daemons exchange, prefixed by an eight byte magic that tells an rpc
apart from raw shell bytes on the same port::

    cloonix cli nemo plug cnt1 eth0 lan1
    {"fctident":"add_lan_endp","tid":0,"name":"cnt1","num":0,"lan":"lan1"}

*cloonix cli nemo trace json_cli* prints every such message as the server
receives and sends it.


A shell in a guest
==================

.. image:: /svg/ssh_path.svg
   :align: center
   :width: 100%

The guest is named, not addressed. The main server looks the name up in its
topology and opens a byte pipe to it:

  * a **kvm**: a unix socket given to qemu as a chardev, seen inside as the
    virtio port */dev/virtio-ports/net.cloon.0*
  * a **cnt**: a unix socket bind-mounted into the container

**cloonix-agent** reads that pipe and hands the bytes to
**cloonix-cxp-srv**, which runs the command or opens a pty. Nothing in the
path uses an ip address of the guest: take every interface down, flush the
routes, firewall the machine, the shell still answers.

One pipe per guest, whatever is happening in it: ten shells in one kvm are
ten legs down one virtio port.


The exec protocol
=================

Once the pipe is open, the two ends speak a protocol of their own, defined
in *glob_include/cxp_wire.h*::

    EXEC       a command, stdout and stderr apart, the real exit status
    PTY        an interactive shell on a pty, window size followed
    PUT GET    one file in or out
    PUTTREE GETTREE   a directory, member by member
    LIST       one directory and none of its contents (files)

One operation per leg: the first frame says which, the server serves it and
closes. The frames are authenticated by the hmac of the doors port and
encrypted by its tls; the tunnel is private to the net. Member names
of a tree transfer that are absolute or contain *..* are refused on both
sides.


Testing the path
================

::

    cloonix cli nemo trace probe cnt1

crosses the five layers one at a time, tcp, hmac, leg, agent, far end, and
says how far it got. The clients chapter has the output.
