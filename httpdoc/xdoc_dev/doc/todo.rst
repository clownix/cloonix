===============================
Things that could be added next
===============================

A list to choose from, each entry naming what exists in the tree so nothing
is built twice. What was given up on purpose, with the architecture that
would bring it back, has its own chapter, *Things set aside*.


Small, groundwork done
======================

  * **publish=<addr>:<hostport>:<guestport>**: passt's *-t* takes an
    address; *ports_to_passt* in *proxymous_passt/src/passt_spawn.c* builds
    it.
  * **An instantaneous cpu rate**: *dump pid* reports the average over the
    life of the process; a rate needs two samples and the half-second beat
    of the switch could hold the first.


From the windows test on a work machine
=======================================

The syslog of a failed *add win* was readable only with the source open
and the guest at hand. Each entry below removes one trip.

  * **why <guest>**: the startup state *qga_where* prints, the ping status
    of doorways, the exit text of the agent launcher and the qemu stderr
    tail the death KERR carries, answered to the client as one message instead of scattered over
    a syslog the user has to grep on the server.
  * **A windows launcher that returns at once**: *QGA_WIN_SCRIPT_LAUNCH*
    uses *start /b*, and cmd.exe came back 33s later on the test machine.
    The switch now waits for windows instead of failing, but the cause
    (Defender scanning the exes from the cd, or inherited pipe handles) is
    unmeasured. *powershell Start-Process* or *wmic process call create*
    detaches without inheriting the qemu-ga pipes; *C:\\cloonix-agent.log*
    timestamps say which of the two it was.
  * **Second family of demo helpers**: twelve demos keep an *ssh()* with a
    *log=* argument and a longer error text, thirteen a *put()* that traces,
    twenty-seven a *place()* built on *cli()*. Converging each pair on one form
    moves about 700 more lines into *demos/lib/demolib.py*; the error
    texts change, so it goes with a run of *debug/demo_test.py*.


From the toy runs of October 2026
=================================

  * **The green of a capturable interface**: the topology shows *eths* as
    soon as *sniff=on* is accepted, before its snf is up; a webshark sync
    waits for the snf (*timer_sync_wait* of *ovs_snf.c*). The endp_type
    could turn *eths* at the ok of the snf instead.
  * **Sniff off with a webshark open**: refused while the webshark runs
    (*handle_snf* of *web_ctl_post.c*); the webshark of that interface
    could be closed by the same click instead.


From the testitall run of October 18-19 2026
============================================

  * **The interface of a new bridge**: once in a night of *tst_max.sh*,
    *ovs_cmd_add_lan_endp* found no interface *_b_<lan>* after the *add-br*
    that *msg_lan_add_name* sends without waiting for its answer, and syslog
    showed no failed add-br. *bridge_intf_up* of *ovs_cmd.c* now waits 1 s
    for it and tells which case it was: a late interface, or a bridge ovs no
    longer holds because its lan was deleted meanwhile.
  * **The loop of suid_power waiting for its children**: one turn of 3.3 s
    with 68 ms of cpu while a container with 32 eth is made, its slowest
    callback the timers: the creation runs in a timer of suid_power, and
    the 3000 voluntary switches are the 1 ms naps of *execute_cmd_out*
    (*utils.c*) waiting for its commands, the *ip -batch* that makes and
    moves 32 veth pairs among them. The creation run in a child of
    suid_power, as the dtach consoles are, would keep the loop free.
  * **A turn of proxymous holding 14 s**: 1.4 s of cpu in one turn, the
    host at 97 % softirq during the demos. The late-loop warning of
    *channel.c* now names the slowest callback of that turn and its time,
    so the next one says which channel it was.
  * **One event, two lines from two processes**: the death of a container
    (*crun_beat* of suid_power, *crun_sigdiag_resp* of the switch), a
    refused byte pipe (doorways and the agent of the guest).


From the testitall run of October 23-24 2026
============================================

  * **The tcp of the nat**: *testbigflows.py* finds 242 Mbit/s for four tcp
    streams from a kvm to the host through the nat, against 1377 Mbit/s of
    udp on the same path, and the flow control of *passt_shuttle.c*
    (*flow_ctrl_update*) stopped the tap read 52 times in that phase. Its
    backlog thresholds, measured against a tcp flow, would say whether they
    are the limit.
  * **A move of an eth asked by two clients at once**: the client moves a
    cabled eth itself (*cable_first* of *cloonix*: it reads *dump commands*,
    unplugs, then plugs), so two clients moving the same eth at the same
    time act on a lan that is no longer the one of the eth, and the server
    refuses the second plug (*eth0 is already cabled to lan ...*, once in the
    night of *tst_max.sh*). A move done by the server in one request would
    close that window.
  * **The drop warnings of a flood**: one line every 10 s per process, so a
    c2c saturated for an hour writes some 360 lines of *the tap is not taking
    frames* (*rxtx.c* of *ovs_c2c*, *intf_flags.c*): 263 and 262 in this run.
    A gap growing from 10 s to 60 s while the drops go on would keep the
    count and the first warning, with fewer lines.
