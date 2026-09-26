======================================
The autotest: both suites in two kvm
======================================

*debug/autotest/autotest.sh* runs the two non-regression suites of
*debug/* inside two kvm of the net nemo, so that a delivery is tested on a
machine that has nothing of the development host: *testitall.sh* on the
bundle in *kvm1*, *testitalltoy.sh* on the toy in *kvm2*, each as the user
*user*, each writing its logs in its */home/user*; then the endurance on
each, *tst_max.sh* in kvm1, *tst_maxtoy.sh* in kvm2. No nat: the one
interface of each kvm is on the lan *autolan* with the tap *autotap* of the
host, for the browser of the host only (The web of the nets under test).

.. image:: /svg/autotest.svg
   :align: center
   :width: 100%

::

    cd debug/autotest
    ./kvm_autotest_trixie.sh                        once: bulk/kvm_autotest_trixie.qcow2
    ./autotest.sh                                   kvm_autotest_trixie.qcow2
    ./autotest.sh rocky                             kvm_autotest_rocky.qcow2
    cloonix cli nemo ssh kvm1 "tail -f /home/user/results_outputs.log"
    cloonix cli nemo ssh kvm2 "tail -f /home/user/results_outputs.log"
    cloonix cli nemo ssh kvm1 "tail -f /home/user/tst_max_output.log"
    cloonix cli nemo ssh kvm2 "tail -f /home/user/tst_max_output.log"


The machines, made once
=======================

The autotest must run on every distro cloonix supports, so there is one
machine per distro, *kvm_autotest_<distro>.qcow2*, for trixie, alpine,
arch, fedora44, resolute, rocky and tumbleweed. *autotest.sh* takes the
distro as its argument, trixie by default, and refuses any other name.

*debug/autotest/kvm_autotest_<distro>.sh* boots the base *<distro>.qcow2* on nemo
with a nat to the mirror, as *kvm_python_trixie.sh* does, and writes into
it everything the suites ask of the machine; *debug/autotest/doitall.sh --all*
runs the seven. The mechanics and what every machine gets are in
*kvm_autotest_common.sh*; each *kvm_autotest_<distro>.sh* holds only its
package command and the mechanisms its init and syslog need::

    python3 gcc, a libc-dev, sudo       the client and the tests, check_wire.sh
    newuidmap (uidmap, shadow...)       the second uid mapping of the toy
    rsyslog                             /var/log/syslog, read by demo_test.py: Debian's
                                        writes it, elsewhere rsyslog.d adds the file, 0644,
                                        and journald forwards where rsyslog reads /dev/log;
                                        arch has no rsyslog, syslog-ng writes the file
    dbus                                the buses crun's cgroup manager opens: the system
                                        one for a cnf cpu= ram= of the bundle, the user one
                                        for the toy's own container
    user                                created when the base has none
    /etc/sudoers.d/user                 passwordless sudo, fail_spider.sh and the tap check use it
    /etc/subuid /etc/subgid             user:100000:65536
    linger for user                     user@<uid> and its bus, with no login (systemd)
    kvm, vhost-net, tun                 mode 0666, the nested kvm: udev rules, or a local.d
                                        script on alpine (openrc, mdev)
    /etc/hosts                          the hostname, so sudo says nothing
    /var/lib/cloonix/bulk               mode 777
    no firewall                         firewalld, ufw, nftables, iptables: stopped, disabled, masked, flushed

A machine is kept only if every tool the suites and the runner call
answers (*TOOLS* in *kvm_autotest_common.sh*: tar, python3, gcc with a libc
to link, newuidmap, setsid, pgrep, ip ...) and *autotest.sh* asks the same
of each kvm before pushing anything to it; *iperf3* is optional, the tap
flow is measured only with it.

RHEL kernels (fedora44, rocky) ship no *mac80211_hwsim*: the wifi sub test
and the wifi demos are skipped there, each saying so, as the user book
lists that module among the optional ones.

The toy asks for no cgroup of its own (the toy chapter of the user book
says why), so a user bus is no longer needed by it on any machine; the
runner still says when there is none, as alpine has no user@. The systemd
image sub test, in a toy, sees the starter fall back and prints its reason
instead of failing; *CLOONIX_TOY_CGROUPM=systemd ./autotest.sh <distro>*
runs the toys the old way. The nat, the mirror and the package manager are not used
again once the machine is made.


What the host needs at a run
============================

::

    ${HOME}/cloonix-bundle-<num>.tar.gz    the newest is taken
    ${HOME}/cloonix-toy-<num>.tar.gz       the newest is taken
    /var/lib/cloonix/bulk/kvm_autotest_<distro>.qcow2   the machine of the distro asked
    trixie.qcow2 trixie perf_trixie        the images the suites name, copied
    flow_trixie trixie_systemd             into the bulk of each kvm
    nested kvm                             the kvm of the inner nets
    sudo                                   the address of autotap

The image list is *IMAGES* in the script; *AUTOTEST_IMAGES* in the
environment replaces it. A container image goes to the kvm as
*<image>.tar.gz*, the form the server extracts at the first *add cnt*:
taken as it is when bulk holds that form, packed from the directory
otherwise, as *cnt_common_pack* does, inside *unshare --map-auto* since the
files belong to the subuids of whoever built it, or with *sudo* when the
host has no subuid line.

*DEMO_IMAGES* (*AUTOTEST_DEMO_IMAGES* replaces it) names the images of the
demos, so that *demo_test.py* runs nearly every demo in the kvm instead of
skipping it; one that bulk does not hold warns, and its demo skips. Left
out: *ebpf_trixie* (XDP asks for the initial user namespace),
*python_trixie.qcow2* (phy and realnet, a third level of kvm) and the
freebsd, windows and cisco disks.

Each kvm gets the source tree in */home/user/sources*, .git and the targz
stores left out, and those images in its bulk. Then kvm1 gets the
bundle installed with *install_cloonfs* and kvm2 the toy tarball in
*/home/user*, where *testitalltoy.sh* looks for it.


The run
=======

*autotest_run.sh*, written by the script into */home/user* of each kvm and
started there with *setsid -f* as *user*, so the session of the agent may
end, with *XDG_RUNTIME_DIR* and *DBUS_SESSION_BUS_ADDRESS* naming the user
bus, since a *su* opens no session. The tests stop a *sniff --count* or a
*trafgen* with SIGINT, and a signal ignored or blocked anywhere up the
chain is inherited by every process of the suite, which a shell cannot
undo: the runner is exec'd through *autotest_launch.py*, a python that
puts the dispositions and the mask back to default, and *setsid -f* rather
than *&*, since a job a non-interactive shell puts in the background gets
SIGINT ignored.

The runner empties */var/log/syslog*, runs the suite with
*TESTITALL_NO_ENDURANCE=1*, which stops *testitall.sh* after phase 8 with
*TESTITALL OK*, and writes, in */home/user*::

    results_outputs.log   SigIgn and SigBlk the suite runs with, the suite's output,
                          a summary at its end: verdict, duration, phases reached,
                          checks ok, demos skipped, the FAILED lines, KERR count;
                          every minute a time stamp line of *progress.py*: the clock,
                          the time elapsed against the expected total, the phase, the
                          demo that runs; under every phase title one line of the
                          machine: free memory, load, disk left
    kerr1_outputs.log     the KERR lines of /var/log/syslog during the suite, as they come;
                          in kvm2 also those of the two toys' own syslogs
    tst_max_output.log    when the suite passed, the endurance: tst_max.sh in kvm1,
                          tst_maxtoy.sh in kvm2, SOAK_MAX_LIVE=20 SOAK_MAX_LIVE_KVM=2
                          for the 4 GB of the kvm
    kerr2_outputs.log     the KERR lines during the endurance, syslog emptied
                          again between the two

*tst_maxtoy.sh* starts the toys with *testitalltoy.sh start* and points
*soak.py* at them with its *env*: the toy's client, and the work zones and
syslogs inside the rootfs of each net, where the soak reads what a dead
container left. When a suite fails, the logs the toy names (crun.log, the
init-starter log) and the last 200 lines of *kerr1_outputs.log* follow the
summary: a toy's proxymous logs to the machine's syslog, so only the merged
file shows it beside the two nets. The sizes of *tst_max.sh*
are those of the development host, 45 objects alive and 25 kvm of 1 GB;
the two exports size it for kvm1. *RAM* and *CPU* of the two machines are
at the top of *autotest.sh*.


The web of the nets under test
==============================

Once the two kvm answer, *web_lan* adds the tap *autotap*, cables it and
eth0 of each kvm to *autolan*, and gives the addresses::

    kvm1 eth0   10.9.9.1/24     https://10.9.9.1:4444 nemo, https://10.9.9.1:4454 mito
    kvm2 eth0   10.9.9.2/24     https://10.9.9.2:4444 nemo, https://10.9.9.2:4454 mito
    autotap     10.9.9.3/24     on the host, with sudo

user *nemo* password *nemoclown*, *mito* and *mitoclown*. No route is
added: a machine still has no internet, the suites run as before. A
machine built before the builder took the firewall out gets the same
*FIREWALL_OFF* there, and one that does not answer the ping of the host
has its eth0 and its rules printed. A failure in *web_lan* warns and the suites start all the same.

The machines are left running, pass or fail. To start again, run
*autotest.sh* again: it kills nemo and rebuilds the pair.
