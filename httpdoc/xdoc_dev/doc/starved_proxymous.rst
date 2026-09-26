=====================================================
A starved proxymous: trixie passes, rocky does not
=====================================================

This chapter is the record of one investigation, 17 and 18 september 2026.
*autotest.sh trixie* passed; *autotest.sh rocky* failed in the toy, always
in the same phase of *testbigflows.py*, *everything at once, 600s*. It keeps
what was seen, what each run ruled out, the cause, the change made, and the
instruments that stay in the code for the next time.


The symptom
===========

The test reported a file transfer refused::

    FAILED: everything at once, 600s
      scp up in the storm: cloonix refused the leg to 'bfB': KO_name_not_found

*bfB* was not missing by accident. The syslog of the toy showed the net
destroying itself a moment before, in three steps::

    cloonix-ovs-nat:     nat bfnat: proxymous said nothing for 3000 ms, stopping
    cloonix-main-server: ERROR IMPOSSIBLE CTRL CONNECTION TO PROXYMOUS
    cloonix-main-server: net stopping, asked by: a client asking for kil (llid=0 tid=0)

The main server had lost its proxymous, deleted every item, *bfB* with
them, and doorways then answered the scp with *KO_name_not_found*. The
last line was wrong: no client had asked, the server had decided alone.

One net is two halves in a toy. The servers run inside the crun container;
**cloonix-proxymous** runs outside it, on the machine, because it holds
what must be on the machine: the two tcp ports, passt, the udp relay of
the c2c. Each half watches the other by a heartbeat. The failure was
always the inside half no longer hearing the outside one.


Seven runs
==========

Each run of the suite lasts about forty minutes, so each build carried
several instruments at once. The table gives what each run added to the
picture.

.. list-table::
   :header-rows: 1
   :widths: 6 44 50

   * - run
     - what the logs showed
     - what it settled
   * - 1
     - the nat stopped after 3 s of silence, the server after about 10 s;
       two *IMPOSSIBLE CTRL* lines 102 ms apart for a timer of 20 beats
     - a beat lasted 5 ms under load, not 10: every timeout counted in
       beats was half its written value
   * - 2
     - *cloonix-proxymous-nemo: this process did not run for 10407 ms*,
       the same for mito; shorter stalls already in earlier phases
     - the silent side was proxymous, on the machine; the servers in the
       crun kept logging
   * - 3
     - stalls of 30 s; *cpu 7 ms, 242 voluntary and 73 forced switches,
       0 major faults* for a gap of 30940 ms
     - not a busy loop, not paging: the process was idle or not run
   * - 4
     - *user 27% system 55% softirq 16% irq 0% steal 0% iowait 0% idle 0%*
       during the stalls; one *epoll_wait* of 5 ms back 25958 ms late;
       no KERR line dropped
     - not the disk, not the outer host, not *syslog()*: a process ready
       to run was given no cpu for 26 s
   * - 4, kvm1
     - the bundle, same rocky image, same storm: *ok in 618.3s*, worst
       *epoll_wait* about 1.3 s late
     - the difference is in the toy, not in rocky alone
   * - 5
     - proxymous in *session-c1.scope*, the servers in
       *user@1000.service/app.slice/crun-nemo.scope/container*
     - the busy processes sit in a cgroup of their own; proxymous is
       outside it
   * - 6
     - toy started with *--cgroup-manager=disabled*: *systemd image*
       failed, *tisubSd never answered in 90s*
     - without a cgroup of its own, systemd as pid 1 of a container of the
       toy cannot start
   * - 7
     - same, that sub test set aside: the storm *ok in 612.1s*, no stall
       line at all, worst *epoll_wait* 848 ms late
     - the cause is confirmed
   * - after
     - the default changed and the defects below corrected: on rocky the
       bundle and the toy both *PASSED*, 87 checks each, the toy without
       one stall line; on fedora44 the toy *PASSED*
     - the change holds on the kernel that showed the defect and on
       another one


The cause
=========

.. image:: /svg/cgroup_toy.svg
   :align: center
   :width: 100%

On the left, the toy as it was. *crun --cgroup-manager=systemd* asks the
systemd of the user for a cgroup, *crun-nemo.scope*, and every process of
the net lives in it: the servers, the switch, and during the storm the
packet generators and the qemu, all of them asking for cpu without pause.
proxymous is started by *nemocmd* before crun and stays where the shell
was, in *session-c1.scope*.

A linux scheduler shares the cpus first between cgroups, then between the
processes of each. With kernel *6.12.0-211.54.1.el10_2* of rocky, a busy
cgroup beside a nearly idle one left the process of the idle one without a
cpu for tens of seconds. proxymous needs a few milliseconds per second; it
did not get them, its heartbeat stopped, and the net did what it must do
when its proxymous is gone.

On the right, the toy as it is now: no cgroup of its own, one cgroup for
both halves, as the bundle always was. The same kernel then shares between
processes only, and proxymous is served within a second.

The kernel lists describe this family of defects in the scheduler of the
6.12 line (EEVDF with group scheduling: a process outside a cgroup that
holds a cpu hog, the weight of that cgroup recomputed often). Which
corrections a given distribution kernel carries was not established; the
observation is that trixie's 6.12 did not show it and rocky's did.


What differs between the machines
=================================

The seven autotest machines are made by *debug/autotest/kvm_autotest_<distro>.sh*.
The columns are what the investigation looked at; *measured* means read in
a log of these runs, the rest comes from the builders.

.. list-table::
   :header-rows: 1
   :widths: 14 22 22 42

   * - machine
     - init, user bus
     - syslog file written by
     - note
   * - trixie
     - systemd, user bus
     - rsyslog
     - the storm passed with a cgroup of its own for the toy
   * - resolute
     - systemd, user bus
     - rsyslog
     -
   * - rocky
     - systemd, user bus
     - rsyslog reading the journal (imjournal)
     - kernel 6.12.0-211.54.1.el10_2, */tmp* on the ext4 root, selinux
       off (all measured or set by the builder); no *mac80211_hwsim*
   * - fedora44
     - systemd, user bus
     - rsyslog reading the journal (imjournal)
     - kernel 7.2.4-200.fc44 (measured); selinux off; no *mac80211_hwsim*;
       the toy suite passed there with no cgroup of its own
   * - tumbleweed
     - systemd, user bus
     - rsyslog, journald forwarding to it
     - kernel 7.2.4-1-default (measured); no sbin in a user's PATH nor in
       sudo's, so *modinfo* and *sudo modprobe* were not found; no
       *C.UTF-8* locale; 87 checks passed on both machines
   * - arch
     - systemd, user bus
     - syslog-ng, its *system()* source
     - no rsyslog package
   * - alpine
     - openrc, no user@, no user bus
     - rsyslog
     - crun never had a systemd cgroup manager there

Four differences between trixie and rocky were suspected and each was
cleared by a measure:

  * **/tmp on a disk**: the storm writes 64 MB files in */tmp*. iowait was
    0% during every stall.
  * **the log chain**: *syslog(3)* blocks when the log daemon does not read
    */dev/log*. KERR was given a socket that never blocks and counts what
    it drops; it dropped nothing.
  * **the outer host**: the two kvm run side by side. steal was 0%.
  * **memory**: no major fault, no swap, 2.1 GB free at the failure.

What remained was the only thing the toy has and the bundle has not: a
cgroup of its own.


What was changed
================

For the cause:

  * *toy_targz/tools_crun/setup.sh*: *crun create* with
    *--cgroup-manager=disabled* by default. *CLOONIX_TOY_CGROUPM=systemd*
    asks for the former behaviour; the start prints which is used.
  * *cloonixsbininit*: with *CLOONIX_WITH_SYSTEMD=1*, a *mkdir* is tried in
    */sys/fs/cgroup*. When it fails the container starts without systemd
    and */run/cloonix_no_systemd* holds the reason. The chapter *Things set
    aside* describes what would give this back.

Defects met on the way, each corrected where it was:

  * **The beat** (*lib_io_clownix/src/channel.c*): 100 beats lasted 1029 ms
    idle, 502 ms under a flood, 625 ms with sparse events. A beat is now
    two heartbeat periods of the monotonic clock in every case: 1000 ms
    measured in all three.
  * **Two watchdogs on one proxymous** (*uml_cloonix_switch/src/proxymous.c*):
    the older one counted beats and fired at 10 s under load. It now covers
    only the time before the connection exists; *proxycrun.c* alone
    judges a silent proxymous, 30 s on the wall clock.
  * **The nat's watchdog** (*ovs_nat_passt.c*): 3 s, and fed only when the
    main server sent a pid request, so it measured the server as well. It
    sends its own beat every second and allows 20 s, as *ctrl_link* does.
  * **The c2c verdicts on the peer** (*ovs_c2c.c*): counted in beats, 9 s
    nominal. On the wall clock, 20 s.
  * **The reason of a stop**: *asked by* names the server when no client
    asked.
  * **The del_all pass on an a2b**: asking again while a delete is in
    flight is the pass waiting, no longer an ERROR per second.
  * **A close forced on a slow client** (*doorways/src/llid_traffic.c*):
    a session asked to close with bytes still queued was cut after 30 s
    whatever the queue did. On fedora44 the storm left more than 30 s of
    a 64 MB scp in that queue and the transfer was cut. The 30 s now count
    only while nothing leaves the queue.
  * **Dead code**: *nat_tcp.c*, *nat_udp.c*, *nat_icmp.c*, *nat_utils.c* of
    proxymous, about 1500 lines no request reached since passt; removed.


The instruments that stay
=========================

Four lines now exist in every daemon and need nothing to be switched on.

A stall says what the process did and what the machine did::

    WARNING this loop did not beat for 5615 ms (cpu 16 ms, 222 voluntary and
    27 forced switches, 0 major faults; the machine over the last 6015 ms:
    user 27% system 55% softirq 16% irq 0% steal 0% iowait 0% idle 0%)

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - what the line shows
     - what it means
   * - cpu close to the gap
     - the loop turned too slowly, the work is in this process
   * - little cpu, forced switches
     - the process was ready and was not given a cpu
   * - little cpu, voluntary switches, major faults
     - the process waited in the kernel: a call that blocks, paging
   * - steal
     - the hypervisor did not run this machine
   * - softirq
     - the network stack of the kernel took the cpus
   * - iowait with idle
     - the machine had time, something waited for a disk

The loop says where a late turn lost its time, at most once per ten
seconds and only above 2 s::

    WARNING late loop, 1053 turns in 37743 ms: epoll_wait came back late by
    11007 ms in all, 8046 ms at worst (the cpu was not given); the callbacks
    took 21764 ms in all, one turn 21752 ms at worst with 1 ms of cpu, ...

KERR says when lines were lost::

    WARNING 2989 line(s) before this one were dropped: the log daemon did
    not read /dev/log for 5005 ms

*testitalltoy.sh* prints at its start the cgroup of each proxymous and of
each main server, with the kernel version; a failed autotest trace ends
with the last 200 lines of *kerr1_outputs.log*, *free*, the swap and fault
counters, the pressure files, *df -T* and the dmesg lines about stalls.

To look at a machine by hand::

    cat /proc/$(pgrep -f cloonix-proxymous-nemo | head -1)/cgroup
    cat /proc/$(pgrep -f "cloonix-main-server.* nemo" | head -1)/cgroup
    grep -H . /proc/pressure/cpu
    grep -E "did not beat|late loop|dropped" /var/log/syslog | tail
