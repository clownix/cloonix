==============
The web canvas
==============

The graphical interface is a web page served by cloonix itself: canvas 2d,
websockets, xterm.js, noVNC, webshark. Nothing to install, nothing to
forward: *https://127.0.0.1:4444* for the net nemo on the machine running
it, *https://<ip>:4444* from anywhere. The port is the *cloonix_web* keyword
of the net in *cloonix.cfg*; a net without it runs headless.

The page asks for a user and a password, not secret: they are shipped in the
configuration, where they can be changed::

    https://127.0.0.1:4444      user  nemo      password  nemoclown

    the file, bundle      /usr/libexec/cloonix/cloonfs/etc/cloonix.cfg
    the file, toy         <toy directory>/rootfs/cloonix/cloonfs/etc/cloonix.cfg

The user is the name of the net, the password its *cloonix_passwd* in that
file, the net name followed by *clown*: user *nemo*, password *nemoclown*.


The port
========

  * https only, on every address. The browser asks for http basic
    credentials: the net name and its *cloonix_passwd*. Basic is the scheme
    a browser applies by itself to a websocket handshake, so the console
    panels carry the credentials without a token in any url.
  * TLS ends in proxymous, with the same certificate as the doors port. The
    gui server listens on one unix socket, *proxy_pjsweb.sock*, and sees
    plaintext.
  * The console panels need *crypto.subtle*, which a browser grants to a
    secure context alone; https is one from any address.
  * The certificate is self-signed, made once at packing time, shipped in
    the tarball and installed to */usr/share/cloonix/cert* (the *config*
    directory of a toy). A net that finds none makes its own at its first
    start, with the libcrypto of cloonfs, into the first writable of
    *CLOONIX_TLS_DIR*, the toy's *config/*, */usr/share/cloonix/cert* and
    */var/lib/cloonix/<net>/cert*; syslog names the file. The browser warns
    once. Replace it on anything but a private lab machine, or point
    *CLOONIX_TLS_DIR* at a directory holding *cloonix_cert.pem* and
    *cloonix_key.pem*.

A browser that refuses the certificate leaves this in syslog::

    ERROR TLS SSL_accept: ... ssl/tls alert bad certificate

The browser holds an exception for an older certificate of that address. In
firefox: Settings, Privacy, Certificates, View Certificates, Servers, delete
the entry, reload, accept once.


From a phone or a tablet
========================

The browser of an Android phone or tablet on the network of the host opens
the same page::

    https://<ip>:4444      user nemo, password nemoclown
    ip -4 addr             run on the host: its address

The warning of the self-signed certificate is accepted once, as on a
desktop. The canvas is driven by mouse events: with a USB or Bluetooth mouse
on the phone or tablet it works as on a desktop; by touch alone a tap
clicks, and items cannot be dragged.


The canvas
==========

.. image:: /svg/canvas.svg
   :align: center
   :width: 100%

The topology is drawn live. A machine's colour follows its state: dark red
down, bright red booting, blue up with its agent answering. A force-directed
layout keeps placing the nodes; *Lock into place* in the menu of a node pins
it, as *cnf <n> x= y=* and *cnf <n> eth<i> angle=* do from the command line.
The server holds a pinned place while the net has objects, so a reload or a
second browser draws the items where they were put; *del all* forgets the
places, and the next topology is framed to the window as the first was.

A note, set by *Note...* in the menu of an item or lan or by *cnf <n>
note="text"*, is written whole in amber under the item, in lines of 28
characters, and kept by the server in the same way.


The menus
=========

Every menu opens on a right-click.

On the empty canvas::

    <rootfs currently selected>    create a machine with it
    List Other Choices             the images found in bulk
    kvm config / cnt config        the kind and its options
    lan  tap  nat  a2b  c2c        the other items
    phy ▶                          one line per physical interface of the server, absorbed or macvlan

On every item and every lan, under *Lock into place*: *Note...*, a comment
on it, empty to remove it.

On a kvm::

    Desktop   Console   Save   Reboot   Delete   Info

On a cnt::

    remote console   info   delete

On an interface dot::

    PlotTraffic <n> eth<i>
    Sniff on / Sniff off           live, without restarting the guest

A tap, a phy or a c2c has the same dot and toggle; born without capture
capability, its dot is cyan until *Sniff on*, which also opens the webshark
of the interface once the dot turns green; a double-click on the green dot
opens it after that.

On a lan: *Delete LAN*. On a cable: *Delete link*. On an a2b: *Tune a2b*.
On an air: *Select*, after which a click on a wlan plugs or unplugs it, and
its capture. An air is born not captured, drawn dark brown; *Sniff on*
captures it, turns it bright orange with a green ring, the colours of a
sniffable interface, and opens its webshark. A captured air offers *Sniff
off* and *Webshark*, and a double-click on it opens its webshark. A wlan
dot is captured with its air: same three choices, its double-click opens
the webshark of its air, and it has the green ring while the air is
captured. A blue capture line of the Comments panel arms a capture not
armed yet, as the *sniff* verb does, and opens the webshark once it is.


Cabling
=======

::

    double-click a lan          it turns purple, selected
    single-click a port         the cable is drawn
    double-click a machine      root shell in a panel
    double-click a green port   webshark on that interface


The panels
==========

Consoles, desktops, webshark, plots and dialogs open as floating panels.
The title bar closes a panel or moves it to its own browser tab with its
live connection: a console keeps its scrollback, a capture keeps running.
A panel is dragged by its title bar and resized from its borders and
corners, a console fitting its rows and columns to the new size. Behind the
drawing, a panel resizes the same way where its border shows with no item
over it: the cursor turns to the resize arrow, and the first click brings
it to the front.

::

    Console      xterm.js on a root shell, the network of the guest not involved
    Desktop      noVNC on the graphical output of a kvm; no usb redirection;
                 the C-A-D button at the top right sends Ctrl+Alt+Del
    webshark     the packet list and the detail tree; can stop and save a .pcap
    PlotTraffic  live throughput of one interface, tx and rx
    Tune         the sliders of the layout engine
    Tune a2b     loss, delay, rate per direction, applied live


The Demo panel and its Comments panel
=====================================

The *Demo* button of the header opens the Demo panel. *Play* opens a picker
with the packages of the chapter *Learning with cloonix*, 0 to 11, their
demos in the order and with the line of that chapter, the file
*/usr/share/cloonix/demos/packages*; a click plays a demo. With the panel
open, *Demo* brings it to the front with the picker; during a demo the
picker is only to read, *Close* ending it while the run goes on. During a
demo the canvas fits its drawing to the low edge of the view, under the two
panels, a taller drawing continuing behind them:

.. image:: /svg/demo_history.svg
   :align: center
   :width: 100%

**The log**, in the Demo panel, receives the cloonix commands of the demo
and nothing else, one after the other, each as it can be typed in a shell.
It is what the same demo prints when it is played from a terminal.

**The Comments panel** opens beside it, titled *Comments -- <demo>*, and
explains the demo. It has two views:

*Steps* is written while the demo runs. Its top part stays in view: the
demo, *7/69 cloonix/snf.py* under all.py, and its one-line description, the
first line of its docstring. Under it the steps scroll, one section per step,
the scroll bar giving back the earlier ones.

* The first section, *i*, holds what the demo says before its first step:
  for a demo about a protocol its history, where it comes from and who wrote
  the software under study, and the drawing of the network.
* Each following section is a step of the demo: its number, its title, the
  time into the run at which it started, and under it what the demo has to
  say, why the step is there, what was measured or captured, what it proves.
  A step that is an act of the demo has *ACT n* in front of its title.
* Under the text of a step, a folded line, *n cloonix commands, as typed in
  a shell*, holds the commands the step gave the server, the same lines as
  the log. Unfolded, a step reads as a worked example: the goal, the commands,
  the result.
* A line in blue is a command the demo suggests to try. A *terminal of
  <node>* line opens a terminal of that node and runs the command in it,
  the command also put in the clipboard; a *capture of* line opens the
  capture of that cable; any other cloonix line is copied, ready to paste
  in a shell where the client is. The demos end with several.
* A line in amber is something the demo asks for, the Enter it waits for at
  a published port for instance, which is typed in the log. A line in red is
  what the demo said on its error output.
* The row of chips under the tabs is the table of contents: one chip per
  step, amber while it runs, green once done, red if the demo failed there;
  a click goes to the step. The status at the right of the tabs gives the
  step in progress and the time elapsed, then *done* or *failed*.
* The view follows the run; scrolling up to read stops that, going back to
  the bottom resumes it.
* The last box says how the demo ended. A demo leaves its network running,
  so what it explained can be tried at once.

*About* is known before the demo runs: the docstring at the top of its file.
It holds the network as ascii art, the acts, the image the demo needs, its
size, and *the packet to catch*, the sniff commands that put the
subject of the demo on the screen, packet by packet.

The mouse on the chip or the title of a step outlines on the canvas, with a
pulsing ring, the items that step's commands named. A blue capture line
opens webshark already filtered: its tcpdump filter is translated into the
display filter webshark reads (*icmp and host 1.1.1.2* becomes *icmp &&
ip.addr == 1.1.1.2*); a *portrange* is not translated and the capture then
opens unfiltered. *Save*, beside the tabs, downloads the comments as a
markdown file: About, every step with its text and its commands, and how
the demo ended.

The panel can be moved, resized and closed like any panel. The button of
the Demo panel names what a click does: *Comments on* opens it again with
everything the current demo said, *Comments off* closes it. Once closed,
by that button or by its own close, the next Play leaves it closed and
keeps the comments for a *Comments on*; the next Play starts new comments.

At its start a demo says in one sentence what it shows, the line of
*demos/voice*. When a demo stops on a refusal or a failure, the voice
says *The demo stopped. Here is why.* and the browser reads the first line
of the reason, when it has an english voice. *sound off* silences the
voice, *sound on* says the sentence again. *stop and del* kills every demo going on and erases the net; the
Demo panel stays for the next Play.

The history exists on the canvas only. Played from a shell, a demo prints
its commands and its failures::

    cloonix cli nemo demo security/wg.py

and a reader who wants the text there all the same sets
*CLOONIX_DEMO_STORY=1*, each line of the history then arriving with
*#story#* in front of it.


The commands of a topology
--------------------------

*Commands* prints in the log of the panel the commands that rebuild the net
as it is, those of *dump commands*: the items, their lans and airs, the
captures, the settings and the places. Nothing is written: they are the start of a demo of one's own,
a python programme written by hand.


Where a demo runs
-----------------

A demo played from the panel runs where the **server** is: the gui server
reads the file from its own disk and hands the text to proxymous, which runs
it on the host. What the browser sent is only the name. A key typed in the
log of the panel reaches the demo as a line on its stdin: a demo that stops
to let a page be opened, *cloonix/nginx.py* and *cloonix/nat.py* at their
published port, waits for it, and goes on when Enter is typed in the panel
or in the terminal the demo runs from. Played from a shell, the same file
runs on your machine.

*/usr/share/cloonix/demos* is sticky and shared by every user of the
machine; the shipped demos belong to root, a saved one belongs to its author.


Header buttons
==============

::

    Delete all    asks, kills every demo going on, then del all; closes every panel, the demo player and PcapRec included
    PcapRec       records every capturable interface at once, one .pcap each, into /usr/share/cloonix/pcap
    Demo          the picker above

PcapRec and a webshark on the same interface read the one dumpcap of that
interface, so both can run at once; what PcapRec holds is the capture
capability, so *Sniff off* on a recorded interface is refused until the
record stops.


webshark
========

One capture per interface, one dumpcap per capture started with the
interface and shared by webshark and PcapRec. The panel shows the rows as
they are dissected, 256 to a message, and keeps the last 20000 drawn; the
server keeps the last 500000 frames and the bytes of the last 200000, and
stops the capture at that ceiling with a red line saying so. Closing the
panel stops the process; a double-click on the same dot starts a fresh one.

The filter box takes a wireshark display filter, applied in the server
before the rows cross the network::

    icmp
    tcp.port == 80
    ip.addr == 192.168.1.1
    ip.src == 10.0.0.0/24 && !dns

A viewer that reads slower than the capture is told so in the bar under
the filter and loses rows of its own only; the capture and the other
viewers of the interface are not held back. *Stop* freezes the list,
*Save* writes it as a .pcap, on the machine of the server at the path
asked, or as a download of the browser.


Overlays
========

::

    UNABLE TO CONNECT    the page lost the gui server: socket closed, or four seconds without the beat
    NET IS DOWN          the page is served but the net is not running; it reconnects on its own
    RELOAD THIS PAGE     the build stamp of the server changed under an open page; ctrl-shift-R

The drawing and every panel are emptied on a disconnect; the positions
dragged by hand are kept. The header shows the build stamp of the scripts,
*js Aug 2 2026 16:27:18*, stamped by *jscloonix/cmd*.
