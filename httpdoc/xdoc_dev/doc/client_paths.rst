===========
The ways in
===========

Three transports, ending in the same place: **cloonix-main-server** is the
only process that changes the topology. Everything is multiplexed: one tcp
port carries the commands and every conversation with every guest, one
byte pipe per guest carries every session into it.

.. image:: /svg/client_paths.svg
   :align: center
   :width: 100%


The cloonix client
==================

Every verb speaks the doors protocol over *cloonix_port*. The tcp socket is
bound by **cloonix-proxymous**, in the bundle as in the toy: doorways binds
no tcp port at all, *doorways_sock_server()* in *lib_io_clownix* listens on
*proxy_pmain.sock* and its port argument is not used. A frame is a 48 byte
header and a payload; two header fields do the multiplexing::

    type        doors_type_switch   a command for the switch, served first
                doors_type_dbssh    a leg into a guest
                                    (vnc and openssh keep their numbers and
                                    are answered link_ko; nothing sends them)
    dido_llid   which conversation the frame belongs to

Twenty shells, a file copy and a *dump json* at once are twenty-two llids
on one socket. Each frame carries an hmac-sha256 keyed with
*cloonix_passwd*; **cloonix-doorways** checks it and closes without a word
on a mismatch, which the client reports as a possible wrong password.
*cloonix-proxymous* stands in front on the public address, ends the tls,
and relays the bytes to doorways over *proxy_pmain.sock*, framing untouched
and unread: one tcp connection is one unix connection, both ways.

The leg request carries the wire version of the client, *wire=<n>*.
*check_wire_version* in *doorways/src/llid_traffic.c* logs a difference,
with the guest the leg goes into, once per distinct version and lets every leg through; from 76 the numbering
only grows, so a difference is a hint for a broken interface, never a gate.

A demo is the same client, running where it was typed.


The web port
============

**https** ends at proxymous, which hands the plaintext to
*proxy_pjsweb.sock*. The gui server asks http basic credentials on it, the
net name and *cloonix_passwd*. Every request
has its *Origin* compared with its *Host*; no *Origin* means no browser and
is let through. What the browser asks becomes rpc to the main server over
*proxy_pmain.sock*.


Into a guest
============

The client opens a *doors_type_dbssh* leg naming a guest and a cookie::

    DBSSH_CLI_DOORS_REQ name=<vm> cookie=0000cxp wire=<n>

doorways looks the name up and joins the leg to the agent socket of that
guest. The cookie is kept in the format; its value no longer selects
anything, the ssh daemon that once shared the pipe being gone. The bytes
enter the guest by a unix socket given to qemu as a chardev, seen as
*/dev/virtio-ports/net.cloon.0*, or by a socket bind-mounted into a
container. **cloonix-agent** passes them to
*/cloonixmnt/tmp/unix_cloonix_cxp_sock*, where **cloonix-cxp-srv** runs the
command, opens a pty, or serves a transfer. The opcodes are in
*glob_include/cxp_wire.h*, compiled by the linux, windows and freebsd
servers alike::

    EXEC PTY PUT GET PUTTREE GETTREE LIST

One operation per connection; an unknown opcode is answered *unexpected
first opcode N*.

The two consoles of the canvas take one route: **cloonix-dtach** attaches
to a session on the host, the qemu stdio console wrapped in dtach at launch
for a kvm, and a session that **cloonix-suid-power** forks on demand running
*crun exec* for a cnt. Only the fork is privileged; the socket is chowned to
the user and left 0600.


What runs on the server, and as whom
====================================

A demo played from the canvas: the gui server resolves the name under
*/usr/share/cloonix/demos*, reads the file from its own disk, hands the text
to proxymous, which runs it on the host as the user of the net. Nothing a
browser sent is executed.

**cloonix-suid-power** is the only root process of a net: installed setuid,
drops to the caller's uid at boot, takes root per request, speaks a fixed
vocabulary. There is no "run this command" among its verbs.

Everything a browser sends that becomes part of a path goes through
*web_name_ok*: a name, not a path. */files/save*, which once wrote a
browser-supplied body into the share, answers nothing any more; the *lsh*
local shell is gone; no route accepts a programme.
