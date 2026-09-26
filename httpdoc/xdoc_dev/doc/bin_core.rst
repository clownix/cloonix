=================
The core binaries
=================

The processes that exist for every net whether or not it has a guest.


cloonix-main-server
===================

Source: *cloonsrc/uml_cloonix_switch*, a *Makefile*, about sixty files. The
only process holding the topology; no two helpers talk to each other
directly. At start, in the order of *uml_clownix_switch.c*: read
*cloonix.cfg* for the net named on the command line, check the work
directory, refuse if another instance holds the lock, create the proxyshare
directory, fork the helpers.

::

    recv_from_client.c    every command a client can send, in one switch
    cfg_store.c           the topology, the only place it is stored
    machine_create.c      adding a kvm or a cnt, end to end
    suid_power.c          the client half of the privileged channel
    ovs.c ovs_snf.c ...   one file per helper it manages: ovs_a2b, ovs_c2c, ovs_nat_main, ovs_tap
    automates.c           the state machines that survive a helper dying
    qcow2_hdr.c           the qcow2 overlay of a kvm, header only, no qemu-img
    pcap_filter.c         the capture filter grammar
    list_commands.c       dump commands, the lines that replay the net
    targz.c               a cnt image given as bulk/<n>.tar.gz: extracted once into
                          bulk/<n>_targz by a clone, the add parked until it is there;
                          the tree keeps the modes of the archive, its protection is
                          idmap_mount_ro of suid_power, the read only lower of the overlay

Every helper is started the same way, *ovs.c* at *connect_ovs_try* being the
model: write the socket path, fork the helper with it in argv, retry
connecting on a timer until it answers or a count runs out.


cloonix-doorways
================

Source: *cloonsrc/doorways*, a *Makefile*, about 2200 lines. Binds no tcp
port: it listens on *proxy_pmain.sock*, where proxymous delivers what
arrived on *cloonix_port*, checks the hmac, and relays between that client
and the unix socket of the main server, so the process holding the topology
never listens on tcp. *dispach.c* routes the leg types; *llid_traffic.c* holds the
data path and *check_wire_version*.


cloonix-cxp-cli
===============

Source: *cloonsrc/cxp_cli*, one file of about 650 lines::

    cloonix-cxp-cli <ip:port | unix_socket> <passwd> <vm> [command...]

A shell in a guest or one command in it, over a *doors_type_dbssh* leg, the
C end of the cxp protocol of *cxp_wire.h*; no key exchange, no host key,
nothing of ssh but the *ssh* verb of the client. An *ip:port*
target is wrapped in tls, unverified, and spoken to in clear with a
warning when it does not answer the hello, unless *CLOONIX_REQUIRE_TLS=1*
is set. A *<passwd>* of *-* is read
from *CLOONIX_PASSWD*, which is how *web_pty.c* of the gui server runs it
under *forkpty*, and how the *sh* script that is *cloonix* inside the toy's
container calls it; there is no C command line, the vocabulary lives in
*cloonix/cloonix* alone, and a new verb is one entry of its *to_json()*
plus the message in *json_interface.c*.


cloonix-proxymous
=================

Source: *cloonsrc/proxymous_passt*, a *Makefile*, about 6400 lines. The one
process that stays on the host in the toy, hence its four jobs:

  * **The web door.** *jsweb_relay.c*, *proxy_tls.c*: accepts https on
    every address, terminates TLS, splices the plaintext to
    *proxy_pjsweb.sock*. The same *proxy_tls.c* ends the tls of the doors
    port in *set_config.c*, where the first byte of a connection tells a
    client hello from a plain frame. OpenSSL never sees a descriptor: the
    bytes go through a pair of memory BIOs, and after every operation the
    write BIO is emptied towards the peer.

    .. image:: /svg/tls_bios.svg
       :align: center
       :width: 100%

  * **passt.** *passt_spawn.c* starts one *cloonix-passt* per nat, on the
    host, with the *-t* and *-u* of the published ports. The binary is taken
    beside proxymous, *pthexec_self_bin_dir()*: *cloonfs/bin* in the bundle,
    *bin/* of the toy, which carries it for that.
  * **Demos.** *demo_exec.c* runs the text the gui server read from its own
    disk.
  * **The udp relay of the c2c.** *udp_proxy.c*: one relay per c2c keyed by
    its name, five at most (*MAX_UDP_PROXY*), binding the first free port at
    or above *c2c_udp_port_low* over a hundred and sending it back to the
    switch; the unix sockets carry the port, *dgram_c2c_<port>_tx.sock* and
    *_rx.sock*. A sixth c2c is refused naming the five.
