#!/usr/bin/env python3
r"""nginx - a port of the host that arrives inside a container.

    ./nginx.py [net]          the net defaults to nemo

One container, one wire, one nat::

                    published: HOSTPORT -> 80
                            |
    Cnt1 ---- lan1 ---- [nat1] ---- the host
  10.117.0.10        10.117.0.1
   nginx :80          = passt

A cloonix network is isolated and its nat is egress only; a published port
is the one way in, spelled as docker spells it, publish=HOSTPORT:80.

  ACT 1  the container, the nat, the wire.
  ACT 2  nginx started inside Cnt1, serving one page, read from inside.
  ACT 3  the same page asked for from the host, with nothing published.
  ACT 4  the port published, and the same question answered.
  ACT 5  the port moved, then taken away, nginx never told of it.

Image: proxy_trixie (nginx-light and curl)
Size:  1 container, 1 lan, 1 nat

THE PACKET TO CATCH
-------------------
  The interface of the container, with the port published:

    cloonix cli nemo cnf Cnt1 eth0 sniff=on
    cloonix cli nemo sniff Cnt1 eth0 --dump tcp port 80

  The connection leaves the host as HOSTPORT and arrives on this wire as
  80: a filter on HOSTPORT catches nothing. During ACT 3, nothing being
  published, nothing appears either: the connection is never carried, so
  there is nothing to drop.

Important
  The address is set by hand: passt is started with -a 10.117.0.10 -g
  10.117.0.1, which two commands write, where a dhcp client costs a hundred
  lines; demos/cloonix/nat.py shows the dhcp exchange.

  The host port is asked of the kernel, not fixed, so a second run does not
  meet the port of the first; it is printed, to put in a browser.

  A cnf restarts passt, which cuts every connection through that nat; the
  container keeps its address. Where that matters, publish at creation.

WHAT A PUBLISHED PORT IS
------------------------
The nat is passt, an unprivileged process on the host owning the real
sockets. A published port is one more -t argument of that process: passt
binds the port on the host and carries what arrives to the container. No
firewall rule is written, no privilege taken, and the port goes with the
nat. docker reaches the same result with an iptables DNAT rule and a
userland proxy, both root, both able to outlive their container:

    cloonix cli nemo add nat nat1 publish=8080:80
    cloonix cli nemo cnf nat1 publish=9090:80
    cloonix cli nemo cnf nat1 publish=none

publish=5353:53/udp does the same for udp; each cnf replaces the whole list.
"""
import glob
import os
import socket
import sys
import time

# What every demo shares is in demos/lib/demolib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import (
    NET, answers, check_net, erase_net, place, put, reached, run, ssh, step,
    story, wait_exists, unavailable, needs_subuid, guarded)

ROOTFS_CHOICES = ("proxy_trixie",)

CNT = "Cnt1"
NAT = "nat1"
LAN = "lan1"

PASST_GUEST = "10.117.0.10"
PASST_GW = "10.117.0.1"

GUEST_PORT = 80
MESSAGE = "hello from cloonix container Cnt1"

def pause(what):
    """Stop until a key comes, where somebody can press one.

    A terminal is one place, the Demo panel of the canvas another: there
    every key typed in the log reaches this stdin as a line, and the panel
    says so with CLOONIX_DEMO_KEY=1. Run from a script, or by
    debug/demo_test.py, nobody is there and the demo goes on.
    """
    print()
    print("    %s" % what)
    if sys.stdin.isatty() or os.environ.get("CLOONIX_DEMO_KEY") == "1":
        print("    press Enter here to go on")
        sys.stdin.readline()
    print()

def choose_rootfs():
    said = ""
    for cand in ROOTFS_CHOICES:
        r = run("cloonix cli %s add cnt probe_ngx eth=1 %s" % (NET, cand))
        ok = (r.returncode == 0) and wait_exists("probe_ngx", 20)
        run("cloonix --wait 0.3 cli %s del probe_ngx" % NET)
        if ok:
            return cand
        said = (r.stdout + r.stderr).strip() or "nothing"
    unavailable("the server would not start a container on %s.\n"
             "  it said: %s\n"
             "  fetch it:\n"
             "    cd /var/lib/cloonix/bulk\n"
             "    wget https://clownix.net/downloads/cnt_demo_bulk/"
             "proxy_trixie.tar.gz\n"
             "    (cloonix extracts it at the first use)\n"
             "  or build it:\n"
             "    cd vm_create/cnt_kvm_demo && ./cnt_proxy_trixie.sh"
             % (" or ".join(ROOTFS_CHOICES), said))

def create(name, rootfs, eths):
    last = ""
    for _ in range(6):
        r = run("cloonix cli %s add cnt %s %s %s"
                % (NET, name, "eth=%d" % eths, rootfs))
        last = (r.stdout + r.stderr).strip()
        if r.returncode == 0 and wait_exists(name):
            return
        if r.returncode == 0:
            last = "the add was taken and %s never appeared" % name
        time.sleep(2)
    raise RuntimeError("%s was never created. The server said: %s"
                       % (name, last or "nothing at all"))

NGINX_CONF = """user root;
worker_processes 1;
error_log /var/log/nginx/error.log;
pid /tmp/nginx.pid;
events { worker_connections 64; }
http {
    access_log /var/log/nginx/access.log;
    server {
        listen PORT default_server;
        root /var/www/html;
        index index.html;
    }
}
"""

def start_nginx(node):
    put(node, "/etc/nginx/nginx.conf",
        NGINX_CONF.replace("PORT", str(GUEST_PORT)))
    ssh(node, "mkdir -p /var/www/html /var/log/nginx")
    put(node, "/var/www/html/index.html", MESSAGE + "\n")
    said = ssh(node, "nginx -t 2>&1", check=False)
    if "successful" not in said:
        raise RuntimeError("nginx refused its configuration:\n%s" % said)
    ssh(node, "pkill nginx; true", check=False)
    ssh(node, "nginx </dev/null >/tmp/nginx.log 2>&1 &", check=False)
    time.sleep(2)

def knock(port, timeout=4):
    """Open the published port from the HOST and say what came back.

    A socket rather than curl: the demo must run where curl is not
    installed, and what is wanted here is the plain answer to "did the
    connection open at all", answered by the name of an exception rather
    than by the exit code of a tool.
    """
    try:
        with socket.create_connection(("127.0.0.1", port), timeout) as sk:
            sk.sendall(b"GET / HTTP/1.0\r\nHost: cnt1\r\n\r\n")
            sk.settimeout(timeout)
            got = b""
            while len(got) < 4096:
                more = sk.recv(1024)
                if not more:
                    break
                got += more
        return got.decode("utf-8", "replace")
    except OSError as err:
        return "no connection: %s" % err.__class__.__name__

def port_is_free(port):
    """True when nothing on this host has the port."""
    s = socket.socket()
    try:
        s.bind(("127.0.0.1", port))
        return True
    except OSError:
        return False
    finally:
        s.close()

def free_host_port():
    """A port nothing on this host is using, asked of the kernel."""
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port

def body_of(said):
    """What came after the http headers, or the whole thing if there are none."""
    return said.split("\r\n\r\n", 1)[1].strip() if "\r\n\r\n" in said \
        else said.strip()

LAYOUT = {
    CNT: (-250, 0),
    LAN: (0, 0),
    NAT: (250, 0),
}

ETHS = {
    (CNT, "eth0"): 90,
}

CARD = r"""
                    published: HOSTPORT -> 80
                            |
    Cnt1 ---- lan1 ---- [nat1] ---- the host
  10.117.0.10        10.117.0.1
   nginx :80          = passt

  One container, one wire, one nat. Nothing is routed and nothing is
  bridged: the whole demo is the arrow above, a port of the machine you
  are sitting at carried to port 80 of a container that has no address
  on any network you can reach.
"""

def main():
    needs_subuid()
    step("checking the server")
    check_net()

    step("looking for the image")
    rootfs = choose_rootfs()
    erase_net()
    place()
    story(CARD)
    story("  image: %s" % rootfs)

    step("ACT 1: the container, the nat, the wire")
    story()
    create(CNT, rootfs, 1)
    r = run("cloonix cli %s add nat %s" % (NET, NAT))
    if r.returncode or not wait_exists(NAT, 20):
        sys.exit("the server would not add a nat: %s"
                 % ((r.stdout + r.stderr).strip() or "it said nothing"))
    run("cloonix cli %s plug %s eth0 %s" % (NET, CNT, LAN))
    run("cloonix cli %s plug %s %s" % (NET, NAT, LAN))
    end = time.time() + 90
    while time.time() < end and not answers(CNT):
        time.sleep(2)
    if not answers(CNT):
        raise RuntimeError("%s never answered ssh" % CNT)
    ssh(CNT, "ip addr add %s/24 dev eth0" % PASST_GUEST)
    ssh(CNT, "ip link set dev eth0 up")
    ssh(CNT, "ip route add default via %s" % PASST_GW)
    story()
    story("    %s is %s, and that is its only address. passt is started" %
          (CNT, PASST_GUEST))
    story("    with -a %s -g %s, so these two are what it" %
          (PASST_GUEST, PASST_GW))
    story("    expects on the wire and there is no dhcp to wait for.")
    story()
    # The nat learns the guest from its frames: a first ping to the gateway
    # is the proof it carries traffic outward, and what a later publish
    # carries inward is delivered to a guest it has seen.
    out = ssh(CNT, "ping -c 3 -W 1 %s" % PASST_GW, check=False)
    story("    %s reaches its gateway: %s" %
          (CNT, "yes" if reached(out) else "no"))
    story()

    step("ACT 2: nginx inside the container")
    story()
    start_nginx(CNT)
    inside = ssh(CNT, "curl -sS -m 5 http://127.0.0.1/ 2>&1",
                 check=False).strip()
    story()
    story("    read from INSIDE %s, before any claim about the host:" % CNT)
    story("      %s" % (inside or "(nothing)"))
    ok_inside = MESSAGE in inside
    if not ok_inside:
        said = ssh(CNT, "cat /tmp/nginx.log /var/log/nginx/error.log 2>&1",
                   check=False)
        raise RuntimeError("nginx is not serving inside the container:\n%s"
                           % (said.strip() or "it logged nothing"))
    story()

    step("ACT 3: the same page, asked from the HOST, nothing published")
    story()
    host_port = free_host_port()
    story("    the host port for this run is %d, asked of the kernel so a"
          % host_port)
    story("    second run does not fight the first for a fixed number.")
    story()
    said = knock(host_port)
    ok_closed = MESSAGE not in said
    story("      http://127.0.0.1:%d/  ->  %s"
          % (host_port, said.strip().split("\n")[0] or "(nothing)"))
    story()
    story("    Refused, and that is the demonstration. nginx is running and")
    story("    listening; the container is up and cabled; the nat is up and")
    story("    carrying traffic outward. A nat is egress only until it is")
    story("    asked, so there is nothing carrying this connection in.")
    story()

    step("ACT 4: the port published")
    story()
    r = run("cloonix cli %s cnf %s publish=%d:%d"
            % (NET, NAT, host_port, GUEST_PORT))
    ok_published = (r.returncode == 0)
    if not ok_published:
        story("      the server would not take it: %s"
              % ((r.stdout + r.stderr).strip() or "it said nothing"))
    else:
        time.sleep(6)
        said = knock(host_port)
        story()
        story("      http://127.0.0.1:%d/  ->  %s"
              % (host_port, body_of(said) or "(nothing)"))
        # A knock that opened but got nothing is retried a while: the log
        # then says whether the published port was slow or never answered.
        waited = 0
        while (MESSAGE not in said) and ("Timeout" in said) and (waited < 30):
            time.sleep(3)
            waited += 3
            said = knock(host_port)
        if waited:
            story("      the port answered after %ds more: %s"
                  % (waited, "yes" if MESSAGE in said else "no"))
        if MESSAGE not in said:
            # proxymous keeps passt's own log beside its socket, bundle and
            # toy alike: what the republished passt said, for the record
            for log in sorted(glob.glob("/tmp/*proxymous*/nat_p/%s_passt.log"
                                        % NAT)):
                try:
                    txt = open(log).read().strip()
                except OSError as e:
                    txt = str(e)
                story("      %s:" % log)
                for line in txt[-3000:].split("\n"):
                    story("        " + line)
        ok_published = MESSAGE in said
    story()
    story("    Nothing inside %s changed between the two knocks. The same" % CNT)
    story("    nginx was listening on port %d for both, it was never" % GUEST_PORT)
    story("    reloaded and never told anything. What changed is that passt")
    story("    was given -t %d:%d instead of -t none, so it binds that port"
          % (host_port, GUEST_PORT))
    story("    on the host and carries what arrives to the container.")
    story()
    pause("open http://127.0.0.1:%d/ in a browser now, a new tab of this "
          "one does: the page comes from the container" % host_port)

    step("ACT 5: moved, then taken away")
    story()
    second_port = free_host_port()
    run("cloonix cli %s cnf %s publish=%d:%d"
        % (NET, NAT, second_port, GUEST_PORT))
    time.sleep(6)
    moved = knock(second_port)
    # A knock that opened but got nothing is retried a while: the log then
    # says whether the moved port was slow or never answered.
    waited = 0
    while (MESSAGE not in moved) and ("Timeout" in moved) and (waited < 30):
        time.sleep(3)
        waited += 3
        moved = knock(second_port)
    if waited:
        story("      the new port answered after %ds more: %s"
              % (waited, "yes" if MESSAGE in moved else "no"))
    gone = knock(host_port)
    story()
    story("      the new port %-6d -> %s"
          % (second_port, body_of(moved) or "(nothing)"))
    story("      the old port %-6d -> %s"
          % (host_port, gone.strip().split("\n")[0] or "(nothing)"))
    ok_moved = (MESSAGE in moved) and (MESSAGE not in gone)
    story()
    story("    Each cnf replaces the whole list, so a nat publishes what the")
    story("    last one said and nothing else -- which is why the old port")
    story("    went away without being mentioned.")
    story()
    run("cloonix cli %s cnf %s publish=none" % (NET, NAT))
    time.sleep(6)
    said = knock(second_port)
    ok_closed_again = MESSAGE not in said
    story()
    story("      with nothing published again: %s"
          % (said.strip().split("\n")[0] or "(nothing)"))
    story()
    story("    And nginx, which was never part of any of this, is still")
    story("    serving the same page to whoever is inside:")
    inside = ssh(CNT, "curl -sS -m 5 http://127.0.0.1/ 2>&1",
                 check=False).strip()
    story("      %s" % (inside or "(nothing)"))
    story()

    bad = []
    if not ok_inside:
        bad.append("nginx never served the page inside the container, so"
                   " nothing after act 2 means anything")
    if not ok_closed:
        bad.append("the host reached the container with nothing published,"
                   " so act 3 shows nothing: a nat is egress only until"
                   " asked")
    if not ok_published:
        bad.append("the published port did not carry the connection in")
    if not ok_moved:
        bad.append("moving the port with a second cnf did not work: the new"
                   " port did not answer, or the old one still did")
    if not ok_closed_again:
        bad.append("publish=none left the port open, so what a nat"
                   " publishes cannot be taken back")
    if bad:
        sys.exit("\n" + "\n".join("  " + b for b in bad))

    last = 8080 if port_is_free(8080) else free_host_port()
    run("cloonix cli %s cnf %s publish=%d:80" % (NET, NAT, last))
    time.sleep(6)
    said = knock(last)
    story()
    story("      http://127.0.0.1:%d/  ->  %s"
          % (last, body_of(said).strip().split("\n")[0] or "(nothing)"))

    story("""
the network is left up, with the port published. Things to try:

  read the page in a browser on this machine:
    http://127.0.0.1:%d/

  watch the connection arrive on the wire of the container, and note the
  port: it leaves the host as %d and gets here as 80.
    cloonix cli %s cnf %s eth0 sniff=on
    cloonix cli %s sniff %s eth0 --dump tcp port 80

  change the page and reload the browser, nginx serving it from the
  container the whole time:
    cloonix cli %s ssh %s "echo hello again > /var/www/html/index.html"

  take it away, and watch the browser stop being able to reach it:
    cloonix cli %s cnf %s publish=none
""" % (last, last, NET, CNT, NET, CNT, NET, CNT, NET, NAT))

if __name__ == "__main__":
    guarded(main, (CNT, NAT))
