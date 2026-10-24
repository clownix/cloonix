"""frrlib - the talk to frr of the frr_demo demos: vtysh, configure mode,
the kernel MPLS switch, and the words looked for in what vtysh answers. The
rest is toylib.py."""
from demolib import ssh
from toylib import (
    forwarding, need_kernel, opening, ping, pings, proof, until,
    verdict, words_in, words_out)

# What the demos import from here, toylib re-exported with it.
__all__ = ["forwarding", "need_kernel", "opening", "ping",
           "pings", "proof", "until", "verdict", "vtysh", "conf", "has",
           "lacks", "mpls_on"]

def vtysh(node, cmd):
    """One vtysh command, its output; never raises."""
    return ssh(node, "vtysh -c '%s'" % cmd, check=False)

def conf(node, *lines):
    """Lines typed in vtysh configure mode, one -c each."""
    args = " ".join("-c '%s'" % l for l in ("configure terminal",) + lines)
    return ssh(node, "vtysh %s" % args, check=False)

def has(node, cmd, words):
    return words_in(vtysh(node, cmd), words)

def lacks(node, cmd, words):
    return words_out(vtysh(node, cmd), words)

def mpls_on(node, eths):
    """Kernel label switching in a container: the label table sized, and
    labelled packets accepted on the interfaces named."""
    lines = ["net.mpls.platform_labels=100000"]
    lines += ["net.mpls.conf.eth%d.input=1" % num for num in eths]
    ssh(node, "sysctl -w %s >/dev/null" % " ".join(lines))
