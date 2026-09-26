The cloonix client on windows 10 and windows 11
===============================================

What this is
------------
The cloonix command line client, which is python source and nothing else,
plus the demos it can play. There is no server here: a cloonix server is
linux, and everything below drives one across the network, over the single
tcp port of the net.

What it needs
-------------
Python 3.7 or later, and nothing else. No package is installed, nothing is
compiled, nothing is downloaded. If there is no python on the machine, take
one from

    https://www.python.org/downloads/windows/

and tick "Add python.exe to PATH" while installing it. The python of the
microsoft store works too.

Installing
----------
Extract this zip anywhere and run

install.bat

by double clicking it or from a cmd window. It needs no administrator: the
files go under your own AppData, and only your own environment is changed.

What it does:

  - checks that this is windows 10 or windows 11
  - finds every python on the machine, asks each one its version, and takes
    the newest that is 3.7 or later
  - copies the client and the demos to
    %LOCALAPPDATA%\Programs\cloonix
  - writes cloonix.cmd there, so "cloonix" is a command in any new terminal
  - adds that directory to your PATH, and sets CLOONIX_CLIENT and
    CLOONIX_DEMO_DIR
  - adds a start menu entry and an entry in Add or remove programs

Using it
--------
Open a NEW terminal -- the one the installer ran in still has the
environment from before -- and:

cloonix list
cloonix cli nemo demo
cloonix cli nemo demo cloonix/a2b.py
cloonix cli nemo ssh cnt1
cloonix cli nemo files cnt1

list gives every net of the configuration, this client's version and the
version of each net that answers; demo alone lists the demos by name and
plays one when named; ssh opens a root shell inside a guest; files shows
two panes, this machine and the guest.

The nets, their ports and their passwords are in cloonix_config.py, beside
the client in the install directory. It is the configuration of the cloonix
that made this package; if your server has another one, copy its
cloonix_config.py over that file.

The demos
---------
A demo is a python programme that drives the server with the same client
you type. All of them run here except cisco, which types at three serial
consoles through a pty and needs a linux or a macos machine.

Removing it
-----------
Three ways, all of them the same thing:

  - Settings, Apps, "Cloonix client", Uninstall
  - uninstall.cmd in the install directory
  - powershell -File uninstall.ps1

It lists what is about to go and asks once, then takes back the whole lot:
the install directory, the PATH entry, CLOONIX_CLIENT, CLOONIX_DEMO_DIR,
the start menu entry and the entry in Add or remove programs. Nothing else
is touched -- your python is left alone, and no directory outside the
install one is looked at.

The install directory goes whole, so a demo of your own saved under demos\
goes with it. Copy it out first.

A terminal that was already open keeps the old PATH until it is closed.
