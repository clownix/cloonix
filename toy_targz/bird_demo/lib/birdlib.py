"""birdlib - the talk to BIRD of the bird_demo demos: the configuration
written as the guide prints it, bird started, birdc, and the words looked
for in what birdc answers. The rest is toylib.py."""
import time
from demolib import ssh
from toylib import (
    forwarding, has_tcp_ao, need_kernel, opening, ping, pings, proof,
    put_any, until, verdict, words_in, words_out)

# What the demos import from here, toylib re-exported with it.
__all__ = ["forwarding", "has_tcp_ao", "need_kernel", "opening", "ping",
           "pings", "proof", "until", "verdict", "put_any", "bird_start",
           "bird_stop", "birdc", "reconfigure", "has", "lacks"]

def bird_start(node, conf):
    """bird.conf written, bird started, back once birdc answers."""
    put_any(node, "/etc/bird/bird.conf", conf)
    ssh(node, "mkdir -p /run/bird /var/log/bird")
    out = ssh(node, "bird -p -c /etc/bird/bird.conf 2>&1", check=False)
    if out.strip():
        raise RuntimeError("bird.conf of %s refused:\n%s" % (node, out))
    ssh(node, "bird -c /etc/bird/bird.conf")
    for _ in range(30):
        if "Daemon is up" in birdc(node, "show status"):
            return
        time.sleep(1)
    raise RuntimeError("bird did not start on %s" % node)

def birdc(node, cmd):
    """One birdc command, its output; never raises."""
    return ssh(node, "birdc -r %s" % cmd, check=False)

def reconfigure(node, conf):
    """A new bird.conf, applied by birdc configure."""
    put_any(node, "/etc/bird/bird.conf", conf)
    return ssh(node, "birdc configure", check=False)

def bird_stop(node):
    """bird asked to stop, as birdc down does."""
    return ssh(node, "birdc down", check=False)

def has(node, cmd, words):
    return words_in(birdc(node, cmd), words)

def lacks(node, cmd, words):
    return words_out(birdc(node, cmd), words)
