#!/usr/bin/env python3
# cloonix_completion.py -- the words cloonix accepts, and who may ask for
# them.
import os
import shlex
import sys
import threading
import time

# The client module, handed over by the loader: resolve() and Doorways live
# there and are not duplicated here.
CL = None

# The verbs live in the entrance, beside the help that prints them, and are
# read from there through CL so there is one list and not two.

COMPLETE_DUMP = {
    "config":   "dump config",
    "topo":     "the topo in three lines per object",
    "json":     "the whole topo as json, every field of it",
    "drawing":  "the topo drawn",
    "commands": "the commands that would rebuild the net",
    "pid":    "the pid of every process",
    "images": "what bulk holds, cnt directories and qcow2, with owners",
    "ip":     "<nat> <guest>: the address the nat gave the guest",
    "namespaces": "every namespace of the net, and how to enter each",
}

# The ovs dumps, offered beside the six above but taking a subcommand of their own.
COMPLETE_DUMP_OVS = {
    "ovs-vsctl":  "the ovs config db of this net",
}

# A literal like COMPLETE_DUMP: the entrance carries its own copy and runs without this file.
COMPLETE_TRACE = {
    "qemu": "the command line of every kvm created from now on",
    "json_cli": "every json message in and out of the server, from now on",
    "crun": "the command line of every container created from now on",
    "nspace": "the namespaces every container gets and gives up",
}

COMPLETE_TRACE_PROBE = {
    "probe": "where the pipe into a guest breaks: tcp, hmac, leg, agent, "
             "far end",
}

# The kinds of add/del/cnf and the two phy kinds live in the entrance beside the verbs.

# the objects that have interfaces
ITEM_KINDS = ("kvm", "cnt", "tap", "nat", "a2b", "c2c", "phy")

COMPLETE_SNIFF = {"sniff=on": "capturable", "sniff=off": "not capturable"}
COMPLETE_ANGLE = {"angle=90": "its place around the item, 0 top, 90 right"}
COMPLETE_PLACE = {"x=300": "then y=, the item on the canvas, pinned there",
                  "note=gateway": "a comment shown on the canvas, quoted for "
                                  "spaces, note= removes it"}

# cnf <name>, what comes next, by the kind of the name: an interface, a
# direction or a key; no two kinds share a key.
COMPLETE_CNF = {
    "kvm":  {"eth0": "an interface, then sniff= or angle="},
    "cnt":  {"eth0": "an interface, then sniff= or angle=",
             "cpu=50": "percent of one core, 0 for no cap",
             "ram=200": "megabytes, 0 for no cap"},
    "tap":  dict(COMPLETE_SNIFF, **COMPLETE_ANGLE),
    "phy":  dict(COMPLETE_SNIFF, **COMPLETE_ANGLE),
    "nat":  dict(COMPLETE_SNIFF, **dict(COMPLETE_ANGLE, **{
             "publish=8080:80": "hostport:guestport[/udp], or none"})),
    "c2c":  dict(COMPLETE_SNIFF, **dict(COMPLETE_ANGLE, **{
             "mac_mangle=": "rewrite the source mac of what crosses"})),
    "a2b":  {"A": "side A, then sniff=on|off",
             "B": "side B, then sniff=on|off",
             "A2B": "the a to b direction, then a setting",
             "B2A": "the b to a direction, then a setting"},
    "lan":  {"rstp=on": "let the bridge run rapid spanning tree",
             "rstp=off": "stop it running rstp"},
    "air":  COMPLETE_SNIFF,
}

COMPLETE_A2B_CMD = {
    "loss=100":         "packets dropped in 1/10000, 100 is 1%",
    "delay=50":         "milliseconds added",
    "rate=1000":        "kilobytes per second",
    "silentms=200":     "bursts: closed n ms, open n ms",
    "qsize=":           "queue size (rate sets it by itself)",
    "bsize=":           "burst size (rate sets it by itself)",
    "replace_ipsrc":    "A.B.C.D with A.B.C.D",
    "replace_ipdst":    "A.B.C.D with A.B.C.D",
    "unreplace_ipsrc":  "stop replacing the source",
    "unreplace_ipdst":  "stop replacing the destination",
}

# The options each kind takes after its name.
COMPLETE_OPTS = {
    "kvm": {
        "ram=":            "memory in megabytes, or 2G",
        "cpu=":            "number of processors",
        "eth=":            "interfaces, born not sniffed: sniff=on sniffs one",
        "--persistent":    "the writes survive the shutdown",
        "--uefi":          "boot through OVMF, for a gpt disk",
        "--no_qemu_ga":    "the guest has no qemu-guest-agent",
        "--fullvirt":      "full virtualisation, rootfs on the ide bus",
        "added_cdrom=":  "add a second cdrom",
        "added_disk=":   "add a second disk",
        "install_cdrom=": "boot on an installation iso",
        "--no_reboot":     "do not reboot at the end of an install",
        "mac_addr=":     "eth<n>:<6 bytes>, instead of a random one",
        "wlan=":           "number of emulated wlan interfaces",
    },
    # windows and --uefi are what "add win" is, so they are not offered here.
    "win": {
        "ram=":            "memory in megabytes, or 2G",
        "cpu=":            "number of processors",
        "eth=":            "interfaces, born not sniffed: sniff=on sniffs one",
        "--persistent":    "the writes survive the shutdown",
        "--no_qemu_ga":    "the guest has no qemu-guest-agent",
        "added_cdrom=":  "a disc instead of virtio-win.iso",
        "added_disk=":   "add a second disk",
        "install_cdrom=": "boot on an installation iso",
        "--no_reboot":     "do not reboot at the end of an install",
        "mac_addr=":     "eth<n>:<6 bytes>, instead of a random one",
    },
    # freebsd is what "add freebsd" is, so it is not offered here.
    "freebsd": {
        "ram=":            "memory in megabytes, or 2G",
        "cpu=":            "number of processors",
        "eth=":            "interfaces, born not sniffed: sniff=on sniffs one",
        "--persistent":    "the writes survive the shutdown",
        "--uefi":          "boot through OVMF, for an efi-only disk",
        "--no_qemu_ga":    "the guest has no qemu-guest-agent",
        "added_cdrom=":  "add a second cdrom",
        "added_disk=":   "add a second disk",
        "install_cdrom=": "boot on an installation iso",
        "--no_reboot":     "do not reboot at the end of an install",
        "mac_addr=":     "eth<n>:<6 bytes>, instead of a random one",
    },
    # no_qemu_ga is what "add cisco" is, so it is not offered here.
    "cisco": {
        "ram=":            "memory in megabytes, 4000 and up",
        "cpu=":            "number of processors",
        "eth=":            "interfaces, born not sniffed: sniff=on sniffs one",
        "--persistent":    "the writes survive the shutdown",
        "added_cdrom=":  "add a second cdrom",
        "added_disk=":   "add a second disk",
        "mac_addr=":     "eth<n>:<6 bytes>, instead of a random one",
    },
    "cnt": {
        "eth=":           "interfaces, born not sniffed: sniff=on sniffs one",
        "--persistent":   "the writes survive the stop",
        "startup_env=": "VAR=value for the init",
        # Offered apart: the one value with a cost (a real init per cnt).
        "startup_env=CLOONIX_WITH_SYSTEMD=1":
                          "run a real init: ~5 inotify instances each, "
                          "128 per user by default, so two dozen at most",
        "vmount=":      "an extra mount inside the container",
        "wlan=":          "number of emulated wlan interfaces",
        "mac_addr=":    "eth<n>:<6 bytes>, instead of a random one",
    },
}

COMPLETE_USAGE = [
    (["cli", "*", "plug"],
     "<name> [ethN|wlanN|A|B] <lan>    an air for a wlan; tap, nat, c2c, "
     "phy: no interface",
     ["cnt1", "eth0", "lan1"]),
    (["cli", "*", "unplug"],
     "<name> [ethN|wlanN|A|B] [lan]    the lan it is on when not given",
     ["cnt1", "eth0", "lan1"]),
    (["cli", "*", "add", "kvm"],
     "<name> [ram=N|NG] [cpu=N] [eth=N] [--persistent] "
     "[--uefi] [--no_qemu_ga] [--fullvirt] [--no_reboot] "
     "[install_cdrom=iso] [added_cdrom=path] "
     "[added_disk=path] [mac_addr=ethN:xx:xx:xx:xx:xx:xx] [wlan=N] "
     "<rootfs>",
     ["kvm1", "ram=2000", "cpu=2", "eth=1", "trixie.qcow2"]),
    (["cli", "*", "add", "win"],
     "<name> [ram=N|NG] [cpu=N] [eth=N] [--persistent] [--no_qemu_ga] "
     "[--no_reboot] [install_cdrom=iso] [added_cdrom=disc] "
     "[added_disk=path] [mac_addr=ethN:xx:xx:xx:xx:xx:xx] "
     "<rootfs.qcow2>",
     ["Win1", "ram=4000", "cpu=2", "eth=1", "win.qcow2"]),
    (["cli", "*", "add", "freebsd"],
     "<n> [ram=N|NG] [cpu=N] [eth=N] [--persistent] [--uefi] "
     "[--no_qemu_ga] [--no_reboot] [install_cdrom=iso] [added_cdrom=path] "
     "[added_disk=path] [mac_addr=ethN:xx:xx:xx:xx:xx:xx] "
     "<rootfs.qcow2>",
     ["bsd1", "ram=2000", "cpu=2", "eth=1", "freebsd.qcow2"]),
    (["cli", "*", "add", "cisco"],
     "<n> [ram=N|NG] [cpu=N] [eth=N] [--persistent] "
     "[added_cdrom=path] [added_disk=path] "
     "[mac_addr=ethN:xx:xx:xx:xx:xx:xx] <rootfs.qcow2>",
     ["cisco1", "ram=6000", "cpu=4", "eth=3", "c8000v.qcow2"]),
    (["cli", "*", "add", "cnt"],
     "<name> [eth=N] [--persistent] "
     "[startup_env=VAR=val] [vmount=spec] [wlan=N] "
     "[mac_addr=ethN:xx:xx:xx:xx:xx:xx] <directory>",
     ["cnt1", "eth=1", "trixie"]),
    (["cli", "*", "add", "tap"], "<name>", ["tap1"]),
    (["cli", "*", "add", "nat"],
     "<name> [publish=hostport:guestport[/tcp|/udp]]", ["nat1"]),
    (["cli", "*", "add", "a2b"], "<name>", ["a2b1"]),
    (["cli", "*", "add", "c2c"], "<name> <distant_net>", ["c2c1"]),
    (["cli", "*", "add", "phy"],
     "<host_interface> [--macvlan]    absorbed by default; tab lists them",
     ["eth0", "--macvlan"]),
    (["cli", "*", "add"],
     "<kvm|win|freebsd|cisco|cnt|tap|nat|a2b|c2c|phy> ...", []),
    (["cli", "*", "del"],      "<name>    or all", ["cnt1"]),
    (["cli", "*", "cnf"],
     "<name> [ethN|A|B|A2B|B2A] <key=value>    sniff= x= y= angle= note= "
     "publish= cpu= ram= rstp= mac_mangle= delay= loss= rate= silentms=",
     ["cnt1", "eth0", "sniff=on"]),
    (["cli", "*", "dump", "ip"], "<nat> <guest>", ["nat1", "vm1"]),
    (["cli", "*", "sniff"],
     "<name> [ethN|A|B] [--dump|--bytes|--line|--count] [filter]",
     ["cnt1", "eth0", "--dump"]),
    (["cli", "*", "save"],     "<name> <path>", ["kvm1", "/tmp/sav"]),
    (["cli", "*", "rpc"],      "'<json>'    one json_interface.c message, verbatim",
     ['{"fctident":"list_pid_req","tid":0}']),
    (["cli", "*", "scp"],  "<guest>:<path> <local>   or the other way round",
     ["cnt1:/root/file", "."]),
    (["cli", "*", "ssh"],  "<guest> [command]", ["cnt1", "ip -br addr"]),
    (["cli", "*", "files"],
     "<guest> [<guest>]   two panes, copy between",
     ["cnt1", "cnt2"]),
    (["cli", "*", "demo"], "[<area>/<demo>.py]    none: the list",
     ["routing/frrmix.py"]),
    (["start"], "<net>", []),
    (["stop"], "<net>", []),
]

COMPLETE_USAGE += [(["cli", "*", "dump", w], "is the whole command", [])
                   for w in COMPLETE_DUMP]
COMPLETE_USAGE += [(["cli", "*", "trace", w],
                    "is the whole command, it waits until ctrl-c", [])
                   for w in COMPLETE_TRACE]
# The other two kinds of dump want a subcommand, so they are not whole
# commands and get an entry each.
COMPLETE_USAGE += [(["cli", "*", "trace", "probe"],
                    "<guest>    where the pipe into it breaks", ["cnt1"])]
COMPLETE_USAGE += [(["cli", "*", "dump", tool], "<subcommand> [args]",
                    [ex, arg])
                   for tool, ex, arg in (("ovs-vsctl", "list-br", "Bridge"),)]
COMPLETE_USAGE += [
    (["cli", "*", "dump"],
     "<%s>" % "|".join(list(COMPLETE_DUMP) + list(COMPLETE_DUMP_OVS)), [""]),
    (["cli", "*", "trace"],
     "<%s>" % "|".join(list(COMPLETE_TRACE) + list(COMPLETE_TRACE_PROBE)),
     [""]),
    (["list"], "[seconds]    every net asked at once, 5 s by default; "
               "0 asks nothing", ["5"]),
]

def required_places(syntax):
    """The <...> tokens after the pattern, in order."""
    return [t for t in syntax.split() if "<" in t and not t.startswith("[")]

def places_required(syntax):
    """How many words the syntax still wants after its pattern."""
    return len(required_places(syntax))

def choices(token):
    """The words a <a|b|c> place takes, or [] where it takes anything."""
    return token.strip("<>").split("|") if "|" in token else []

def places_repeat(syntax):
    """True when optional words sit BETWEEN two required ones -- add kvm,
    add win, add freebsd, add cisco, add cnt, where any number of options
    come before the image. A word that is an option or a key=value fills no
    required place there."""
    toks = syntax.split()
    req = [i for i, t in enumerate(toks) if "<" in t and not t.startswith("[")]
    return bool(req) and any(t.startswith("[") for t in toks[req[0]:req[-1]])

def line_is_command(client, words):
    """True when the line can be run as it stands.

    False is a word missing or a word wrong: an error from a script, a question
    from a terminal, guide(). Called before guide() and run(); the demo test
    reaches load_sibling through it.
    """
    global CL
    CL = client
    if len(words) > 2 and words[0] == "cli" and words[2] == "demo":
        return demo_is_named(words)
    # cnf is a command once a setting follows the name
    if len(words) > 2 and words[0] == "cli" and words[2] == "cnf":
        return len(words) > 4 and any(
            "=" in w or w.startswith(("replace_", "unreplace_"))
            for w in words[4:])
    entry = complete_entry(words)
    if entry is None:
        return False
    rest = words[len(entry[0]):]
    if places_repeat(entry[1]):
        rest = [w for w in rest if not w.startswith("-") and "=" not in w]
    if len(rest) < places_required(entry[1]):
        return False
    return all(word in choices(place)
               for place, word in zip(required_places(entry[1]), rest)
               if choices(place))

def trim_to_menu(client, words):
    """The line back to the first word its place does not take.

    The whole tail after that word goes, typed for a command that does not
    exist; the place left is the one the guide asks again, its menu under the
    prompt. Only a menu of this client counts: a net or a guest missing from a
    server's list proves nothing, a net silent for 0.7 s or a guest made a
    second ago, so <net> and <guest> are never trimmed. The demos are read off
    this disk, demo_table, so a demo name that reaches nothing is trimmed word
    by word.
    """
    global CL
    CL = client
    # The two menus the entrance owns, which are in no syntax string.
    if words and words[0] not in CL.VERBS:
        return []
    if len(words) > 2 and words[0] == "cli" and words[2] not in CL.CLI_VERBS:
        return words[:2]
    if len(words) > 3 and words[0] == "cli" and words[2] == "demo":
        return words[:3] + demo_kept(words[3:])
    entry = complete_entry(words)
    if entry:
        rest = words[len(entry[0]):]
        for i, place in enumerate(required_places(entry[1])):
            if i < len(rest) and choices(place) and rest[i] not in choices(place):
                return words[:len(entry[0]) + i]
    return words

def complete_entry(words):
    """The COMPLETE_USAGE entry matching the words, or None."""
    for entry in COMPLETE_USAGE:
        pattern = entry[0]
        if len(words) < len(pattern):
            continue
        if all(w == "*" or w == g for w, g in zip(pattern, words)):
            return entry
    return None

def demo_usage(words):
    """The line above a demo choice: what is built, then what is asked next. A
    demo name is walked a directory at a time, so the line names the place
    being answered, <area> then <demo>, until it reaches a demo.
    """
    demo = demo_helper()
    place = "<demo>"
    if demo:
        try:
            target = demo.demo_target(words[3:])
            if target and demo.demo_find(target):
                place = "is the whole command"
            else:
                place = demo.demo_place(target)
        except Exception:
            pass
    return "cloonix %s %s" % (" ".join(words), place)

def complete_usage(words):
    """The syntax of the command being typed, or "".

    Matched on the words given, so "cli nemo add kvm" finds the kvm entry
    whatever the net. The command comes back from its first word, the words
    typed then the places that follow; what was put in those places is not
    repeated.
    """
    if len(words) > 2 and words[0] == "cli" and words[2] == "demo":
        return demo_usage(words)
    entry = complete_entry(words)
    if not entry:
        return ""
    pattern = entry[0]
    return "cloonix %s %s" % (" ".join(words[:len(pattern)]), entry[1])

def complete_example(words):
    """One typical value for the place being typed, or "".

    On a fixed shape, plug, unplug, save, the place is counted from the end of
    the matched pattern. Where options and a required word mix, add kvm, win,
    freebsd, cisco, cnt, the examples are walked and the first one not on the
    line answers, so the example command keeps its own order and a finished
    line is offered nothing.
    """
    if len(words) > 2 and words[0] == "cli" and words[2] == "demo":
        return ""
    entry = complete_entry(words)
    if not entry:
        return ""
    example = entry[2]
    typed = words[len(entry[0]):]
    if not places_repeat(entry[1]):
        pos = len(typed)
        return example[pos] if pos < len(example) else ""
    if not example:
        return ""
    if not typed:
        return example[0]
    if len([w for w in typed
            if not w.startswith("-") and "=" not in w]) >= places_required(
                entry[1]):
        return ""
    for ex in example[1:]:
        key = ex.split("=", 1)[0]
        if not any(w == ex or w.split("=", 1)[0] == key for w in typed[1:]):
            return ex
    return ""

GUEST_KINDS = ("kvm", "cnt")

def objects_of(net, nets):
    """Every object of a running net as {name: kind}, asked of the server.

    list_commands_req answers the script that rebuilds the net, one command per
    entry of an "items" array: word 4 of an add is the kind, word 5 the name,
    the last word of a plug a lan, or an air for a wlan; the airs a radio
    starts on are in no plug, its wlan= says they exist. A round trip behind a
    keypress: a short timeout, and every failure silent, no candidates.
    """
    try:
        ip, port, passwd = CL.resolve(net, nets, None)
        door = CL.Doorways(ip, port, passwd, 1.0)
    except (Exception, SystemExit):
        # resolve raises SystemExit for a net missing from the config or a field.
        return {}
    found = {}
    try:
        door.send_json({"fctident": "list_commands_req", "tid": 0,
                        "is_layout": 0})
        door.sock.settimeout(1.0)
        while True:
            reply = door.recv_json()
            if reply is None:
                break
            if reply.get("fctident") != "list_commands_resp":
                continue
            for line in (reply.get("items") or []):
                w = line.split()
                if len(w) < 6 or w[0] != "cloonix" or w[1] != "cli":
                    continue
                if w[3] == "plug":
                    found[w[-1]] = ("air" if w[5].startswith("wlan")
                                    else "lan")
                elif w[3] == "add":
                    kind = {"win": "kvm", "freebsd": "kvm",
                            "cisco": "kvm"}.get(w[4], w[4])
                    found[w[5]] = kind
                    if any(x.startswith("wlan=") for x in w[6:]):
                        found[CL.CNTWIFI if kind == "cnt"
                              else CL.KVMWIFI] = "air"
            break
    except Exception:
        pass
    try:
        door.close()
    except Exception:
        pass
    return found

PHY_TOPO_WAIT = 2.5

def host_phys(net, nets):
    """The physical interfaces of the server of a net, from its topo."""
    try:
        ip, port, passwd = CL.resolve(net, nets, None)
        door = CL.Doorways(ip, port, passwd, 1.0)
    except (Exception, SystemExit):
        return []
    try:
        door.send_json({"fctident": "event_topo_sub", "tid": 0})
        door.sock.settimeout(PHY_TOPO_WAIT)
        while True:
            reply = door.recv_json()
            if reply is None:
                return []
            if reply.get("fctident") == "event_topo":
                return sorted(reply.get("info_phy") or [])
    except (OSError, IOError, ValueError, KeyError):
        return []
    finally:
        try:
            door.close()
        except Exception:
            pass

def complete_guests(net, nets, kinds=GUEST_KINDS):
    """The names of a running net that fit this place, of the kinds wanted.

    kinds None means every object, which is what "del <name>" deletes."""
    found = objects_of(net, nets)
    return sorted(n for n, kind in found.items()
                  if kinds is None or kind in kinds)

# What one net is given to answer, and how long every one of them has
# together: they are asked at the same moment, not one after the other.
NET_PROBE_TIMEOUT = 0.7

NET_PROBE_FRESH = 10.0

_PROBE = {"nets": None, "answered": None, "threads": None, "t0": 0.0}

def probe_start(nets):
    """Ask every net now, so the answers are in before anybody needs them.

    It does not wait: called at the top of the guide, while the person reads
    the verbs, so the tab at the net position waits for nothing. A line already
    a command never calls it. Started again when the answers are older than
    NET_PROBE_FRESH, or were asked of another config.
    """
    fresh = (_PROBE["answered"] is not None
             and _PROBE["nets"] is nets
             and time.monotonic() - _PROBE["t0"] < NET_PROBE_FRESH)
    if fresh:
        return
    answered = {}

    def ask(name):
        try:
            if CL.server_version(nets[name], NET_PROBE_TIMEOUT):
                answered[name] = True
        except Exception:
            pass

    threads = [threading.Thread(target=ask, args=(n,)) for n in nets]
    for t in threads:
        t.daemon = True
        t.start()
    _PROBE.update(nets=nets, answered=answered, threads=threads,
                  t0=time.monotonic())

def nets_running(nets):
    """The nets whose server answers, all asked at once.

    The question is server_version's, work_dir_req, answered on the spot. A
    thread per net, so the wait is one timeout, none when probe_start ran early
    enough.
    """
    probe_start(nets)
    left = NET_PROBE_TIMEOUT + 0.2 - (time.monotonic() - _PROBE["t0"])
    for t in _PROBE["threads"]:
        t.join(max(left, 0.0))
    return _PROBE["answered"]

def demo_helper():
    """cloonix_demo.py, or None when it is not beside the client."""
    return CL.load_sibling("cloonix_demo")

def demo_table(words):
    """What comes next in a demo name, one directory level at a time.

    The one candidate list read from the local disk: a demo is a programme this
    client runs here, while a bulk image lives on the server. Asked of
    cloonix_demo.py, so what is offered and what plays are one list.
    """
    demo = demo_helper()
    if not demo:
        return {}
    try:
        target = demo.demo_target(words[3:])
        if target and demo.demo_find(target):
            return {}
        return demo.demo_level(target)
    except Exception:
        return {}

def demo_kept(rest):
    """The demo words as far as they lead somewhere.

    One more component at a time: a word is kept while what it builds is a
    demo or a directory holding some, and dropped with the tail after it
    where it is neither.
    """
    demo = demo_helper()
    if not demo:
        return list(rest)
    kept = []
    for word in rest:
        try:
            target = demo.demo_target(kept + [word])
            if not demo.demo_find(target) and not demo.demo_level(target):
                break
        except Exception:
            break
        kept.append(word)
    return kept

def demo_is_named(words):
    """True when the words after the net leave nothing to choose.

    A demo name is walked a directory at a time, so the count of places cannot
    tell; the player can, the name reaching a file or not. A directory holding
    one demo reaches it, "cli nemo demo cloonix a2b" being a2b.py; one holding
    several reaches nothing.
    """
    demo = demo_helper()
    if not demo or len(words) < 4:
        return False
    try:
        return bool(demo.demo_find(demo.demo_target(words[3:])))
    except Exception:
        return False

def net_table(nets, verb=None):
    """The nets this verb can be given, with their address and web ports.

    start starts a server: the nets that answered are left out. Every other
    verb speaks to a running server: the nets that stayed silent are left out.
    An answer is proof, a silence may be a timeout, so start hides only on
    proof; when no net answers at all, the whole config comes back, as before
    there was a probe.
    """
    show = nets
    if verb in ("start", "stop", "cli"):
        answered = nets_running(nets)
        if verb == "start":
            show = dict((n, nets[n]) for n in nets if n not in answered)
        elif answered:
            show = dict((n, nets[n]) for n in answered)
    doors = dict((n, "%s:%s" % (show[n].get("cloonix_ip", "?"),
                                show[n].get("cloonix_port", "?")))
                 for n in show)
    pad = max([len(d) for d in doors.values()] or [0])
    return dict((name, ("%-*s  https %s" % (
        pad, doors[name], show[name].get("cloonix_web", "-"))).rstrip())
        for name in sorted(show))

def complete_table(words, nets):
    """The candidates for the word about to be typed, each with its line.

    words holds what is on the line after "cloonix", so its length is the
    position completed: [] the top verb, ["cli"] the net, ["cli", "nemo"] the
    verb of cli, ["cli", "nemo", "add"] what to add, ["cli", "nemo", "add",
    "cnt"] the name, which nobody knows. A candidate with nothing to say, a
    guest, has an empty line, put together by two_columns.
    """
    n = len(words)

    if n == 0:
        return dict(CL.VERBS)

    verb = words[0]
    if verb not in CL.VERBS:
        return {}
    # The one that asks every net at once, so it takes none.
    if verb == "list":
        return {}

    # The net, and which nets those are depends on the verb: see net_table.
    if n == 1:
        return net_table(nets, verb)

    # start and stop take the net and nothing else.
    if verb in ("start", "stop"):
        return {}

    net = words[1]
    if n == 2:
        return dict(CL.CLI_VERBS)
    return cli_table(words, net, nets)

OVS_SUBCMD = {
    "ovs-vsctl": (
        ("list",       "list Mirror, list Port, list Bridge ..."),
        ("show",       "the whole database, briefly"),
        ("find",       "find Port name=s_k_1_5_0"),
        ("get",        "one column of one row"),
        ("list-br",    "the bridges, one lan each"),
        ("list-ports", "list-ports _b_lan1"),
        ("list-ifaces", "the interfaces of a bridge"),
    ),
}

def cli_table(words, net, nets):
    """The candidates after "cli <net> <verb>", the verb being words[2].

    m counts from the verb of cli, so m == 1 is the place right after it.
    """
    verb = words[2]
    if verb not in CL.CLI_VERBS:
        return {}
    m = len(words) - 2

    if verb == "demo":
        return demo_table(words)

    # Nothing follows the verb for these.
    if verb == "rpc":
        return {}

    if verb in ("ssh", "scp", "files", "save"):
        # After the guest there is a path, a command: nothing anybody can
        # offer, so the syntax line is shown instead.
        if m != 1 and not (verb == "files" and m == 2):
            return {}
        # save writes a qcow2, which only a kvm has.
        kinds = ("kvm",) if verb == "save" else GUEST_KINDS
        return dict((g, "") for g in complete_guests(net, nets, kinds))

    if verb == "sniff":
        # anything with an interface, and the airs
        if m != 1:
            return {}
        return dict((g, "") for g in complete_guests(
            net, nets, ITEM_KINDS + ("air",)))

    if verb in ("plug", "unplug"):
        if m != 1:
            return {}
        return dict((g, "") for g in complete_guests(net, nets, ITEM_KINDS))

    if verb == "dump":
        if m == 1:
            return dict(COMPLETE_DUMP, **COMPLETE_DUMP_OVS)
        if m == 2 and words[3] == "ip":
            return dict((g, "") for g in complete_guests(net, nets, ("nat",)))
        if m == 3 and words[3] == "ip":
            return dict((g, "") for g in complete_guests(net, nets))
        if m == 2 and words[3] in OVS_SUBCMD:
            return dict(OVS_SUBCMD[words[3]])
        return {}

    if verb == "trace":
        if m == 1:
            return dict(COMPLETE_TRACE, **COMPLETE_TRACE_PROBE)
        # probe names a guest, the four subscriptions name nothing.
        if m == 2 and words[3] == "probe":
            return dict((g, "") for g in complete_guests(net, nets))
        return {}

    if verb == "del":
        if m != 1:
            return {}
        out = dict(CL.DEL_KINDS)
        # an air goes with its last radio, it is not deleted
        out.update((g, "") for g, kind in objects_of(net, nets).items()
                   if kind != "air")
        return out

    if verb == "cnf":
        found = objects_of(net, nets)
        if m == 1:
            return dict((g, found[g]) for g in sorted(found))
        kind = found.get(words[3])
        if m == 2:
            return dict(COMPLETE_CNF.get(kind, {}), **COMPLETE_PLACE)
        if m == 3 and words[4].startswith("x="):
            return {"y=200": "the item on the canvas, pinned there"}
        # after an interface, its sniff or its angle; after a direction, its
        # setting
        if m == 3 and CL.is_iface(words[4]):
            return dict(COMPLETE_SNIFF, **COMPLETE_ANGLE)
        if m == 3 and words[4] in CL.A2B_DIR:
            return dict(COMPLETE_A2B_CMD)
        # cnf cnt1 cpu=50, then ram=
        if m == 3 and kind == "cnt" and words[4].startswith("cpu="):
            return {"ram=200": "megabytes, 0 for no cap"}
        return {}

    if verb != "add":
        return {}

    if m == 1:
        return dict(CL.ADD_KINDS)
    kind = words[3]
    if m == 2 and kind != "phy":
        return {}
    if kind == "c2c":
        if m != 3:
            return {}
        far = net_table(nets)
        return dict((x, far[x]) for x in far if x != net)
    if kind == "phy":
        # The name is a host interface of the server, which only the server
        # can list: its topo carries them as info_phy.
        if m == 2:
            return dict((p, "host interface") for p in host_phys(net, nets))
        return ({"--macvlan": "the host keeps the interface, absorbed "
                              "without"} if m == 3 else {})
    if kind in ("kvm", "win", "freebsd", "cisco", "cnt"):
        # The options, in any order, until the image closes the command.
        if any(not w.startswith("-") and "=" not in w for w in words[5:]):
            return {}
        return dict((o, d) for o, d in COMPLETE_OPTS[kind].items()
                    if not any(w == o or (o.endswith("=") and
                                          w.startswith(o))
                               for w in words[5:]))
    return {}

def complete(words, nets):
    """The candidate words alone, which is all the shell hook can show."""
    return list(complete_table(words, nets))

HOOK = r"""# zsh is the default shell of macos and has neither complete nor compgen.
# zsh: bashcompinit needs compdef from compinit, run here if nothing ran it
# (-u no prompt, -C no dump rebuild). Dead code in bash.
if [ -n "${ZSH_VERSION}" ]; then
    if ! type compdef >/dev/null 2>&1; then
        autoload -U +X compinit 2>/dev/null && compinit -u -C 2>/dev/null
    fi
    autoload -U +X bashcompinit 2>/dev/null && bashcompinit 2>/dev/null
fi

_cloonix_complete()
{
    # $((...)) around the length, not a bare COMP_CWORD-1: bash takes
    # either, zsh reads the bare name as a history modifier and fails with
    # "unrecognized modifier".
    local cur cmd candidates usage
    cur="${COMP_WORDS[COMP_CWORD]}"
    cmd="${COMP_WORDS[0]}"
    # A dash later in the line is an option of the built command, in the table like any word.
    candidates=$("${cmd}" --complete \
                 "${COMP_WORDS[@]:1:$((COMP_CWORD-1))}" 2>/dev/null)
    COMPREPLY=( $(compgen -W "${candidates}" -- "${cur}") )
    # Nothing completable (a name, a number): show the syntax through COMPREPLY,
    # two entries with an empty one so no common prefix is typed in.
    if [ ${#COMPREPLY[@]} -eq 0 ]; then
        usage=$("${cmd}" --complete-usage \
                "${COMP_WORDS[@]:1:$((COMP_CWORD-1))}" 2>/dev/null)
        if [ -n "${usage}" ]; then
            COMPREPLY=( "${usage}" "" )
        fi
        # readline's ambiguous case: bash lists on the second tab (one with show-all-if-ambiguous).
    fi
    return 0
}
complete -F _cloonix_complete cloonix
"""

PROMPT = "cloonix "

TYPED = []

def _readline():
    """The readline module, or None where python has none."""
    try:
        import readline
    except Exception:
        return None
    return readline

def _bind_tab(rl, nets):
    """Tab on our prompt answers out of the same tables the hook uses."""
    if "libedit" in (getattr(rl, "__doc__", "") or ""):
        rl.parse_and_bind("bind ^I rl_complete")
    else:
        rl.parse_and_bind("tab: complete")
    # Only whitespace splits a word, so eth=2 and guest:/path stay whole.
    rl.set_completer_delims(" \t\n")

    def answer(text, state):
        try:
            buf = rl.get_line_buffer()[:rl.get_endidx()]
            done = buf.split()
            if buf and not buf.endswith(" "):
                done = done[:-1]
            cands = [c for c in complete(done, nets) if c.startswith(text)]
            if not cands:
                example = complete_example(done)
                if example and example.startswith(text):
                    cands = [example]
            return cands[state] if state < len(cands) else None
        except Exception:
            return None

    rl.set_completer(answer)

    def show(substitution, matches, longest):
        try:
            buf = rl.get_line_buffer()
            done = buf.split()
            if buf and not buf.endswith(" "):
                done = done[:-1]
            table = complete_table(done, nets)
            sys.stdout.write("\n" + CL.two_columns(
                dict((m, table.get(m, "")) for m in matches))
                + _example(done, table) + "\n" + PROMPT + buf)
            sys.stdout.flush()
        except Exception:
            pass

    if hasattr(rl, "set_completion_display_matches_hook"):
        rl.set_completion_display_matches_hook(show)

def _example(words, table):
    """The example for the place being typed, ready to print, or "".

    With no candidate the example is all there is. Candidates, guests or verbs,
    are the answer and no example is shown, but on the add kvm, win, freebsd,
    cisco and cnt line, where the options are offered and only the example says
    how the line ends, with its image.
    """
    example = complete_example(words)
    if not example:
        return ""
    entry = complete_entry(words)
    if table and not (entry and places_repeat(entry[1])):
        return ""
    return ("\n" if table else "") + "  for example:  %s\n" % example

def _hint(words, nets):
    """The syntax of the place, then every word that fits it with its line. A
    word that is no verb, a typo, is answered with the verbs, so no place is
    met with silence.
    """
    out = ""
    usage = complete_usage(words)
    if usage:
        out += usage + "\n"
    table = complete_table(words, nets)
    if not out and not table:
        table = dict(CL.VERBS)
    return out + CL.two_columns(table) + _example(words, table)

def _keys():
    """The msvcrt module, or None where python is not on windows."""
    try:
        import msvcrt
    except Exception:
        return None
    return msvcrt

def _tab(buf, pos, nets):
    """Tab at pos: the one candidate typed in, or the list under the line."""
    head = buf[:pos]
    done = head.split()
    text = "" if (not head or head.endswith(" ")) else done.pop()
    cands = [c for c in complete(done, nets) if c.startswith(text)]
    if not cands:
        example = complete_example(done)
        cands = [example] if example and example.startswith(text) else []
    if len(cands) == 1:
        add = cands[0][len(text):] + " "
        return buf[:pos] + add + buf[pos:], pos + len(add)
    if cands:
        table = complete_table(done, nets)
        sys.stdout.write("\n" + CL.two_columns(
            dict((c, table.get(c, "")) for c in cands))
            + _example(done, table) + "\n")
    return buf, pos

def _ask_keys(kb, line, nets):
    """The windows line editor, on msvcrt: type, backspace, left, right, home,
    end, delete, up and down through the history, tab for the candidates,
    enter. A redraw is a carriage return and the line, so a line wider than
    the console smears.

    Written from the documented msvcrt api, never run on windows: if it
    misbehaves, the _keys() branch of _ask goes and the plain prompt takes
    over.
    """
    buf = (line + " ") if line else ""
    pos = shown = len(buf)
    seen = TYPED + [buf]
    idx = len(seen) - 1

    def draw():
        nonlocal shown
        sys.stdout.write("\r" + PROMPT + buf + " " * max(shown - len(buf), 0)
                         + "\r" + PROMPT + buf[:pos])
        sys.stdout.flush()
        shown = len(buf)

    draw()
    while True:
        ch = kb.getwch()
        if ch in ("\x00", "\xe0"):
            code = kb.getwch()
            if code == "H" and idx > 0:
                idx -= 1
                buf, pos = seen[idx], len(seen[idx])
            elif code == "P" and idx < len(seen) - 1:
                idx += 1
                buf, pos = seen[idx], len(seen[idx])
            elif code == "K" and pos:
                pos -= 1
            elif code == "M" and pos < len(buf):
                pos += 1
            elif code == "G":
                pos = 0
            elif code == "O":
                pos = len(buf)
            elif code == "S" and pos < len(buf):
                buf = buf[:pos] + buf[pos + 1:]
        elif ch in ("\r", "\n"):
            sys.stdout.write("\n")
            return buf.strip()
        elif ch == "\x03":
            raise KeyboardInterrupt
        elif ch == "\x1a":
            raise EOFError
        elif ch == "\x08":
            if pos:
                buf = buf[:pos - 1] + buf[pos:]
                pos -= 1
        elif ch == "\t":
            buf, pos = _tab(buf, pos, nets)
        elif ch >= " ":
            buf = buf[:pos] + ch + buf[pos:]
            pos += 1
        draw()

def _ask(rl, line, words, nets):
    """The line back, with whatever the person added to it."""
    start = (line + " ") if line else ""
    if rl:
        rl.set_startup_hook(lambda: rl.insert_text(start))
        try:
            return input(PROMPT)
        finally:
            rl.set_startup_hook()
    kb = _keys()
    if kb:
        return _ask_keys(kb, line, nets)
    got = input(PROMPT + start).strip()
    if got:
        got = CL.unabbrev(got, complete(words, nets))
    return ("%s %s" % (line, got)) if got else line

def split(line):
    """The words of a line, quotes honoured, half-typed quotes tolerated."""
    try:
        return shlex.split(line)
    except ValueError:
        return line.split()

def guide(client, nets, words, hinted=False):
    """Ask until the line is a command, and give back its words.

    hinted: the caller has shown what the first place accepts, bare cloonix
    printing its help, so it is not printed twice. None when the person added
    nothing, an unchanged line being a give up, which also ends the loop.
    """
    global CL
    CL = client
    # First thing, before readline, before the help is printed: ask every net
    # whether it is there.
    probe_start(nets)
    rl = _readline()
    if rl:
        try:
            _bind_tab(rl, nets)
        except Exception:
            rl = None
    line = " ".join(words)
    while True:
        # Expanded first so a short verb reads as on the command line.
        line = " ".join(client.expand(split(line)))
        kept = trim_to_menu(client, split(line))
        gone = split(line)[len(kept):]
        if gone:
            sys.stdout.write("'%s' is not one of these, so the line stops "
                             "here:\n" % gone[0])
            hinted = False
        line = " ".join(kept)
        if line_is_command(client, split(line)):
            break
        if hinted:
            hinted = False
        else:
            sys.stdout.write(_hint(split(line), nets))
        try:
            sys.stdout.write("\n")
            got = _ask(rl, line, split(line), nets).strip()
        except (EOFError, KeyboardInterrupt):
            sys.stdout.write("\n")
            return None
        if got == line:
            return None
        TYPED.append(got)
        line = got
    return split(line)

def run(client, nets, words, mode):
    """The one entry point. mode is "words", "usage" or "hook".

    Never raises: a completion that fails is a tab that does nothing, and a
    traceback into somebody's prompt is worse than no candidates.
    """
    global CL
    CL = client
    try:
        if mode == "hook":
            # bash and zsh only.
            if os.name != "nt":
                sys.stdout.write(HOOK)
            return 0
        if mode == "words":
            for cand in complete(words, nets):
                print(cand)
            return 0
        usage = complete_usage(words)
        example = complete_example(words)
        if usage and example:
            print("%s   e.g. %s" % (usage, example))
        elif usage:
            print(usage)
        elif example:
            print("e.g. %s" % example)
    except Exception:
        pass
    return 0
