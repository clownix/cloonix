==========================
Safety and attack surface
==========================

What an attacker can reach in a running cloonix, from where, and what stands
in the way. Everything here is checkable in the source named beside it.

.. image:: /svg/security.svg
   :align: center
   :width: 100%


The map
=======

Every way into a running net, in one table. *the user* is the account that
started the net; in the toy every root of this table is that account.

.. list-table::
   :header-rows: 1
   :widths: 18 18 30 34

   * - entry
     - who reaches it
     - what stands in the way
     - what passing it gives
   * - doors port 45211
     - anyone the address lets in
     - the net password, an hmac on every frame, tls around
     - the whole client: guests, a root shell in each, files in and out
   * - web port 4444
     - anyone, every address
     - the same password, https, Origin equal to Host
     - the canvas: the same, plus playing a demo, a program run as the user
       on the host
   * - c2c udp
     - the peer net
     - the port pair of the config
     - ethernet frames into one lan
   * - nat
     - the guest
     - egress only, passt on the host as the user
     - a way out to the internet; in only with publish=
   * - a guest, kvm
     - whoever owns it
     - qemu, running as the user once its devices are open
     - the user's account if qemu is broken
   * - a guest, cnt
     - whoever owns it
     - a user namespace on the user's subuid window
     - a subuid of the user
   * - the agent channel
     - the guest
     - parsed by the main server, as the user
     - the user's account if the parser is broken
   * - cloonix-suid-power
     - the user, or root, only
     - SO_PEERCRED, a socket in a 0700 directory, a check on each verb
     - qemu, crun, taps, a phy: what cloonix is for, bounded to files the
       user can open
   * - /var/lib/cloonix/<net>
     - the user only
     - 0700
     - the launch scripts root runs
   * - /tmp/cloonix_proxymous_<net>
     - the user only
     - 0700, owner checked at start
     - the sockets between the daemons
   * - a local account that is not the user
     - nothing of the net unless it holds the password
     - the password is what keeps it out; the group cloonix keeps it from
       starting a net of its own
     - with the password, the two ports as above; without it, nothing

The rest of this chapter takes the lines of this table one by one; the
section *Hardening after the install* is what to do about the last line
and about the password.


Who is where
============

A running net is a set of processes on one machine. The account each one
runs under is the whole of what a flaw in it can give away:

==============================  ===================  ============================
process                         bundle               toy
==============================  ===================  ============================
cloonix-main-server             the user             uid 0 of the crun = the user
cloonix-doorways                the user             same
cloonix-proxymous               the user             the user, on the host
cloonix-gui-server              the user             uid 0 of the crun
cloonix-ovs-drv, ovsdb-server,  root                 uid 0 of the crun
ovs-vswitchd
cloonix-ovs-snf, -a2b, -c2c,    root until the tap   uid 0 of the crun
-nat                            is open, the user
                                for good after
cloonix-suid-power              root                 uid 0 of the crun
cloonix-qemu-system             root at start, the   uid 0 of the crun
                                user once running
cnt guest                       the user's subuid    uid 0 of the crun
                                window
==============================  ===================  ============================

*the user* is the account that typed *cloonix start <net>*. In the toy every
uid 0 above is that account, mapped to 0 in a user namespace; the other ids
of that namespace, when the toy has them, are the subuid window of the
account: there is no root anywhere in a toy, whatever is broken.

To see it::

    ps -eo user,pid,comm | grep cloonix


Who may run cloonix
===================

In the bundle, as installed, **every account of the machine** may start a
net: *cloonix-suid-power*, the one setuid binary, lets in anyone. The two
copies of the configuration, */usr/libexec/cloonix/cloonfs/etc/cloonix.cfg*
and */usr/bin/cloonix_config.py*, hold every net password and are readable
by everyone, and so is the key of the https certificate.

One switch changes that: the existence of the **group cloonix**. Once it
exists, *cloonix-suid-power* lets in root and its members, nobody else. The
install does not create it; the section *Hardening after the install* does.

What a member is trusted with is what cloonix is for: qemu started as root
for its tap and its namespace, a physical interface taken into a lan, a
container built by crun. The section *One privilege boundary* lists what
the root side accepts from such a member and what it refuses.

Source: *suid_power/src/guard.c*, *guard_group_allows()*;
*build_tools/install_cloonfs*.


Two ports, nothing else
=======================

A net listens on two tcp ports and no other, both bound by proxymous with
the same certificate::

    cloonix_port      45211   tls, the client, cloonix-cxp-cli and the demos
    cloonix_web        4444   https, name and password, on every address

No guest exposes a port. A shell into a guest, a file transfer and the
topology feed are legs multiplexed on 45211; a desktop and a capture viewer
are websockets on the web port. A firewall in front of cloonix opens one port
for the command line and one for the browser.

To see it::

    ss -ltnp | grep cloonix

**The password is the perimeter.** Whoever holds *cloonix_passwd* of a net
drives it: adds and deletes guests, opens a root shell in any of them, plays
a demo, which is a program run on the host in the account of the net owner.
The passwords shipped in *cloonix_config.py* are in every copy of the
tarball; *cloonix start* says so at every start of a net that still uses one,
and *install_cloonfs* repeats it. Change them before a port is reachable by
anyone but yourself.


The doors port: tls around, an hmac on every frame
==================================================

**Who is talking.** Every doors frame carries a 32 byte **hmac-sha256** of
its payload, keyed by *cloonix_passwd*. *cloonix-doorways* checks it on
every frame, in both directions, in constant time, and closes the
connection on a mismatch. Without the password nothing a client says is
acted on.

**Who is listening.** The tcp connection is wrapped in **tls**, ended in
*cloonix-proxymous* with the certificate of the https port, so the frames
cross the network encrypted. The certificate is self-signed and the client
does not verify it: tls is there for the confidentiality only, the hmac
above is what authenticates.

Two dialects on one port
------------------------

proxymous reads the first byte of every new connection and serves both::

    0x16    a tls client hello: the handshake is done in proxymous, and
            doorways receives the decrypted frames
    0xCA    a doors frame in clear: handed to doorways as it is

The *cloonix* client and *cloonix-cxp-cli* both try tls first. A server that
closes the socket during the handshake, a release before this one or one
without a certificate, is spoken to again in clear, and both print one line
when they do::

    cloonix: 192.168.1.9:45211 does not speak tls, the connection is not encrypted

With *CLOONIX_REQUIRE_TLS=1* in the environment they refuse instead of
falling back, so a server that can be made to fail the hello from outside
cannot obtain the frames in the clear. The clear form is still what the c2c
leg between two nets sends.

*cloonix-cxp-cli* takes the password from the environment variable
*CLOONIX_PASSWD* when its second argument is *-*, which is how the gui server
and the toy start it: the password never appears in the process list.

Source: *proxymous_passt/src/set_config.c*, *proxy_tls.c*,
*lib_io_clownix/src/doorways_sock.c*, *cxp_cli/src/cloonix_cxp_cli.c*,
*cloonix/cloonix* function *tls_connect*.


The web port
============

  * https only. The port asks for http basic credentials: the net name as
    user, its *cloonix_passwd* as password, compared in constant time. A
    wrong password is answered after one second, one connection per guess.
    Basic is the one scheme a browser applies by itself to a websocket
    handshake, so the console panels carry the credentials without a token
    in any url.
  * Every request has its **Origin** compared with its *Host*. A page open
    in the browser on another site cannot drive a cloonix on this machine.
  * The certificate is self-signed, made once at packing time and shipped
    in the tarball; a net that finds none makes its own at its first
    start. Its key is as public as the tarball: replace it on anything but
    a private lab machine::

        sudo cp my_cert.pem /usr/share/cloonix/cert/cloonix_cert.pem
        sudo cp my_key.pem  /usr/share/cloonix/cert/cloonix_key.pem

    or point *CLOONIX_TLS_DIR* at a directory holding the pair. The
    installed key is readable by everyone until the post-install step
    below limits it to the group.

Source: *jscloonix/jscloonix/web/web_auth.c*, *web_http.c*.


What a browser cannot do
========================

  * **Run bytes it sent.** A demo is played by name; the gui server hands
    that name to proxymous, which runs the file from */usr/share/cloonix/demos*
    on the host. A name that is not there, or a net without proxymous, gets
    a *KO* and nothing runs. The text sent with the name is only looked at
    for its shebang.
  * **Write a file.** The pcap recorder writes under */usr/share/cloonix/pcap*
    and nowhere else, and every name a browser sends that becomes part of a
    path is checked by *web_name_ok*: no separator, no *..*, no leading dot.
  * **Get a shell on the host.** The console urls name a guest and one of
    three kinds; anything else is refused before any fork.
  * **Replace a shipped demo.** */usr/share/cloonix/demos* is sticky
    (1777) and the shipped tree belongs to root, writable by root only: a
    user saves demos under names of their own and cannot rewrite
    *frrspider.py* for the next user who presses Play.

What a browser can do, with the password, is play a demo of its choice
among those on disk, which is a program run as the net owner, and open a
root shell in every guest. That is the intended use of the port.


One privilege boundary
======================

One binary of the delivery is setuid root: **cloonix-suid-power**. It is
started by the main server, reachable through a unix socket in
*/var/lib/cloonix/<net>*, mode 0600 and owned by the net owner, and every
connection is checked with *SO_PEERCRED*: the net owner or root, nobody
else. It speaks a fixed vocabulary, and each verb is bounded on the root
side, whatever the caller writes:

==========================  =================================================
request                     what root does, and the check on it
==========================  =================================================
cloonsuid_req_launch        starts *dtach -n <sock> bash <script>*: the
                            dtach and the bash are the pinned ones of
                            cloonfs, the socket and the script are inside
                            */var/lib/cloonix/<net>*, the script belongs to
                            the caller and holds the two lines the main
                            server writes, on the pinned nsenter and
                            qemu; every file named on the qemu line is one
                            the caller can open with their own uid
cloonsuid_req_vm_kill       SIGKILL of a qemu started here
cloonsuid_req_pid_kill      SIGKILL of a process of the caller's uid, or of
                            a process of this net; any other pid is refused
cloonsuid_req_phy           the list of physical interfaces
cloonsuid_req_ovs           *ovs-vsctl* read commands from a fixed list,
                            no option token, no shell character
cloonsuid_req_spawn         one of the five ovs satellites, by its full
                            path in the installed bin dir, arguments of
                            identifier characters only
cloonsuid_req_suidroot      takes root back for the verbs above
cloonsuid_crun_create_net   a container in a user namespace; one without,
                            which would be host root, is refused outside
                            the toy
erase_workdir (argv)        removes */var/lib/cloonix/<net>* when the
                            caller owns it
==========================  =================================================

The bin dir and the work dir are not taken from the command line: the
first is the installed one, the second is */var/lib/cloonix/<net>*, and a
start with anything else stops. There is no "run this command" among the
verbs. Every name that becomes a path or an argument is checked by
*name_is_sane()*: letters, digits, underscore, dash, dot, no leading dash or
dot, no *..*.

qemu is started as root because a tap in a namespace needs it, and is
given *-run-with user=<uid>:<gid>* so that once its devices are open it is a
process of the net owner. A guest escaping qemu lands in that account, not
in root.

The ovs satellites, *cloonix-ovs-snf*, *-a2b*, *-c2c*, *-nat*, carry no
setuid bit: suid-power starts them as root, they enter the namespace and
open their tap, then drop to the net owner with *cloonix_drop_root()*,
real, effective and saved ids together, before they read a byte from the
network. A c2c peer or a nat flow talks to a process of the user.
*cloonix-ovs-drv* and the two openvswitch daemons it runs stay root: the
bridges are kernel objects and the datapath needs it.

To see it::

    find /usr/libexec/cloonix -perm -4000
    ps -o user,pid,args -C cloonix-qemu-system

Source: *suid_power/src/suid_power.c*, *guard.c*, *crun.c*.


Directories
===========

::

    /var/lib/cloonix           1777   any user creates a net dir, only the
                                      owner removes or renames it
    /var/lib/cloonix/bulk      777    images shared and rebuilt by anyone;
                                      1777 after the post-install step; a
                                      <n>_targz tree extracted from <n>.tar.gz
                                      by the main server, as the user; mounted
                                      read only for every container
    /var/lib/cloonix/<net>     0700   the launch scripts and config.json that
                                      root runs are inside: the owner only
    /tmp/cloonix_proxymous_<net>  0700  the sockets between the daemons of a
                                      net; owned by the user, or the net
                                      does not start
    /usr/share/cloonix/demos   1777   shipped demos root, 644
    /usr/share/cloonix/pcap    1777
    /usr/share/cloonix/cert    644, group only after the post-install step
    cloonfs/etc/cloonix.cfg    644, group only after the post-install step
    /usr/bin/cloonix_config.py 644, same

A name that is known in advance under */tmp* is a name another account
can create first; the main server and proxymous both refuse a directory
that exists and belongs to someone else.

Source: *build_tools/install_cloonfs*, *suid_power.c* function
*run_dir_create*, *uml_clownix_switch.c* function *mk_and_tst_work_path*,
*proxymous_passt/src/main.c*.


Namespaces
==========

  * The lans, the guest interfaces and the ovs daemons live in the network
    namespace *cloonix_<net>*. The host sees no interface of a running net,
    and two nets on one machine cannot collide.
  * A **cnt** runs in a user namespace: container uid 0 is the user running
    cloonix, container 1 upward is that user's subuid range, which
    *cloonix-suid-power* adds to */etc/subuid* and */etc/subgid* when the
    user has none. The image is mounted idmapped for each container --
    its owner seen as that user, the owner's window as his -- so bulk is
    never modified by cloonix and one image serves several users at once.
    A net started by root gets no container rather than one without a
    user namespace.
  * There is no privileged container in the bundle: no flag asks for one,
    and *cloonix-suid-power* refuses the request outside the toy. In the
    toy every container is one, and its uid 0 is the user.
  * The toy runs the whole of cloonix inside a crun of the user's own uid:
    root inside it is the user on the disk.

To see the namespaces of a container::

    cloonix cli nemo trace nspace


Guests
======

  * A guest image contains no cloonix: no agent, no library, no binary. The
    agent comes on a cd-rom built by cloonix, *insider_agent_*.iso*.
  * A shell into a guest uses no ip address of the guest: the bytes travel
    on a virtio serial port for a kvm, on a shared mount for a cnt.
  * The directory transfer of *cloonix cli <net> scp -r* refuses member names that
    are absolute or contain *..*, on both sides of the wire.
  * The nat is egress only unless a port is published with *publish=*.
  * What a guest sends back on its agent channel is parsed by the main
    server, a process of the net owner: a hostile guest talks to that
    account, not to root.


No host library
===============

Every binary of *cloonfs* has its interpreter and rpath rewritten to
*cloonfs* with patchelf. A vulnerable library of the host is never loaded by
cloonix, and a vulnerable library of cloonix is never loaded by the host.

To see it::

    readelf -l /usr/libexec/cloonix/cloonfs/bin/cloonix-main-server | grep interpreter
    readelf -d /usr/libexec/cloonix/cloonfs/bin/cloonix-main-server | grep RPATH


Hardening after the install
===========================

The install leaves cloonix open: every account starts a net, the passwords
and the key are readable, a client falls back to clear. Each of the seven
steps below closes one of those, and each is optional: a private machine
with one user needs none of them.

One: change the passwords
-------------------------

The *cloonix_passwd* of every net is in *cloonix_config.py* of the tarball,
so it is public: the name of the net followed by *clown*. For the net
*nemo*: user *nemo*, password *nemoclown*. Edit */usr/bin/cloonix_config.py* and
*/usr/libexec/cloonix/cloonfs/etc/cloonix.cfg*, the same text twice, one
line per net::

    cloonix_passwd   nemoclown

Every client of the net, on this machine or another, uses the same file,
so the edited file is what to hand out. *cloonix start* says at every start
whether a net still runs with a shipped password.

Two: the group cloonix
----------------------

::

    sudo groupadd cloonix
    sudo usermod -aG cloonix bob

From the *groupadd* on, *cloonix-suid-power* refuses every account that is
not root or a member. The check reads */etc/group* at each start, so bob
starts a net the second the *usermod* is done, in any terminal, including
the one he is sitting in. Adding and removing others::

    sudo usermod -aG cloonix alice
    sudo gpasswd -d alice cloonix

Deleting the group turns the gate off again::

    sudo groupdel cloonix

Three: limit the passwords and the key to the group
---------------------------------------------------

::

    sudo chown root:cloonix /usr/bin/cloonix_config.py \
                            /usr/libexec/cloonix/cloonfs/etc/cloonix.cfg \
                            /usr/share/cloonix/cert/cloonix_key.pem
    sudo chmod 640          /usr/bin/cloonix_config.py \
                            /usr/libexec/cloonix/cloonfs/etc/cloonix.cfg \
                            /usr/share/cloonix/cert/cloonix_key.pem

From then on a local account outside the group sees neither the passwords
nor the key.

A process gets its groups once, at login: a shell opened before the
*usermod* does not carry *cloonix*, and no command it runs can add it. The
daemons and the client read these files with the groups of the shell, so
that shell stops on *permission denied*; *cloonix-suid-power* reads
*/etc/group* itself, which is why step two needs no new terminal. *id*
shows whether a shell carries the group; one that does not gets it without
a new login::

    newgrp cloonix                      a child shell carrying the group
    sg cloonix -c "cloonix start nemo"  one command carrying the group

The client says so when it finds *cloonix_config.py* unreadable::

    cloonix_config.py is there but not readable by this shell: it is limited
    to the group cloonix, and a session opened before you were added to
    that group does not have it. Open a new terminal, or run:
      newgrp cloonix

A reinstall rewrites the three files at 644, so the *chown* and *chmod*
above are redone after one; the group and its members are kept, */etc/group*
is not touched by *rm -rf /usr/libexec/cloonix*.

Four: only the owner removes an image
-------------------------------------

::

    sudo chmod 1777 /var/lib/cloonix/bulk

Sticky, like */tmp*: an image is still readable and usable by every net,
but only the account that made it can remove or rename it. Rebuilding an
image made by another account then needs that account, or a *chown* of
the image.

Five: refuse the clear fallback
-------------------------------

In the environment of the shell that runs the client, and of the gui
server when its consoles reach a remote net::

    export CLOONIX_REQUIRE_TLS=1

A server that does not answer the tls hello is then refused rather than
spoken to in clear.

Six: your own certificate
-------------------------

The shipped key is as public as the tarball. Replace the pair, then limit
the key as in step three::

    sudo cp my_cert.pem /usr/share/cloonix/cert/cloonix_cert.pem
    sudo cp my_key.pem  /usr/share/cloonix/cert/cloonix_key.pem

A browser that had accepted the previous certificate asks again once.

Seven: the ports
----------------

*cloonix_ip* in the config is the address the doors port binds; 127.0.0.1
keeps the command line on this machine. The web port listens on every
address: a firewall in front of it opens 4444 to the addresses that need
the canvas and nothing else.


What is not protected
=====================

  * Until the group exists, every local account starts a net; until step
    three above is done, every local account reads the net passwords and
    the key of the certificate.
  * A net owner is trusted with the host interfaces: a phy item puts any
    physical interface of the host into a lan.
  * The c2c leg between two nets and its udp travel in clear.
  * A client speaking plain frames on the doors port is still served by the
    server; it is the client that refuses to speak plain to a remote net.
  * The key of the shipped certificate is public until replaced.
  * The password is one string per net, in a file; there is no account, no
    audit of who used it.
  * qemu opens its devices and disks as root before dropping; a flaw in
    that first second is a flaw in root.
  * *cloonix-ovs-drv*, *ovsdb-server* and *ovs-vswitchd* are root for the
    life of the net; their sockets are in the net directory, mode 0700.
