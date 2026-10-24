==============
Writing a demo
==============

The rules every file of *demos/* follows. *debug/check_demos.py* enforces
the ones a source shows; *debug/demo_test.py* runs the demos.


The tree and the packages
=========================

*demos/<area>/<demo>.py*, one level, python only; *windows.py*, *freebsd.py*
and *cisco.py* at the top. A new demo goes into an existing area::

    cloonix       the objects cloonix provides
    switching     how a frame finds a port
    routing       how a packet finds a way
    address       who gets an address, what a name resolves to
    security      tunnels, filters, and who is let in
    services      the machines in the middle
    availability  staying up when one of them stops
    watching      reading what happened
    wifi          the air, before the ip stack

Two files are not demos. The first is *demos/packages*: the packages of the
chapter *Learning with cloonix* of the user book, number and title, each
followed by its demos with the line the chapter gives them::

    4 Routing
    routing/frrspider.py         sixty routers, OSPF
    routing/frrspider_bgp.py     the same sixty, BGP

*Play* in the Demo panel of the canvas fetches that file and shows it as it
is: the reader of the book finds on the canvas the packages of the book.
A new demo is added to a package in *learn.rst* and to the same place in
*demos/packages*; *check_demos.py* refuses a demo listed in neither or not
once, and any difference between the two files in packages, titles, demos,
order or lines.

The second is *demos/voice*: for each demo, the phrase the Demo panel says
when the demo starts, a title and the item it shows, written as it is to be
spoken::

    routing/frrspider.py         O S P F. Sixty routers learn their routes.

*debug/demo_voice.py* makes each phrase a clip, *audio/<area>/<demo>.ogg* of
jscloonix; the *sound off* button of the panel silences it, *sound on* says
the phrase again. *check_demos.py* refuses a demo without its line.

The docstring at the top of a demo holds the network as ascii art, the acts,
the image, the packet to catch. Outside *cloonix/*, a *HISTORY* constant
follows *ROOTFS_CHOICES* and is the first thing *main()* tells with
*story()*; the same paragraph heads the section of the demo in the chapter
of its area, *xdoc_user/doc/demos_<area>.rst*, and *check_demos.py* refuses
a difference.


The console and the history
===========================

A demo writes two things, to two places::

    the console   show(): the cloonix commands, one after the other. A
                  terminal, or the log of the Demo panel.
    the history   story(): HISTORY, the text of each step, what an act
                  proved. The Comments panel of the canvas only.

The Demo panel sets *CLOONIX_DEMO_STORY=1*; the demo then writes each line
of history with the mark *#story#* in front, the panel sends the marked
lines to the popup and the others to the log. From a terminal the variable
is not set and *story()* writes nothing. A refusal or a failure goes to
stderr and shows in both. *demo_test.py* sets the variable, and
*check_demos.py* refuses a *print()* outside *show()* and *pause()*.

What the popup makes of the history::

    step("...")          a new section, numbered and timed, a chip in the
                         table of contents; "act 2: ..." is shown as an act
    the lines after it   the text of the step, kept as written, ascii art
                         included: why the step, what was measured, what it
                         proves
    a cloonix command    a line that begins with "cloonix " is blue and a
                         click copies it
    "       . text"      a trace, dim
    show() lines         folded under the step that ran them
    stderr, pause()      repeated in the step, red or amber
    the docstring        the About view; a line in capitals is a heading

A result is shown: a ping is five pings, its last lines in the history, then
*hold()* and the pause *step()* takes, four seconds, none with
*CLOONIX_DEMO_FAST=1* and none on a terminal. A demo that leaves something to
try by hand calls *pause(what)*: it waits for Enter from a terminal or from
the Demo panel, and goes on at once from a script.


Start clean, draw itself
========================

After finding its image a demo calls *erase_net()*: *del all*, then the
topology read until it is empty. Then *place()*: a position for every item
and an angle for every interface, sent before the items exist, so the
topology appears as the drawing of the docstring. A demo never starts the
net; it checks the net answers and prints *cloonix start <name>* otherwise.


The commands are visible
========================

In the source a command is written whole, *cloonix cli* included::

    run("cloonix cli %s plug %s eth0 %s" % (NET, node, LAN))
    ssh(node, "ip addr add 10.0.10.1/24 dev eth0.10")

In the output every command prints itself as typed, alone on its line with
nothing in front, so any line of the console pastes into a terminal as it
stands::

    cloonix cli nemo add cnt a10 eth=1 trixie
    cloonix cli nemo ssh a10 "ip link add link eth0 name eth0.10 type vlan id 10"

Two commands do not print, the two that are polled; both are named in the
docstring of the function that polls them::

    cloonix cli <net> dump commands
    cloonix --wait 3 cli <net> ssh <node> "echo yes"

*run()* takes the whole line and splits it as a shell does, so the line
printed is the line executed. *ssh()* prints the line and builds the
argument vector from the string, its command being one argument holding
spaces and quotes. What the demos share is in *demos/lib/demolib.py*.


Images
======

Keys, certificates, zone files, rulesets and configurations are written over
*cloonix cli ssh* while the demo runs; an image holds none, so several demos
share one. Every image except *trixie_systemd* is built without systemd: a
running systemd takes several inotify instances, counted by the kernel per
user id. The images are built by *vm_create/cnt_kvm_demo/doitall.sh*, keyed
by the demo name::

    ./doitall.sh wg
    ./doitall.sh --all


The two checkers
================

::

    debug/check_demos.py                  static, under a second, no server
    debug/demo_test.py nemo               runs every demo and reports
    debug/demo_test.py nemo --list        what would run, what is skipped
    debug/demo_test.py nemo --only vlan,stp
    debug/demo_test.py nemo --repeat 3

A demo that cannot run here, its image absent, a module not loaded, a
privilege missing, stops with *unavailable()* of *demolib*: the message on
the error output and exit *EXIT_UNAVAILABLE* (3). Any other stop is a
failure. all.py stops on either.
*guarded(main, nodes)* runs the *main* of a demo: a failure prints its
reason and which of *nodes* are still up, then exits 1; *unavailable()*
goes through with its own exit.

In a toy, found by the client running the demo (*<toy>/bin* beside
*<toy>/rootfs*), *needs_subuid()* stops a demo whose daemon drops its
privileges when the toy has a single uid mapping, through *unavailable()*.
The demos a toy cannot play are not in it: *toy_targz/toy_drop* lists them,
for *create_toy_targz.sh* and for *demo_test.py*. *shared_dir(name)* gives
a directory the demo and the server both see: */tmp/<name>* for a bundle,
for a toy */var/lib/cloonix/bulk/<name>*, named */root/bulk_host/<name>* to
the server.

A demo that needs a kernel module the host must load names it in a
*HOST_MODULES* constant beside *ROOTFS_CHOICES* and calls
*need_host_modules()* of *demolib*, which stops it with the *modprobe* to
type. *demo_test.py* reads the constant, loads an absent module with
*modprobe* through passwordless *sudo* before the first demo, and reports
one that stays absent as SKIP with that *modprobe*, as it does for an
image. A demo that
works on a net run as root only says *NEEDS_ROOT = True*; *demo_test.py*
reports it as SKIP on a net whose *cloonix-main-server* runs as another
user.

*demo_test.py* reports a missing image as SKIP with its build command, runs
each demo under a timeout, waits for an empty net between two demos, and
prints the distinct KERR lines of syslog for each demo, passed or not.
