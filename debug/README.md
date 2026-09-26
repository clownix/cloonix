# debug/ — tools for working on cloonix, not for using it

Nothing in here is shipped. `build_tools/pack_cloonfs` stages `demos/` into
`share/cloonix/demos` and deliberately leaves this directory alone, because
these scripts create and destroy objects at random, sweep processes and
unmount trees. They have no business on an installed system.

They are run from the source tree:

    cd sources/debug
    ./soak.py nemo 4

The nets they talk to are the ones already running; nothing here needs
installing.

## testitall.sh -- the whole non-regression, first failure ends it

    ./testitall.sh

Nine phases in a row, the short ones first and the long ones last, so a
failure is known at once: the two nets nemo and mito cleaned and started;
the doc checks below plus a `sphinx-build -W` of the two books;
`testitall.py`, one check per item of cloonix with a sniff counting the
packets of each (cnt, kvm, lan, eth kinds, unplug, del, scp, vmount,
cnf caps, rstp, dump, position, tap, nat on a cnt and on a kvm with a
published port, a2b, c2c to mito, save, the web port, del all) and the flows
(iperf3 between two perf_trixie, under a sniff read and unread, scp of 48 MB
through the agent alone and under load, an a2b rate, tcp and udp across the
c2c, the nat with iperf3 on the host, 32 MB by scp to a kvm); then every
`sub_*.py` (below); then `demo_test.py` on every demo, nemo and mito each
taking the next one, the long ones last (`LONG` in it) and the ones that
drive other nets themselves or boot a kvm (`ALONE`) at the very end, alone; then `testfailures.py`, `testbigflows.py` and
`teststorm.py` (below), the two stress ones run long there (a 600 s
all-at-once storm, a 1800 s moving-target storm);
then `tst_max.sh` for the endurance. The docstring of `testitall.py` lists every check and
what it asserts. `TESTITALL_PHY=<iface>` adds and deletes a macvlan on
that interface; `TESTITALL_CNT` and `TESTITALL_KVM` name other images than
trixie and trixie.qcow2.

## testitalltoy.sh -- the same on two toys

    ./testitalltoy.sh [cloonix-toy-<num>.tar.gz]        the whole run
    ./testitalltoy.sh start [cloonix-toy-<num>.tar.gz]  the toys only, left running
    ./testitalltoy.sh run ./testfailures.py nemo mito   one test on them
    eval $(./testitalltoy.sh env)                       the same for this shell
    ./testitalltoy.sh stop

A test started by hand without `run` or `env` takes the installed client
and lands on the bundle's nets, not on the toys.

The newest `${HOME}/cloonix-toy-*.tar.gz` when none is given. Any nemo or
mito running, bundle or toy, is killed; the tarball is unpacked twice under
`${HOME}/testitalltoy/nemo` and `/mito` (`TESTITALLTOY_DIR` moves it),
`setup.sh` names each, both are started, and phases 2 to 8 of
`testitall.sh` run on them through the toy's own client, config and demos:
`CLOONIX_CLIENT`, `TESTITALL_CFG`, `CLOONIX_DEMO_DIR`, `TESTITALL_SYSLOG`.
`TESTITALL_TOY` tells the tests what is not the toy's: the tap of a toy is
in its network namespace and is not addressed from the host, a killed net
leaves its proxymous and container init alive, a net restart is
`<net>cmd stop` then `start`. Everything else, kvm included, runs as on a
bundle; `/dev/kvm`, `/dev/vhost-net` and `/dev/net/tun` must be readable
and writable by the user. With the toy on a single uid mapping (no subuid
lines or no `newuidmap`, the toy chapter of the user book has the whole
of it) `TESTITALL_TOY_SINGLE_IDMAP` is set and `demo_test.py` skips the
demos whose daemon drops privileges. The endurance is `tst_maxtoy.sh`:
`testitalltoy.sh start`, then `soak.py` on the toys through the exports of
`env` (`TESTITALL_TOY` and `CLOONIX_CLIENT`: the toy's client, and its work
zones and syslogs inside the rootfs of each net, where the soak reads what
a dead container left). The toys are stopped at the end of the whole run,
pass or fail.

## autotest/autotest.sh -- both suites in two kvm

    cd autotest
    ./kvm_autotest_trixie.sh            once: bulk/kvm_autotest_trixie.qcow2 (./doitall.sh --all: every distro)
    ./autotest.sh                       trixie; or: ./autotest.sh rocky
    cloonix cli nemo ssh kvm1 "tail -f /home/user/results_outputs.log"
    cloonix cli nemo ssh kvm2 "tail -f /home/user/results_outputs.log"
    cloonix cli nemo ssh kvm1 "tail -f /home/user/tst_max_output.log"
    cloonix cli nemo ssh kvm2 "tail -f /home/user/tst_max_output.log"

testitall.sh on the bundle in kvm1 and testitalltoy.sh on the toy in kvm2,
two unconnected kvm of nemo made from kvm_autotest_<distro>.qcow2, with no nat, as user
`user`; then tst_max.sh in kvm1 and tst_maxtoy.sh in kvm2. Each writes
results_outputs.log with a summary at its end, kerr1_outputs.log (the KERR
lines of syslog during the suite), then tst_max_output.log and
kerr2_outputs.log for the endurance. The chapter *The autotest* of the
developer book has the whole of it.

## sub_*.py -- one feature each, runnable alone

    ./sub_kvm_persistent.py             defaults: nemo mito
    ls sub_*.py                         the list

Each file is one check of one feature testitall.py does not touch, on the
two nets already up, with the helpers of `testitall.py` (through
`sublib.py`); it creates what it needs under names starting `tisub`,
deletes them at its end, and starts by deleting them if a previous run left
them. Run by testitall.sh as its fourth phase, in the order `ls` gives.

  sub_c2c_udp_garbage       malformed datagrams on the udp port of a c2c:
                            dropped, same c2c processes, the ping crosses

  sub_c2c_replug            a c2c unplugged and plugged back at once and
                            after a pause, five rounds: dump commands follows
                            each move, the plug back is done and not taken
                            for already on, the ping crosses, the next
                            unplug is accepted
  sub_c2c_mac_mangle        cnf mac_mangle= given after the c2c is
                            cabled: every frame from the peer wears the mac
                            on this side, arp sender included, the ping
                            works both ways; a bad mac and a cnt refused
  sub_cnt_limits_enforced   cnf cpu=25 measured on a spin loop, ram=100
                            kills a 200 MB shell, 0 lifts both
  sub_cntwifi               wlan=1 and =2 give wlan0 wlan1 on the air
                            cntwifi; plug and unplug move a wlan between
                            airs, an eth on an air and a wlan on a lan are
                            refused, no item named cntwifi; dump json, dump
                            list, the /topo of the canvas and the capture fifo
                            of each air follow; the radios are returned (loads
                            mac80211_hwsim with sudo, skips without)
  sub_client_exits          the client into a closed pipe ends quietly with
                            status 0; a wrong verb, net or word ends with a
                            status and a sentence, no traceback
  sub_fail_bad_images       a zero-byte and a text qcow2, a file as rootfs,
                            a missing added_disk: refused with a reason
  sub_fail_publish_busy     publish= on a host port already held: ko at
                            once, passt back on the previous ports, the nat
                            still carries traffic, a free port then works
  sub_ftopo_consistency     ftopo holds exactly what was built, the lans
                            under each endpoint, ovs list-ports agrees
  sub_host_clean            processes, mounts, netns and links of the host
                            identical before a full topology and after del all
  sub_kvm_added_cdrom       added_cdrom=: a second rom, the file's bytes
  sub_kvm_added_disk        added_disk=: a virtio disk, bytes written at
                            1 MiB are in the host file and survive a reboot
  sub_kvm_fullvirt          --fullvirt: sata root, boots, pings
  sub_kvm_no_reboot         --no_reboot: reboot inside ends the kvm
  sub_kvm_persistent        --persistent on a kvm and a cnt, on copies in
                            bulk: the marker survives; without it a fresh
                            overlay; two refusals on an image in use
  sub_kvm_resources         ram= cpu= as nproc and MemTotal
  sub_kvm_save_boot         save, then a kvm booted on the saved qcow2
  sub_kvmwifi               the sub_cntwifi of the kvm: wlan=1 and =2 give
                            wlan0 wlan1 on the air kvmwifi, made by the agent;
                            plug and unplug move a wlan between airs, a
                            kvm radio onto cntwifi too, an eth on an air and a
                            wlan on a lan are refused, no item named kvmwifi;
                            dump json, dump commands, the /topo of the canvas and
                            the capture fifo of each air follow; two radios on
                            one air ping, the ping captured on the air, and
                            fails off it (nothing on the host); with
                            mac80211_hwsim, a cnt radio on that air pings the
                            kvm both ways, captured on the air
  sub_mac_addr              mac_addr= on a cnt and a kvm, read back
  sub_max_eth               32 interfaces on two cnt, all cabled and pinged,
                            33 refused naming the ceiling
  sub_nat_ipv6_dns          dhclient on a kvm behind a nat: the nameserver
                            passt forwards, a name resolves; a global ipv6
                            by RA when the host has one (skips otherwise)
  sub_position_readback     cnf x= y= and angle= replayed to a topology
                            subscriber, forgotten after del
  sub_rpc                   rpc: a verbatim tap_add and del_name; an
                            unknown fctident and a non-json refused
  sub_sniff_modes           sniff --line, --dump, --count read
  sub_targz_image           a cnt image left as <image>.tar.gz in bulk,
                            packed from a running cnt: extracted once into
                            <image>_targz mounted read only, a second add without
                            extraction, --persistent refused on both names,
                            dump images kinds, a directory of the plain name
                            first, a stale .tmp erased, two adds at once on
                            one extraction, a corrupt tar.gz refused naming
                            its log, a missing image named in both forms
  sub_systemd_image         trixie_systemd (TESTITALL_SYSTEMD) with the
                            CLOONIX_WITH_SYSTEMD=1 opt-in: pid 1 is systemd,
                            running/degraded, scp, a service; without it
                            pid 1 is cloonixsbininit
  sub_trace_events          trace crun, nspace, json_cli, qemu see the
                            creations; trace probe finds the pipe whole
  sub_twonets_samename      the same names on the net and its peer at once:
                            eth0 and wlan0 macs differ, an access point and a
                            station of the same names join on each net at
                            the same time (the radios with mac80211_hwsim)
  sub_web_ctl               /ctl/add-cnt add-lan add-edge del-edge del-node
                            rma topo commands as the browser sends them
  sub_webshark_wifi         webshark on a written pcap of 802.11 frames, no
                            radio: link type 127 to the page, the decoding
                            of beacon, joins, EAPOL, encrypted data, ARP and
                            ICMP, the labels of the tree, the filters eapol,
                            ip and the one of the button Data only
  sub_web_items             every other /ctl route of the canvas by its effect:
                            add-tap add-nat add-a2b add-c2c, cnf-a2b on a
                            ping, snf with webshark start stop and its 409,
                            position poseth, add-vm reboot-vm save-vm,
                            add-phy; then add cisco (--no_qemu_ga), -s, --cfg
  sub_web_refusals          401, 403 on origin and .., 400, 404 of the web
  sub_web_ws_topo           the /topo and /images websockets: first frame is
                            the snapshot

Not covered, on purpose: `--uefi` (trixie.qcow2 is grub-pc only, it cannot
boot uefi; the windows demo covers it), `install_cdrom=` (boots an
installer), two guests behind one nat (passt serves one guest address,
10.117.0.10), and what testitall.py, testbigflows.py and testfailures.py
already do. Not covered: `--via-unix`, a debug path
straight to cloonix-cxp-srv (see the dev book, conventions), and
`dump ip <nat> <guest>`, which answers only for a guest the nat has
already seen.

## testbigflows.sh -- every flow path above its limit

    ./testbigflows.sh

Needs the image flow_trixie (vm_create/cnt_kvm_demo/cnt_flow_trixie.sh:
iperf3, trafgen, hping3, socat). Four flow_trixie on one lan, one on mito.
The generators: iperf3 -P 4 for tcp, iperf3 -u -b 0 for udp with no brake,
trafgen for 64-byte frames at the packet rate of the machine. The scenarios:
the lan alone (tcp, udp, pps); a sniff read under both floods (delivered
and missing counted from the packet numbers, the lan rate kept); a sniff
whose client never reads under the floods (the lan rate kept, dump commands and
ssh answer, doorways and server memory bounded, a fresh sniff after, no snf
left); four sniffs at once; 128 MB by scp under the floods and a sniff; the
c2c under tcp, udp and pps floods then still peered and back to half its
tcp rate; an a2b at rate=100 kB/s and at delay=500 ms under a 50 Mbit/s udp
flood; the nat under a udp flood then tcp still crossing; ssh stdin and
stdout streams of 128 MB under the floods; two kvm (one sniffed, one not) with
tcp, udp, a sniff on the kvm port, scp and ssh over the virtio serial under
load; the c2c with 8 tcp streams each way at once, udp each way, frames
each way; the a2b with loss, silentms and rate each way under floods; the
nat from a kvm on dhcp, tcp and udp both ways including the host into a
published port; the tap both ways (iperf3 and sudo on the host); then a
storm of all of it at once for TESTBIGFLOWS_LONG seconds (180) with scp
and ssh round trips in a loop, memory watched throughout, every path pinged
after; the rss of every daemon before and after. Figures are printed; the
failures are health. Also run by testitall.sh as its seventh phase; about
25 minutes. Not covered: phy, and the browser hop of webshark.

## testfailures.sh -- things going wrong on purpose

    ./testfailures.sh

The contract: a fault is refused or reported with a reason, nothing hangs,
no ghost is left, the rest of the net keeps working, the same name can be
created again. Forty refusals of bad input with dump topo unmoved, garbage
on the doors port; kill -9 of the dumpcap, the snf, the a2b, the passt and
the nat, the c2c on both nets, the qemu of a kvm, the init of a cnt, each
followed by the object back or gone, no process left, a re-add; clients
killed mid scp, ssh and sniff with the fds of doorways and proxymous back;
100 connections held on the doors port and on the web port, oversized and
random requests; the agent killed inside a kvm and a cnt, an interface
flapped, a reboot from inside; the image chowned to root (sudo) and
repaired; a full net killed with no process, namespace or mount left, then
restarted. Also run by testitall.sh as its sixth phase.

## teststorm.sh -- flows on a moving target

    ./teststorm.sh
    TESTSTORM_SECS=600 TESTSTORM_SEED=4242 ./teststorm.py nemo mito

Needs flow_trixie on both nets. Four flows that never stop -- an iperf3
client inside the guest, run again every 10 s until the time is up, counting
the runs that got through: tcp on the lan, udp at 100 Mbit/s on the same
lan, tcp on a second lan on not sniffed ports, tcp across the c2c -- and, thrown
at random under them for TESTSTORM_SECS (240): a sniff read on any flowing
port with a random filter, a sniff never read, capture switched off and on
under the not sniffed flow, one end of a flow unplugged and plugged back, an a2b
inserted under the udp flow with delay, loss or rate, containers added on
the flow lan and deleted later (three alive at most), a tap and a nat added
and deleted, the c2c deleted and rebuilt, one kvm booted on the flow lan,
the dumps, positions. Every ten throws: dump commands within 10 s, the rss of
every daemon bounded, the snf count bounded, every lan of ftopo a bridge
with ports; a trace json_cli subscriber lives through it all. After: every
flow reports its runs (half got through at least), every path pings with no
loss, tcp on each at half its pre-storm rate at least, fresh sniffs count on
a sniffed and a not sniffed port, no snf, process or bridge left for what was
deleted, ftopo holds the standing topology only, rss bounded, both nets
erased. The seed is printed, so a failing run repeats; on a failure the
last fifteen throws and the last commands are printed and the nets are left
as they are. Also run by testitall.sh as its eighth phase; about 6 minutes.

## The checkers

Eight scripts that answer one question each and exit non-zero when the answer
is no. None needs a running net and none writes outside the source tree:

    ./check_docs.sh          does every cloonix-* named in the docs exist?
    ./check_doc_words.sh     does every line of the docs carry a fact?
    ./check_cli.py           is the command spelled as the client reads it?
    ./check_doc_cli.py       does the client take every cli line of the docs?
    ./check_site.py          are the links, the figures and the svgs all there?
    ./check_wire.sh          do the C and the python agree on the wire?
    ./check_cloonfs_bins.sh <rootfs.tar.gz>
                             does that rootfs carry every binary cloonix runs?
    ./deadcode.sh            what does the linker throw away?

The first six run on a checkout and nothing else. The seventh takes the
rootfs tarball as its argument, and the tarballs are not in the checkout.
The eighth builds every daemon, so it needs what a build needs.

Each carries the failure that produced it in its own header. In short:

`check_docs.sh` reads every `cloonix-*` name out of `httpdoc/xdoc_dev` and
`httpdoc/xdoc_user` and checks it against what `pack_cloonfs` copies into
`cloonfs/bin`. Exceptions go in `check_docs_known.txt`.

`check_doc_words.sh` reads `httpdoc`, the two books included, for two
lists of words: judgement, which tells the reader what to think of a fact,
and filler, which removing changes nothing. The rule it enforces is the one
`httpdoc/README.md` states. A line that names a banned word so as to ban it
goes in `check_doc_words_allow.txt`.

`check_doc_cli.py` loads `cloonix/cloonix` as a module and gives every
`cloonix cli <net> ...` line of httpdoc and the two books to its own
`expand` and `to_json`, with the nets of `cloonix_config.py`: a verb, a
keyword or a count of arguments the client refuses is a doc out of step
with the code. A line holding a `<placeholder>` is checked up to its verb.
The refusals the docs show on purpose are listed in its SHOWN_REFUSED.

`check_site.py` checks the local links and images of `httpdoc/*.html`
(`downloads/` is filled on the server, `build_*` only once built), and for
each book that every figure an rst names exists, that every svg parses as
xml and that no svg of `svg/` is left shown by no page.

`check_cli.py` reads VERBS, CLI_VERBS and the completion tables out of
`cloonix/cloonix` itself, so the grammar is written in one place and this
cannot approve a word the client refuses. It checks the two things a rename
actually broke, and nothing else. First, a brace closing after the verb
instead of after the variable: 329 lines wrapped the net variable and the
verb together inside one `${...}`, which `bash -n` accepts and which then
dies at run time with `bad substitution`. Second, a word of the command that has been renamed away --
`lst`, `kil`, `add snf`, a verb of cli standing at the top level where only
version, list, net and cli belong. It follows `${CLOONIX}` as well as the
literal word, `smoke.sh` having stayed on the old order through every sweep
that grepped for `cloonix`. It does not argue with prose: the C comments say
"cloonix cli ssh" in eighty places and mean the mechanism, and a line
showing the old form beside the new one is teaching the rename.

`check_wire.sh` compares the constants in `glob_wire.h` against the copies in
`cloonix/cloonix`, which is a standalone python client and cannot include a C
header. Run it after any change to `glob_wire.h`; `wire_baseline.txt` is what
it compares against.

`check_cloonfs_bins.sh` unpacks the rootfs tarball and looks for the binaries
`pthexec.c` names, that being the one place they are listed.

`deadcode.sh` builds every binary with one section per function and
`--gc-sections`, at `-O0 -fno-inline` so an inlined function is not mistaken
for a dead one, and prints what the linker discarded: the binary's own dead
functions, then the library functions that every binary linking the library
drops. Anything listed is unreachable from main. It leaves the tree clean and
deletes nothing itself. What it lists today stays: two functions behind
compile-time trace flags, and the readers of `cxp_wire.c` in `cloonix-cxp-cli`,
which links the file for its escaper only.

## soak.py — random create and destroy until something breaks

    ./soak.py [net] [workers] [seed]
    ./soak.py nemo 4
    SOAK_PEER=mito ./soak.py nemo 4          # enables c2c, needs both nets
    SOAK_C2C=2 SOAK_PEER=mito ./soak.py nemo 8   # keep two c2c alive at once
    SOAK_ONLY=cnt,lan ./soak.py nemo 4       # one fault at a time
    SOAK_MAX_LIVE=50 ./soak.py nemo 8        # a bigger population

Several worker threads each pick an object kind by weight and act on it.
Objects get a random lifetime and a reaper retires them, so creation and
destruction are always interleaved. The pace shifts on its own between
bursts, a normal rhythm and long pauses. Ctrl-C stops the workers, deletes
everything, waits for it to go, and checks the machine is as clean as it
was found.

**Weights**, which are estimates and one table at the top of the file so
they can be argued with: `cnt` 20, `lan` 14, `snf` 6, `tap` 3, `nat` 3,
`a2b` 1, `kvm` 1 (skipped without an image), `c2c` 1 (needs `SOAK_PEER`).
`phy` is absent on purpose: it takes a real interface off the host.

## What the soak found, and what holds

The investigations that produced the tools above ran for weeks and were
written up here at length. The record is in git; what is still true of the
tree is this.

**A daemon could be connected to exactly once.** `connect_from_ctrl_client`
in the snf, a2b and c2c deleted its listening llid on the first accept, so
every later connect from the switch was refused. Most of the snf symptoms of
a month came from that line. The listener is kept; a second connection
replaces the first.

**The record is not the process.** Everything in `ovs_snf.c` keys on the
record, and a process outlives its record whenever the record is freed by
a path that never reaches the process. Every kill and every existence test
now reads `/proc/<pid>/cmdline` and requires both `cloonix-ovs-snf` and the
tap name (`snf_pid_is_this_tap`), because pids are reused thousands of
times an hour under soak and a stale kill lands on the replacement.
`snf_find_pid_of_tap` asks /proc who owns a tap where a wrong answer costs
a leaked process, and nowhere else.

**A stop flag that was never reset.** `del_snf_ethv_sent` was set in two
places and cleared in none, so the fourth capture generation on one eth
left its snf alive at delete with not one line in the log. It is cleared
wherever a capture is (re)started.

**The mirror row.** It is named `mir<vhost>_<lan>` by one function both the
add and the del call. A mirror a bridge still lists cannot be destroyed --
ovsdb keeps the reference and the destroy is a silent no-op -- so the add
removes a stale row from its own bridge's list and lets ovsdb collect it,
and `free_snf` removes the row of an snf that died any other way than being
uncabled. The add is skipped while the snf has no pid, `snf_process_started`
re-issues it: vsctl commits before vswitchd finds the tap missing, and a
failed add left a row behind.

**The life of an snf is a ring, printed only when something goes wrong.**
`snf_hist`, 4096 entries, cleared for a tap when an snf starts on it (tap
names are recycled with object ids). Fifteen anomalies dump it. Every
assignment to the record's state fields must have an `snf_hist` within four
lines, and every `snf_hist_dump` must be inside a braced block; both are
worth re-checking after touching `ovs_snf.c`. `lan_history` in
`lan_to_name.c` is the same shape.

**Every refused request says why.** The scan: find every `send_status_ko`
and check its enclosing block calls KERR. It found 28 silent refusals; one
deliberate exception remains (`cnt_del_lan`, already logged in more detail
above it). Expect a newly loud condition to be too loud at first: read the
digest, quieten by count, not in advance.

**A container that died at birth.** `crun_utils_prepare_with_delete` cleaned
a leftover `_k_<id>_<n>` in the default namespace while the veth's last
step had moved it into the cloonix one; object ids come round every
thousand allocations, so the same id died at birth once per cycle. Both
namespaces are cleaned now. The stderr of every `ip` step is kept, since
exit 2 alone covers three different faults.

**Timing.** The fixed delays and what replaced each are the table in the
developer book, *Fixed delays on the object paths*. `DELETE_SETTLE_BEATS`
in `suid_power/src/crun.c` is 2. `1 mount(s) still there after the delete`
on the persistent cnt of sub_kvm_persistent was not timing: the idmapped
lower of a persistent cnt was never unmounted by the delete steps, only by
that retry; step 3 unmounts it now. If the line appears again, that is
timing, and 3.

**Still open.** An snf the switch never connects to (`llid 0` and `pid 0`
at the stop): both sides now say whether the socket file existed and how
long each startup step took, and it has not recurred since.

**Traces.** No trace flag is on. The flags, the file each lives in and the
question each answers are one table in the developer book, *Trace flags*.

## fail_spider.sh — the burst that loses cabling, and a cleaner

    ./fail_spider.sh [net] [count] [rootfs] [ack] [rounds] [wait]
    ./fail_spider.sh clean nemo

The first form creates many containers and cables them as fast as possible,
with no retries and no checking, and reports what the server ended up with.
It exists to reproduce a specific fault on demand.

`clean` is the one used daily: it stops the net if it is running, kills
leaked helper processes, unmounts deepest-first and only then removes the
tree. It refuses to delete anything if a mount will not come free, and says
what is holding it -- `rm -rf` across a live mount point is a bad afternoon.

Run it between soak runs. Debris from a previous run gets reported as this
run's fault otherwise, which has cost real time.

## test_round.sh — one bug, one command, one paste

    ./test_round.sh [net] [minutes] [peer]

A single investigation: it checks the installed binary actually contains
the round's instrumentation, refuses to start if not, runs the soak
restricted to the kinds that matter, stops on its own, and then reads the
log back and says which of the possible faults it is.

`NEED_BIN`, `NEED_STR` and `NEED_WHAT` at the top name the marker for the
current round. **They change with each round**, and the string must be one
that reaches the binary -- a `KERR` message, not a comment.

## How to use these to find a bug

The method that worked, repeatedly:

  1. Find a place where something goes wrong and nothing is written.
  2. Add a trace there, behind a `#define`, on for the study.
  3. Run once. Count what went in against what came out.
  4. The gap names the branch.

The traces already in the tree, all off, are the table *Trace flags* of
the developer book.

And the two habits that save the most time:

**Check the binary before the run.** `strings <binary> | grep "<marker>"`.
Several investigations were lost to a stale install, and the round script
now refuses to start without it.

**When a report and the code disagree, suspect the report.** A good share
of what these tools reported turned out to be the tools themselves --
a substring match, a check that ran before a wait instead of after, a
count taken while a teardown was still running. Every fault prints the
evidence it was based on for that reason.
