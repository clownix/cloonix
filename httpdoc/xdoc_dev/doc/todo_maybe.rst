=========================================
Things set aside: systemd inside the toy
=========================================

One service was given up on purpose: **systemd as the first process of a
container, when that container runs inside the toy**. In the bundle it
works as before. This chapter says what the service is, why it went, and
which architecture would bring it back. It assumes nothing about cgroups;
the chapter *A starved proxymous* tells how the subject came up.


What the service is
===================

A cloonix container normally runs no init: *cloonixsbininit* starts the
agent and a syslog daemon, and that is all. One option asks for the init
of the image::

    cloonix cli nemo add cnt c1 eth=1 trixie_systemd startup_env="CLOONIX_WITH_SYSTEMD=1"
    cloonix cli nemo ssh c1 "cat /proc/1/comm"
    systemd

With it, *systemctl start <service>* works in the container as on a real
machine. In a toy the same command now gives::

    cloonix cli nemo ssh c1 "cat /proc/1/comm"
    cloonixsbininit
    cloonix cli nemo ssh c1 "cat /run/cloonix_no_systemd"
    CLOONIX_WITH_SYSTEMD=1 asked, but /sys/fs/cgroup is not writable here: systemd cannot run, started without it


Four notions, in plain words
============================

**A cgroup** is a folder the linux kernel keeps for a group of processes.
Every process is in exactly one. The folders are visible as directories
under */sys/fs/cgroup*, and they nest like directories do. The kernel uses
them to count and to share: so much memory for this group, so much cpu
for that one. *cat /proc/self/cgroup* prints the folder of the shell that
runs it.

**Writing in a cgroup** means creating sub-folders in it and moving
processes between them. The folders belong to users like files do. Almost
all of them belong to root.

**Delegation** is root handing one folder to a plain user: "this one is
yours, make what you want below it". On a machine with systemd, each
logged-in user has a small systemd of their own, the *user manager*, and
it can create such a folder on request. The request travels on the *user
bus*, a socket at */run/user/<uid>/bus*. A machine without systemd
(alpine with openrc) has no user manager, no user bus, and nobody to ask.

**systemd as pid 1 needs to write.** Its way of keeping track of services
is to make one sub-folder per service. With no folder it may write in, it
stops at boot and never answers.


Why it went
===========

The toy is cloonix run by a plain user, with no root at all. Until
september 2026 the toy asked the user manager for a delegated folder,
*crun-nemo.scope*, and lived in it. A container of the toy saw that
folder as its */sys/fs/cgroup*, could write in it, and systemd started.

The cost appeared under load on one kernel. **cloonix-proxymous**, the
half of the net that must stay on the machine, was not in that folder. The
kernel gave the cpus to the busy folder and left proxymous, alone in a
quiet one, without any for up to 35 seconds. A net that loses its
proxymous for 30 seconds destroys itself. Keeping the whole net in one
folder removed the problem, and the only folder available to everybody
without asking is the one the shell is already in, which belongs to root:
nothing can be written there, so systemd in a container of the toy went.

It can still be asked for, on a host where it matters more than the risk::

    CLOONIX_TOY_CGROUPM=systemd ./nemocmd start


The architecture that would bring it back
=========================================

Both wishes hold together if **one delegated folder contains the whole
toy, proxymous included**: one folder, so the kernel shares between
processes and starves none; delegated, so systemd in a container can
write.

.. image:: /svg/cgroup_scope.svg
   :align: center
   :width: 100%

The figure reads in three steps.

  1. *nemocmd start* runs a small new binary first, called here
     **toyscope**, shipped in the toy like the rest.
  2. toyscope speaks to the user manager on the user bus and asks for
     a new delegated folder, *toy-nemo.scope*, with itself as the first
     member. This is what the host command *systemd-run --user --scope -p
     Delegate=yes* does; cloonix does not call host commands, so the
     request is made by toyscope itself.
  3. The user manager creates the folder and moves toyscope in.

From then on everything is inheritance: a process is born in the folder of
its parent. toyscope starts proxymous, then crun with
*--cgroup-manager=disabled*, so that crun asks for nothing and stays
where it is. The servers, the switch, the guests and proxymous are all in
*toy-nemo.scope*. A container of the toy sees that folder as its
*/sys/fs/cgroup*, the *mkdir* probe of *cloonixsbininit* succeeds, and
systemd starts.

Where there is no user bus, toyscope starts the same two things
without asking anybody, the probe fails, and the container comes up
without systemd with its reason written, exactly as today. The behaviour
then differs between machines only where the machines differ.


What exists, what is to write, what is to verify
================================================

Exists:

  * *libsystemd* is already in cloonfs: crun is built with
    *--enable-systemd* and needs it (*build_tools/build_cloonfs*,
    *libsystemd-dev*). Its *sd-bus* functions are the bus client.
  * *setup.sh* already starts proxymous and crun in that order and already
    has the *disabled* path.
  * The probe and the fallback in *cloonixsbininit*, and the sub test that
    reads */run/cloonix_no_systemd*.

To write:

  * **toyscope**, about a hundred lines of C: open the user bus, call
    *StartTransientUnit* of *org.freedesktop.systemd1.Manager* with the
    properties *PIDs* (its own pid) and *Delegate* (true), wait for the job
    to end, then *exec* what it was given. No bus: *exec* at once.
  * Its place in *toy_targz/create_toy_targz.sh* and one line of
    *setup.sh*.
  * The stop: a scope ends when its last process ends, nothing to add.

To verify on real machines before relying on it:

  * **That the move is allowed.** The shell is in a folder of root
    (*session-<n>.scope*); moving a process from there into a folder of the
    user crosses a border only root may cross. *systemd-run --user
    --scope* succeeds from such a shell on current systemd, the user
    manager asking the system manager for the move. To check on each of
    the six systemd machines::

        systemd-run --user --scope -p Delegate=yes cat /proc/self/cgroup

    The answer must name a folder under *user@<uid>.service*.
  * **That systemd in the container accepts a folder that already holds
    processes.** The kernel refuses to give sub-folders their own cpu or
    memory accounting while their parent holds processes directly. The
    former layout had the same shape (the servers and the container shared
    *crun-nemo.scope/container*) and systemd reached *running* or
    *degraded* there; the sub test *sub_systemd_image.py* is the check.
  * **That the storm still passes on rocky**, with proxymous inside: the
    stall lines of the chapter *A starved proxymous* give the answer in
    one run.

Until those three are seen, the present choice stands: no cgroup of its
own for the toy, the same everywhere.
