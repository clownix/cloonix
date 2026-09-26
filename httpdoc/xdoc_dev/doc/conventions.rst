========================
Conventions and pitfalls
========================


Adding a message
================

.. image:: /svg/add_message.svg
   :align: center
   :width: 100%

Four places, in this order:

1. *lib_rpc_clownix/src/json_interface.c*: the *send_xxx*, its parser, and
   a *RECV_DEFAULT* line at the bottom
2. *lib_rpc_clownix/include/rpc_clownix.h*: the declaration, a *type_hop*
   value if a new kind of peer
3. the sender
4. the receiver, in the binary that answers it

A receiver is a weak default in the library -- it warns and drops -- until
a binary defines it; no binary carries a stub for what it does not answer.
The same holds for *doors_recv_xxx* in *json_doors.c* and *rpct_recv_xxx*
in *lib_io_clownix/src/rpct.c*. A receiver mis-spelled in a binary is not
a link error any more: *debug/deadcode.sh* lists it as dead.

Two halves that disagree fail at run time with a parse error naming the
fctident. A value added to *glob_wire.h* bumps *CLOONIX_WIRE_VERSION* and
the copy in the python client; *debug/check_wire.sh* compares them.


Adding a helper, a setuid operation, a client verb
==================================================

A helper the main server starts: copy *ovs.c*, socket path in argv, retry
connecting on a timer, a callback for it dying in *automates.c*.

A setuid operation: a verb of suid-power, on the socket that exists. Not a
second binary.

A client verb: one entry in *to_json()* of *cloonix/cloonix*, its
completion in *cloonix_completion.py*, the message in *json_interface.c*.

The hidden options of the client, all suppressed from its usage:
*--wait*, *-s*, *--cfg*, *--raw*, *--completion*, and three for the
machinery: *--via-unix <path>* connects straight to a cloonix-cxp-srv unix
socket with no cloonix in between, *--complete* and *--complete-usage* are
what the tab hook calls.


Answering a client
==================

A client is answered *status_ok* when what it asked for is, *status_ko*
when it cannot be, and never on acceptance: an ok sent when the request was
taken reaches the client before the refusal that follows and hides it, and
the demos then wait for a machine that never existed. Two shapes exist:

  * the answer arrives on a message: the record holds *cli_llid* and
    *cli_tid*, the handler of the reply calls *utils_send_status_ok* or
    *utils_send_status_ko*, and a heartbeat counter on the record turns a
    reply that never comes into a ko -- *crun.c* for a cnt on
    *crun_start_resp_ok*, *utils_finish_vm_init* for a kvm once qemu has a
    pid, *ovs_a2b.c*, *ovs_nat_main.c*, *ovs_tap.c*, *ovs_phy.c* on the pid
    of their satellite
  * no message says it: *ack_pending()* of *ack_pending.c* keeps the
    client and polls a test every period until it answers 1, -1 with a
    reason, or the limit is reached, which is a ko *TIMEOUT <verb>*; a
    client gone meanwhile is dropped. *cnf ... sniff=on* waits this way for the snf
    process to be up or gone (*ovs_snf_state*), *add c2c* for the c2c
    process to have a pid (*ovs_c2c_up_test*).

The settings the switch holds itself, *x= y=*, *angle=*, *mac_mangle=*,
answer at once, the value being set.


Things that have bitten
=======================

  * A helper defined between two lines of another function compiles as a
    nested function under gcc and fails elsewhere: check the brace depth.
  * *glob_include/pthexec.c* holds every absolute path of the delivery once,
    relative to a prefix that *pthexec_running_in_crun()* chooses at init,
    */cloonix* in the toy and */usr/libexec/cloonix* in the bundle: a new
    path is one *X* line of *PTHEXEC_LIST* and its prototype in *pthexec.h*.
  * A directory of executables *copy_libs* does not scan: silent at build,
    fatal at run.
  * A qcow2 only grows; *qemu-img convert -c* compacts one.
  * A user namespace refuses what capabilities suggest: *setgroups* denied,
    *chown* to an unmapped id EINVAL, a write refused with the full mask.
  * A text built with a path goes through *snprintf* with the size of its
    destination: a *sprintf* of a 299 byte name into an *info* or *err* of
    *MAX_PATH_LEN* overran the stack in *recv_from_client.c* and
    *system_callers.c*.
  * A port open to the network receives anything. *fct_seqtap_rx_udp*
    returns *kind_seqtap_error* with *buf* NULL on any mismatch of magic,
    kind or length, and the udp side of a c2c drops that datagram with one
    warning per period; the stream callers stop, a stream out of step being
    unrecoverable. *debug/sub_c2c_udp_garbage.py* holds the case.
  * A unix socket name is bounded by *sun_path*, 108 bytes, not by
    *MAX_PATH_LEN*: *unix_sock()* of *util_sock.c* refuses a longer one.
  * A fact tested from the wrong namespace is not the fact:
    *if_nametoindex* on a guest tap from the main server sees nothing,
    because the taps live in *cloonix_<net>*; */proc/<vswitchd_pid>/net/dev*
    lists them.


Names that no longer describe the thing
=======================================

  * **the nat item** and **the nat inside proxymous** are two things.
  * **cloonix-xwy-srv** and **cloonix-xwycli** in an old log or diagram are
    the container console before *console_crun* of suid-power.


Saying something in the log
===========================

::

    KOUT            the process cannot continue
    KERR            an error, or an important warning, through the gate below
    KTRC            a normal event a reader may one day want; compiled out unless CLOONIX_TRACE
    KTRC_ON(flag)   the same, behind one file's own flag

Every branch that is not nominal says so with a KERR, WARNING or ERROR
first, with the cause (*strerror(errno)* after a system call). A branch is
nominal when it is the expected end of a thing: a peer that closes, a
reset from a client killed, a lookup of an object deleted meanwhile.
Informational output does not reach syslog by default.

**The gate.** Each KERR call site has its own gate, *glob_include/kerr_gate.h*:
at most *KERR_BURST* (10) lines per *KERR_WINDOW_MS* (10 s); the others are
counted, and the next line of that site that passes ends with::

    [N line(s) of this site not logged]

A site that repeats is therefore never silent and never a flood. Two sites
never share a gate. The loggers of the guest agents, *cxp_log* of
cloonix-cxp-srv, *agent_vlog* of the windows agent and *srv_log* of the
windows cxp server, use the same gate kept per format string. A counter in
the message itself (*N dropped so far*) stays the way to say how much
happened between two lines.

A block of lines that tells one thing, a history, goes through one gate:
*KERR_HEAD(passed, ...)* is its first line and sets *passed*, and each
*KERR_MORE(passed, ...)* after it is written or left with it, so a history
is never cut in its middle. *snf_hist_dump()* of *ovs_snf.c* and
*lan_history_dump()* of *lan_to_name.c* are written that way.

**The net name is written once.** KERR, KOUT and KTRC stamp the net of the
process ahead of the file name, so the text after them names the object and
never the net again.

A message in a library and again in its caller, a message naming the
functions on the way rather than the event, or a loss told by both ends of
one pipe, are the habits that produce volume. The tap child of a2b, c2c and
nat and its parent number their frames; on that pipe only the side that
drops a frame writes it, with its count, and the side that finds the gap
in the numbers says nothing more of it (nat's parent keeps a line: its own
state, why it was not reading).


Trace flags
===========

All off but *FANOUT_TRACE*, which writes into the log of its capture and
not into syslog; each in the file whose behaviour it explains, turning one
on rebuilds that binary only. The udp sequence gap of a c2c is a WARNING
at all times, rate-limited::

    ENDP_TRACE       uml_cloonix_switch/src/recv_add_lan.c     a lan refused because its endpoint was not there
    C2C_KILL_TRACE   uml_cloonix_switch/src/ovs_c2c.c          whether the switch had a handle on a c2c it destroyed
    LAN_TRACE        uml_cloonix_switch/src/recv_add_lan.c     the cabling sequence of a lan
    A2B_TRACE        uml_cloonix_switch/src/ovs_a2b.c          the per-a2b chatter
    PCAP_TRACE       uml_cloonix_switch/src/pcap_relay.h       the capture relay
    CRUN_TRACE       suid_power/src/crun.c                     each step of a container create and delete
    C2C_TRACE        proxymous_passt/src/c2c_peer.c            per-attempt c2c peering
    PASST_TRACE      proxymous_passt/src/passt_spawn.c         per-nat passt spawn and stop
    DEMO_TRACE       proxymous_passt/src/demo_exec.c           each step of playing a demo
    NAT_TRACE        ovs_nat_passt/src/ovs_nat_passt.c         the nat's own path
    BACKDOOR_TRACE   doorways/src/llid_backdoor.c              the backdoor legs
    FANOUT_TRACE     jscloonix/jscloonix/webshark/cloonix_fanout.c   on: the capture fan-out, into <n>_<num>_pcap_log, not syslog

A study: rebuild the one binary, run *debug/soak.py* long enough for the
event to happen a handful of times, read the trace lines next to the
warning they explain, turn the flag off, and put an answer that is needed
again into the message itself. *lan_history_dump* is the other shape, a 64
entry ring printed by the anomaly itself.


Fixed delays on the object paths
================================

Every fixed delay was measured against the fact it guessed at::

    ovs-start wait before qemu       -500 ms per guest      get_daemon_done() was the test
    guest eth ready                  250 ms -> the tap seen in /proc/<vswitchd>/net/dev
    container delete settle          544 -> 397 ms median   3 beats -> 1, delete_mount_retry never fired
    snf kil delay                    500 -> 200 ms
    kil: tail of shutdown stage 1    -900 ms
    kil: ovs destroy countdown       -1650 ms               a kil went from 4.80 s to 1.24 s

Measured over the *CRUN_TRACE* timestamps of one run::

    CREATE  overlay mount -> start        median   54 ms
    DELETE  asked -> freed                median  397 ms   p90 1057 ms

What is left of a delete is umounts and the *crun delete* spawn, not a
constant.


Shared code, where it lives
===========================

  * **rm_tree()**, *lib_io_clownix/src/file_read_write.c*: the one recursive
    delete of the switch and of ovs_drv. A mount point is emptied and kept.
    suid-power keeps *crun_utils_unlink_sub_dir_files*, which logs each
    unlink.
  * **unix_sock(), timed_connect(), is_unix_sock()**, *util_sock.c*: the
    parts of the four unix client connects.
  * **seqtap_hdr()**, *msg_layer.c*: header of a seqtap message, for the
    stream and the datagram reader.
  * **serve_push_ws()**, *web/web_http.c*: a websocket that pushes, topo and
    images; **ws_accept()** is the handshake, *ws_handshake()* the same with
    the *binary* subprotocol.
  * **demos/lib/demolib.py**: the functions every demo repeated. A demo
    drops a function only when its own was the same code; one that needs
    another form keeps it under the same name, and then keeps every function
    that calls it. The constants of a demo (*WIRING*, *LAYOUT*, *ETHS*,
    *ROOTFS_CHOICES*, *GATEWAY*, *ALL*) are read from the running demo as
    *DEMO.<name>*. *demos/lib* holds *demolib.py* and *wifilib.py* and no
    demo: the demo verb, *check_demos.py* and *demo_test.py* skip it and the
    dot names, and the canvas picker shows *demos/packages* only.


Static checks of the demos and the books
========================================

Run by *debug/testitall.sh*, no server needed::

    debug/check_demos.py      tree, summary line, commands printed, images,
                              guest sources, capture filter words, the
                              angle= of an eth facing its lan
    debug/check_docs.sh       binaries named, versions, capture filter words
    debug/check_doc_words.sh  judgement and filler words
    debug/check_cli.py        verbs as the client reads them
    debug/check_doc_cli.py    every *cloonix cli* line of the books and pages,
                              parsed offline by the client's own code
    debug/check_site.py       links of the pages, figures named and present,
                              every svg well-formed and shown by its book

The figure of a demo in the user book is drawn from the *LAYOUT*, *ETHS* and
*WIRING* of the demo itself::

    debug/demo_svg.py ../demos/routing/babel.py
    debug/demo_svg.py --missing
