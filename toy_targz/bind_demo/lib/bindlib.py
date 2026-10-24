"""bindlib - the talk to BIND of the bind_demo demos: the files of the ARM
written as it prints them, named started and waited for, dig, rndc, and the
lab internet the resolvers ask. The rest is toylib.py.

The ARM builds every sample on one base, the example.com zone file and the
two localhost files of its chapter 3: they are here, once, with the base
named.conf and the two named.conf more than one demo runs, the primary and
the resolver of the ARM. Each demo keeps its own named.conf, the subject of
the demo."""
import re
from demolib import in_parallel, ssh
from toylib import forwarding, opening, proof, put_any, until, verdict

# What the demos import from here, toylib re-exported with it.
__all__ = ["forwarding", "opening", "proof", "put_any", "until", "verdict",
           "base_conf", "EXAMPLE_COM", "LOCALHOST_FORWARD", "LOCALHOST_REV",
           "PRIMARY_CONF", "PRIMARY_FILES", "RESOLVER_CONF", "RESOLVER_FILES",
           "ROOT_HINTS", "named_start", "named_stop", "rndc", "dig",
           "status_of", "flags_of", "answer_of", "brief", "log_of",
           "lab_conf", "internet_start", "icmp_in_echos"]

NAMED_CONF = "/etc/bind/named.conf"

# 3.1.1 named.conf Base File, given its options by base_conf().
BASE_CONF = """\
// base named.conf file
// Recommended that you always maintain a change log in this file as shown here
// options clause defining the server-wide properties
options {
  // all relative paths use this directory as a base
  directory "/var";
  // version statement for security to avoid hacking known weaknesses
  // if the real version number is revealed
  version "not currently available";
};

// logging clause
// log to /var/log/named/example.log all events from info UP in severity (no debug)
// uses 3 files in rotation swaps files when size reaches 250K
// failure messages that occur before logging is established are
// in syslog (/var/log/messages)
//
logging {
  channel example_log {
    // uses a relative path name and the directory statement to
    // expand to /var/log/named/example.log
    file "log/named/example.log" versions 3 size 250k;
    // only log info and up messages - all others discarded
    severity info;
  };
  category default {
    example_log;
  };
};
"""

VERSION = '  version "not currently available";\n'

def base_conf(options):
    """The base named.conf, option lines added after its version statement;
    the demos append their zones."""
    return BASE_CONF.replace(VERSION, VERSION + options)

# 3.1.2 example.com base zone file
EXAMPLE_COM = """\
; base zone file for example.com
$TTL 2d    ; default TTL for zone
$ORIGIN example.com. ; base domain-name
; Start of Authority RR defining the key characteristics of the zone (domain)
@         IN      SOA   ns1.example.com. hostmaster.example.com. (
                                2003080800 ; serial number
                                12h        ; refresh
                                15m        ; update retry
                                4d         ; expiry
                                2h         ; minimum
                                )
; name server RR for the domain
           IN      NS      ns1.example.com.
; the second name server is external to this zone (domain)
           IN      NS      ns2.example.net.
; mail server RRs for the zone (domain)
        3w IN      MX  10  mail.example.com.
; the second  mail servers is  external to the zone (domain)
           IN      MX  20  mail.example.net.
; domain hosts includes NS and MX records defined above
; plus any others required
; for instance a user query for the A RR of joe.example.com will
; return the IPv4 address 192.168.254.6 from this zone file
ns1        IN      A       192.168.254.2
mail       IN      A       192.168.254.4
joe        IN      A       192.168.254.6
www        IN      A       192.168.254.7
; aliases ftp (ftp server) to an external domain
ftp        IN      CNAME   ftp.example.net.
"""

# 3.1.4 localhost Zone File
LOCALHOST_FORWARD = """\
$TTL 3h
localhost.  SOA      localhost.  nobody.localhost. 42  1d  12h  1w  3h
            NS       localhost.
            A        127.0.0.1
            AAAA     ::1
"""

# 3.1.5 localhost Reverse-Mapped Zone File
LOCALHOST_REV = """\
$TTL 1D
@        IN        SOA  localhost. root.localhost. (
                        2007091701 ; serial
                        30800      ; refresh
                        7200       ; retry
                        604800     ; expire
                        300 )      ; minimum
         IN        NS    localhost.
1        IN        PTR   localhost.
"""

# 3.3.1.2 Private IP Reverse Map Zone Files
REV_192_168_254 = """\
; reverse map zone file for 192.168.254.4 only
$TTL 2d  ; 172800 seconds
$ORIGIN 254.168.192.IN-ADDR.ARPA.
@     IN      SOA   ns1.example.com. hostmaster.example.com. (
                                        2003080800 ; serial number
                                        3h         ; refresh
                                        15m        ; update retry
                                        3w         ; expiry
                                        3h         ; nx = nxdomain ttl
                                        )
; only one NS is required for this local file
; and is an out of zone name
      IN      NS      ns1.example.com.
; other IP addresses can be added as required
; this maps 192.168.254.4 as shown
4     IN      PTR     mail.example.com. ; fully qualified domain name (FQDN)
"""

# 3.2.1 Primary Authoritative Name Server
PRIMARY_CONF = """\
// authoritative primary named.conf file
// options clause defining the server-wide properties
options {
  // all relative paths use this directory as a base
  directory "/var";
  // version statement for security to avoid hacking known weaknesses
  // if the real version number is revealed
  version "not currently available";
  // This is the default - allows user queries from any IP
  allow-query { any; };
  // normal server operations may place items in the cache
  // this prevents any user query from accessing these items
  // only authoritative zone data will be returned
  allow-query-cache { none; };
  // Do not provide recursive service to user queries
  recursion no;
};
// logging clause
// log to /var/log/named/example.log all events from info UP in severity (no debug)
// uses 3 files in rotation swaps files when size reaches 250K
// failure messages that occur before logging is established are
// in syslog (/var/log/messages)
//
logging {
  channel example_log {
    // uses a relative path name and the directory statement to
    // expand to /var/log/named/example.log
    file "log/named/example.log" versions 3 size 250k;
    // only log info and up messages - all others discarded
    severity info;
  };
  category default {
    example_log;
  };
};
// Provide forward mapping zone for localhost
// (optional)
zone "localhost" {
  type primary;
  file "master/localhost-forward.db";
  notify no;
};
// Provide reverse mapping zone for the loopback
// address 127.0.0.1
zone "0.0.127.in-addr.arpa" {
  type primary;
  file "localhost.rev";
  notify no;
};
// We are the primary server for example.com
zone "example.com" {
  // this is the primary name server for the zone
  type primary;
  file "example.com";
  // this is the default
  notify yes;
  // IP addresses of secondary servers allowed to
  // transfer example.com from this server
  allow-transfer {
    192.168.4.14;
    192.168.5.53;
  };
};
"""

# The files PRIMARY_CONF names, under its directory "/var".
PRIMARY_FILES = [("/var/example.com", EXAMPLE_COM),
                 ("/var/master/localhost-forward.db", LOCALHOST_FORWARD),
                 ("/var/localhost.rev", LOCALHOST_REV)]

# 3.3.2 Resolver Configuration, with one statement added: the lab root is
# not the root of the internet, the trust anchor built in BIND would refuse
# every answer it gives.
RESOLVER_CONF = """\
// resolver named.conf file
// Two corporate subnets we wish to allow queries from
// defined in an acl clause
acl corpnets {
  192.168.4.0/24;
  192.168.7.0/24;
};

// options clause defining the server-wide properties
options {
  // all relative paths use this directory as a base
  directory "/var";
  // version statement for security to avoid hacking known weaknesses
  // if the real version number is revealed
  version "not currently available";
  // this is the default
  recursion yes;
  // recursive queries only allowed from these ips
  // and references the acl clause
  allow-query { corpnets; };
  // this ensures that any reverse map for private IPs
  // not defined in a zone file will *not* be passed to the public network
  // it is the default value
  empty-zones-enable yes;
  // added for the lab: its root has no DNSSEC key
  dnssec-validation no;
};

// logging clause
// log to /var/log/named/example.log all events from info UP in severity (no debug)
// uses 3 files in rotation swaps files when size reaches 250K
// failure messages that occur before logging is established are
// in syslog (/var/log/messages)
//
logging {
  channel example_log {
    // uses a relative path name and the directory statement to
    // expand to /var/log/named/example.log
    file "log/named/example.log" versions 3 size 250k;
    // only log info and up messages - all others discarded
    severity info;
   };
   category default {
     example_log;
  };
};

// zone file for the root servers
// discretionary zone (see root server discussion above)
zone "." {
  type hint;
  file "named.root";
};

// zone file for the localhost forward map
// discretionary zone depending on hosts file (see discussion)
zone "localhost" {
  type primary;
  file "masters/localhost-forward.db";
  notify no;
};

// zone file for the loopback address
// necessary zone
zone "0.0.127.in-addr.arpa" {
  type primary;
  file "localhost.rev";
  notify no;
};

// zone file for local IP reverse map
// discretionary file depending on requirements
zone "254.168.192.in-addr.arpa" {
  type primary;
  file "192.168.254.rev";
  notify no;
};
"""

# The lab internet of the resolver demos: a root server at the address of
# a.root-servers.net, the com and net servers at that of a.gtld-servers.net,
# and ns1.example.com, the primary of the ARM, for example.com and
# example.net. ns2.example.net, external to example.com in the ARM, is the
# same machine, the server of example.net: a name server nothing answers at
# would slow every first query.
# 3.3.1.1 Root Servers (Hint) Zone File: the named.root of the lab, its one
# root server.
ROOT_HINTS = """\
.                        3600000      NS    A.ROOT-SERVERS.NET.
A.ROOT-SERVERS.NET.      3600000      A     198.41.0.4
"""

# The files RESOLVER_CONF names, under its directory "/var".
RESOLVER_FILES = [("/var/named.root", ROOT_HINTS),
                  ("/var/masters/localhost-forward.db", LOCALHOST_FORWARD),
                  ("/var/localhost.rev", LOCALHOST_REV),
                  ("/var/192.168.254.rev", REV_192_168_254)]

ROOT_ZONE = """\
$TTL 86400
@   IN SOA a.root-servers.net. nstld.verisign-grs.com. 1 1800 900 604800 86400
@   IN NS  a.root-servers.net.
a.root-servers.net. IN A 198.41.0.4
com. IN NS a.gtld-servers.net.
net. IN NS a.gtld-servers.net.
a.gtld-servers.net. IN A 192.5.6.30
"""

COM_ZONE = """\
$TTL 86400
@   IN SOA a.gtld-servers.net. nstld.verisign-grs.com. 1 1800 900 604800 86400
@   IN NS  a.gtld-servers.net.
example.com. IN NS ns1.example.com.
example.com. IN NS ns2.example.net.
ns1.example.com. IN A 192.168.254.2
"""

NET_ZONE = """\
$TTL 86400
@   IN SOA a.gtld-servers.net. nstld.verisign-grs.com. 1 1800 900 604800 86400
@   IN NS  a.gtld-servers.net.
a.gtld-servers.net. IN A 192.5.6.30
root-servers.net. IN NS a.root-servers.net.
a.root-servers.net. IN A 198.41.0.4
example.net. IN NS ns2.example.net.
ns2.example.net. IN A 192.168.254.2
"""

# root-servers.net, held by the root servers themselves, as on the internet:
# the address of a.root-servers.net can be asked like any other.
ROOT_SERVERS_NET = """\
$TTL 86400
@   IN SOA a.root-servers.net. nstld.verisign-grs.com. 1 1800 900 604800 86400
@   IN NS  a.root-servers.net.
a   IN A   198.41.0.4
"""

EXAMPLE_NET = """\
$TTL 2d
@    IN SOA ns2.example.net. hostmaster.example.com. 2003080800 12h 15m 4d 2h
@    IN NS  ns2.example.net.
ns2  IN A   192.168.254.2
mail IN A   192.168.254.25
ftp  IN A   192.168.254.21
"""

def lab_conf(zones):
    """named.conf of a server of the lab internet, authoritative only."""
    out = ['options {', '  directory "/var/cache/bind";', '  recursion no;',
           '  allow-query { any; };', '};']
    for name, path in zones:
        out.append('zone "%s" { type primary; file "%s"; notify no; };'
                   % (name, path))
    return "\n".join(out) + "\n"

def named_start(node, conf, files=()):
    """The files of the example and named.conf written, named.conf checked
    with its zones, the key of rndc made, named started; back once rndc
    says it runs. named is not given -u: it stays uid 0 of the container,
    no setgroups, so it runs with the single uid mapping of a toy too."""
    dirs = {"/etc/bind", "/var/log/named", "/var/cache/bind", "/run/named"}
    dirs.update(path.rsplit("/", 1)[0] for path, _ in files)
    ssh(node, "mkdir -p %s" % " ".join(sorted(dirs)))
    for path, text in files:
        put_any(node, path, text)
    put_any(node, NAMED_CONF, conf)
    out = ssh(node, "named-checkconf -z %s 2>&1; echo checkconf=$?"
              % NAMED_CONF, check=False)
    if "checkconf=0" not in out:
        raise RuntimeError("named.conf of %s refused:\n%s" % (node, out))
    ssh(node, "test -s /etc/bind/rndc.key || rndc-confgen -a")
    ssh(node, "named -c %s" % NAMED_CONF)
    if not until(lambda: "up and running" in rndc(node, "status"), 30, 0.5):
        # named reports to syslog once started: in the foreground, the reason
        # goes to the terminal.
        raise RuntimeError("named did not start on %s:\n%s" % (node, ssh(
            node, "timeout 5 named -g -c %s 2>&1 | tail -n 20" % NAMED_CONF,
            check=False)))

def named_stop(node):
    """named stopped by rndc, back once its process is gone (a zombie left
    to an init that does not reap counts as gone)."""
    rndc(node, "stop")
    if not until(lambda: not ssh(node, "ps -C named -o stat= | grep -v Z",
                                 check=False).strip(), 20, 0.5):
        raise RuntimeError("named did not stop on %s" % node)

def rndc(node, cmd):
    """One rndc command, its output; never raises."""
    return ssh(node, "rndc %s 2>&1" % cmd, check=False)

def dig(node, args):
    """One dig command, its output; never raises."""
    return ssh(node, "dig %s 2>&1" % args, check=False)

def status_of(out):
    """The status of a dig answer, NOERROR, NXDOMAIN, REFUSED...; '' when
    no answer came."""
    m = re.search(r"status: ([A-Z]+)", out)
    return m.group(1) if m else ""

def flags_of(out):
    """The flags of a dig answer: qr aa rd ra ad..."""
    m = re.search(r";; flags: ([a-z ]*);", out)
    return m.group(1).split() if m else []

def answer_of(out):
    """The records of the ANSWER SECTION of a dig output, one per line."""
    m = re.search(r";; ANSWER SECTION:\n(.*?)(?:\n\n|\Z)", out, re.S)
    return m.group(1).strip() if m else ""

def brief(out):
    """The status, the flags and the answer of a dig output."""
    keep = [l for l in out.split("\n")
            if l.startswith(";; ->>HEADER") or l.startswith(";; flags:")]
    ans = answer_of(out)
    return "\n".join(keep + ([ans] if ans else []))

def log_of(node, path="/var/log/named/example.log"):
    """The log file of the ARM, as named has written it so far."""
    return ssh(node, "cat %s 2>/dev/null" % path, check=False)

def internet_start(root, gtld, ns1):
    """The lab internet: the root zone and root-servers.net on root, com
    and net on gtld, example.com (the ARM base file) and example.net on
    ns1."""
    servers = {
        root: [(".", "/var/cache/bind/root.zone", ROOT_ZONE),
               ("root-servers.net", "/var/cache/bind/root-servers.net",
                ROOT_SERVERS_NET)],
        gtld: [("com", "/var/cache/bind/com.zone", COM_ZONE),
               ("net", "/var/cache/bind/net.zone", NET_ZONE)],
        ns1: [("example.com", "/var/cache/bind/example.com", EXAMPLE_COM),
              ("example.net", "/var/cache/bind/example.net", EXAMPLE_NET)]}

    def one(node):
        zones = servers[node]
        named_start(node, lab_conf([(z, p) for z, p, _ in zones]),
                    [(p, t) for _, p, t in zones])
    in_parallel(one, [root, gtld, ns1], "internet_start")

def icmp_in_echos(node):
    """The echo requests a machine has received, from /proc/net/snmp."""
    out = ssh(node, "grep '^Icmp:' /proc/net/snmp", check=False).split("\n")
    try:
        names, values = out[0].split()[1:], out[1].split()[1:]
        return int(dict(zip(names, values))["InEchos"])
    except (IndexError, KeyError, ValueError):
        return 0
