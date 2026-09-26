==========================
The event loop and the rpc
==========================

Every binary is built on two libraries.

.. image:: /svg/event_loop.svg
   :align: center
   :width: 100%


lib_io_clownix
==============

Set up, register callbacks, call the loop, never return.

  * **A callback runs to completion.** No preemption, no locking. A callback
    that blocks stops the process: a *system()*, a *read* on a pipe with
    nobody writing, a long loop. Slow work is forked, *pid_clone.c* in the
    main server. The gui server is the one binary with threads beside
    its loop; they hand every request for the main server to the loop
    through a queue, the gui server chapter has it.
  * **An llid is not a file descriptor.** It indexes the channel table and
    is not recycled the moment it closes, so a late callback carrying a
    dead llid is detected.
  * **KOUT kills the process, KERR does not.** *KOUT* for a state that
    cannot be true; *KERR* for something wrong the process survives.
  * **A beat is 10 ms, and a beat is not a clock.** The timers count
    beats of the loop; a process that was not scheduled misses beats
    without noticing. Anything that judges a peer by its silence measures
    the silence on *CLOCK_MONOTONIC* (*stall_detect.h*) and credits back
    the stretches during which it was not running itself, up to a cap.
    The line that reports such a stretch says what the process did in it:
    cpu near the gap is a loop that turned too slowly, forced switches a
    starved process, voluntary switches or major faults one blocked in
    the kernel. The loop itself says, at most once per ten seconds, where
    a late turn lost its time (*late loop*: *epoll_wait* back late, or one
    turn of callbacks and its cpu). A stall line ends with what the
    machine did meanwhile, from */proc/stat*: steal, softirq, iowait, idle.
  * **KERR never blocks.** *syslog(3)* waits while the log daemon does not
    read */dev/log*; *KERR* sends on a non-blocking socket of its own, a
    full socket drops the line, and the first line that passes says how
    many were lost and for how long. *KOUT* waits one second for its last
    word.

.. image:: /svg/stall_credit.svg
   :align: center
   :width: 100%

::

    msg_mngt_loop()                     never returns
    clownix_timeout_add(beats, cb, ...) a one-shot timer, one beat is 10 ms
    string_server_unix(path, cb, name)  listen, one line per message
    string_client_unix(path, err, rx)   connect to one
    proxy_traf_tcp_server(port, cb)     listen on tcp, every address
    msg_delete_channel(llid)            close and forget


lib_rpc_clownix
===============

.. image:: /svg/rpc_frame.svg
   :align: center
   :width: 100%

A message is one line of json with a *fctident*::

    {"fctident":"add_vm","tid":42,"name":"k1","vm_id":3, ... }

*json_interface.c* holds a *send_xxx* and its parser for each; nothing is
generated. The reader the parsers share -- a key lookup on one flat object,
*json_get_int*, *json_get_str*, *json_escape*, the *fctident* -- is
*glob_include/json_min.c*, one copy for *json_interface.c*, *json_doors.c*
and *rpct.c* alike. *tid* is an echo the answer carries back unchanged. A list is
emitted inline, never in a second message.

On the doors port a header goes in front of the line: 14 bytes of magic and
fields, a 32 byte hmac-sha256 keyed by *cloonix_passwd* padded to 32, two
bytes of end magic. A wrong digest closes the connection. See
*doorways_sock.c* of lib_io_clownix and *frame()* of the python client. The hmac
authenticates; the tls proxymous ends on the port encrypts.

*rpc_clownix.h* carries the enum of every kind of peer, used to route a
*hop* message without a table of sockets per feature::

    type_hop_ovs, type_hop_ovsdb, type_hop_endp, type_hop_doors,
    type_hop_suid_power, type_hop_gui_server, type_hop_nat_main,
    type_hop_proxymous, type_hop_c2c, type_hop_a2b, type_hop_snf


The wire version
================

*glob_include/glob_wire.h*::

    CLOONIX_WIRE_VERSION       what this build speaks, sent in the leg request
    CLOONIX_WIRE_VERSION_MIN   76, where the numbering was fixed

Bump *CLOONIX_WIRE_VERSION* when values are added; never move or remove a
value; move *MIN* only when the promise is broken on purpose, and say so in
the release notes. The python client carries the same number and the same
enums, checked by *debug/check_wire.sh*. The check at run time is a log
line, not a refusal.
