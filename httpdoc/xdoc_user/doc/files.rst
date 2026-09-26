============================
Two panes: cloonix cli files
============================

A file manager in the shape midnight commander kept: two directories side by
side, a copy goes from the active pane to the other, no path typed::

    cloonix cli nemo files cnt1           this machine left, a guest right
    cloonix cli nemo files cnt1 cnt2      a guest on each side

.. image:: /svg/files.svg
   :align: center
   :width: 100%

The guest is a **cnt**, a **kvm**, a windows or a freebsd guest. Nothing is
installed in it and nothing listens on any network there: the legs are the
ones *cloonix cli scp* uses, one per listing and per copy.


The keys
========

============  =====================================================
Key           What it does
============  =====================================================
Tab           the other pane becomes the active one
Up Down       move the cursor, also k and j
PgUp PgDn     a screenful; Home and End the ends of the list
Enter         open the directory under the cursor, also Right and l
Backspace     up one directory, also Left and h
F5 or c       copy what is selected into the other pane
F7 or m       make a directory in the active pane
F8 or d       delete what is selected, after a yes
F2 or r       read the directory again
F9 or g       type a path to go to
F1 or ?       the keys, in a box
F10 or q      leave
============  =====================================================

The letters exist because a terminal multiplexer, a browser terminal and a
remote desktop each eat a different F key. A directory copies whole with its
tree.


Two guests
==========

A copy between two guest panes is two transfers: down into a directory made
under */tmp* on this machine, up into the other guest, and the directory
removed. The progress line is the one *cloonix cli scp* writes::

    ClownStuff.tar  4.7 MB / 21.3 MB  22%  1.9 MB/s  2.5s


What it is made of
==================

The guest side of a pane is one opcode of the exec protocol, **LIST**: a
directory and none of its contents, answered the same way by a linux, a
windows and a freebsd guest. A guest whose insider disc predates the opcode
answers *unexpected first opcode 13*, and files says::

    this guest answers no LIST: its insider iso predates the files verb.
    Rebuild it (cmd_mk_iso) and restart the guest.

ssh and scp to that guest keep working.

The screen is *cloonix_files.py*, beside the client and imported by the
verb. A client copied alone does every other verb and says that
*cloonix_files.py* is missing. On windows and macos the same two files: no curses, the screen is
written with ansi escapes; a windows console shows the box characters with
code page 65001 (*chcp 65001* before starting), and ascii otherwise.


A windows guest on the right
============================

Files learns the kind of guest from the path LIST answers: a drive letter
means windows, and that pane uses backslashes::

    host: /home/you/work          |  Win1: C:\Users\Administrator

Backspace stops at the root of the drive; *g* then *D:\\* crosses to another
one. Delete and mkdir run in cmd.exe as *rd /s /q* and *md*.


What it does not do
===================

No multiple selection, no view, no edit, no rename, no move, no copy inside
one pane. *cloonix cli <net> ssh <vm>* is the answer for view and edit.
