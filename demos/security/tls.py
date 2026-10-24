#!/usr/bin/env python3
r"""tls - the same page over http and over https, read on the wire.

    ./tls.py [net]                 the net defaults to nemo

A client, a web server, one lan, and a sniffer on the server side::

    client ---- lan1 ---- web
    10.0.0.1              10.0.0.2   :8080 http   :8443 https

  web serves one page holding a string. It makes its own key and its own
  certificate with openssl when the demo runs.

  ACT 1  the page over http. The string is in the bytes caught on the lan.
  ACT 2  the client takes the certificate of web and trusts it, then
         fetches the page over https. Same page, same lan: the string is
         not in the bytes.
  ACT 3  the client forgets the certificate. curl refuses the connection:
         encryption without knowing who answers protects nothing.

Image: openvpn_trixie (openssl, curl, python3)
Size:  2 containers, 1 lan

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf web eth0 sniff=on
    cloonix cli nemo sniff web eth0 --dump tcp port 8080
    cloonix cli nemo sniff web eth0 --dump tcp port 8443

  On 8080 the packet to catch is the reply, the page readable in it.
  On 8443 it is the first one of the client, the ClientHello: the last
  readable thing of the connection, the versions and ciphers it offers.

Important
  Nothing is baked into the image: the key and the certificate are made at
  run time and no key is ever in a tarball.
"""
import os
import sys
import time

# What every demo shares is in demos/lib/demolib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import (
    NET, build, check_net, choose_rootfs, erase_net, grab_on, hold, place,
    put, ssh, step, story, tools_present)

HISTORY = """\
SSL was written at Netscape in 1994 by Taher Elgamal and his team so that a
card number could be typed into a browser; SSL 3.0, by Paul Kocher, Phil
Karlton and Alan Freier in 1996, is the design everything after it kept.
The IETF renamed it TLS in 1999. Eric Young and Tim Hudson wrote SSLeay in
Australia from 1995, a free implementation outside the export rules of the
United States; in 1998 it became OpenSSL. TLS 1.3, RFC 8446 by Eric Rescorla
in 2018, removed every old cipher and encrypts the certificate itself.

Today: OpenSSL is in nearly every server of the internet, and more than
nine pages out of ten load over https, a share that Let's Encrypt, giving
certificates for free since 2015, took from under a half.
"""

ROOTFS_CHOICES = ("openvpn_trixie",)

CLIENT, WEB = "client", "web"
ALL = (CLIENT, WEB)
LAN = "lan1"

WIRING = {
    CLIENT: [(0, LAN, "10.0.0.1/24")],
    WEB:    [(0, LAN, "10.0.0.2/24")],
}
GATEWAY = {}
NEEDLE = b"SECRET-STRING-THAT-SHOULD-NOT-BE-READABLE"

PAGE_SERVER = '''import http.server, socketserver, ssl, sys
class H(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        body = b"SECRET-STRING-THAT-SHOULD-NOT-BE-READABLE\\n"
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)
    def log_message(self, *a):
        pass
socketserver.TCPServer.allow_reuse_address = True
srv = socketserver.TCPServer(("", int(sys.argv[1])), H)
if len(sys.argv) > 2:
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain("/tmp/cert.pem", "/tmp/key.pem")
    srv.socket = ctx.wrap_socket(srv.socket, server_side=True)
srv.serve_forever()
'''

LAYOUT = {CLIENT: (-300, 0), LAN: (0, 0), WEB: (300, 0)}
ETHS = {(CLIENT, "eth0"): 90, (WEB, "eth0"): 270}

CARD = r"""
    client ---- lan1 ---- web
    10.0.0.1              10.0.0.2   :8080 http   :8443 https

  One page, fetched twice, the lan sniffed both times.
"""

def web_start():
    ssh(WEB, "openssl req -x509 -newkey rsa:2048 -nodes -days 30 "
             "-keyout /tmp/key.pem -out /tmp/cert.pem -subj /CN=web "
             "-addext subjectAltName=IP:10.0.0.2 2>/dev/null")
    put(WEB, "/tmp/page.py", PAGE_SERVER)
    out = ssh(WEB, "python3 -m py_compile /tmp/page.py 2>&1", check=False)
    if out.strip():
        raise RuntimeError("the page server did not survive being written:\n%s"
                           % out)
    ssh(WEB, "pkill -f /tmp/page.py 2>/dev/null; true", check=False)
    ssh(WEB, "python3 /tmp/page.py 8080 </dev/null >/tmp/http.log 2>&1 &")
    ssh(WEB, "python3 /tmp/page.py 8443 tls </dev/null >/tmp/https.log 2>&1 &")
    for _ in range(20):
        if ssh(CLIENT, "nc -z -w1 10.0.0.2 8080 && nc -z -w1 10.0.0.2 8443 "
                       "&& echo up", check=False).strip():
            return
        time.sleep(0.5)
    raise RuntimeError("the page servers did not start:\n%s"
                       % ssh(WEB, "cat /tmp/http.log /tmp/https.log",
                             check=False))

def report(got, port):
    if got is None:
        story("    (could not sniff web eth0; the bytes are not shown)")
        return None
    found = NEEDLE in got["data"]
    story("    %d packet(s), %d bytes on port %d; the string is %s"
          % (got["count"], len(got["data"]), port,
             "IN them, readable" if found else "NOT in them"))
    return found

def main():
    story(HISTORY)
    step("checking the server")
    check_net()
    step("looking for the image")
    rootfs = choose_rootfs()
    erase_net()
    place()
    story(CARD)
    story("  image: %s" % rootfs)

    step("creating and cabling the two machines")
    build(rootfs)
    tools_present(WEB, "openssl", "python3")
    tools_present(CLIENT, "openssl", "curl", "nc")

    step("web makes a key and a certificate, and serves the page twice")
    web_start()
    story()
    story("    the certificate web signed for itself:")
    story(ssh(WEB, "openssl x509 -in /tmp/cert.pem -noout -subject -dates "
                   "-ext subjectAltName", check=False).rstrip())

    step("act 1: http, the page crosses the lan as it is")
    story()
    got = grab_on(WEB, 0, "tcp port 8080",
                  lambda: ssh(CLIENT, "curl -sS -m 5 http://10.0.0.2:8080/",
                              check=False))
    page = got["out"].strip() if got else ""
    story("    client received: %s" % page)
    plain_seen = report(got, 8080)
    hold()

    step("act 2: https, the client trusting the certificate of web")
    story()
    ssh(CLIENT, "openssl s_client -connect 10.0.0.2:8443 </dev/null "
                "2>/dev/null | openssl x509 > /tmp/web.pem")
    story("    the client took the certificate over the lan and keeps it as")
    story("    the one it trusts; a browser holds a list of authorities")
    story("    instead, and web would show a certificate one of them signed.")
    story()
    got = grab_on(WEB, 0, "tcp port 8443",
                  lambda: ssh(CLIENT, "curl -sS -m 5 --cacert /tmp/web.pem "
                                      "https://10.0.0.2:8443/", check=False))
    page_tls = got["out"].strip() if got else ssh(
        CLIENT, "curl -sS -m 5 --cacert /tmp/web.pem https://10.0.0.2:8443/",
        check=False).strip()
    story("    client received: %s" % page_tls)
    tls_seen = report(got, 8443)
    story()
    story("    what the two ends agreed on:")
    story(ssh(CLIENT, "curl -sv -m 5 --cacert /tmp/web.pem -o /dev/null "
                      "https://10.0.0.2:8443/ 2>&1 | grep -i 'SSL connection'",
              check=False).rstrip())
    hold()

    step("act 3: the client trusting another certificate refuses the server")
    story()
    ssh(CLIENT, "openssl req -x509 -newkey rsa:2048 -nodes -days 30 "
                "-keyout /dev/null -out /tmp/other.pem -subj /CN=other "
                "2>/dev/null")
    out = ssh(CLIENT, "curl -sS -m 5 --cacert /tmp/other.pem "
                      "https://10.0.0.2:8443/ 2>&1; "
                      "echo curl exit $?", check=False)
    story(out.rstrip())
    refused = "exit 60" in out
    story()
    story("    exit 60: the certificate is signed by nobody the client knows.")
    story("    Anybody on the lan can answer on 8443 with a certificate of")
    story("    his own; the check of the signature is what tells web from him.")
    hold()

    wanted = NEEDLE.decode()
    if page != wanted or page_tls != wanted:
        sys.exit("the page did not arrive: http=%r https=%r" % (page, page_tls))
    if plain_seen is not True or tls_seen is not False or not refused:
        sys.exit("a check failed: string on 8080=%s, on 8443=%s, refused=%s"
                 % (plain_seen, tls_seen, refused))

    story("""
the network is left up. Things to try:

  the handshake, certificate and ciphers, as openssl sees them:
    cloonix cli %s ssh client "openssl s_client -connect 10.0.0.2:8443 </dev/null"

  the bytes of both fetches, side by side in two terminals:
    cloonix cli %s sniff web eth0 --dump tcp port 8080
    cloonix cli %s sniff web eth0 --dump tcp port 8443
    cloonix cli %s ssh client "curl -s http://10.0.0.2:8080/"
    cloonix cli %s ssh client "curl -s --cacert /tmp/web.pem https://10.0.0.2:8443/"

  an old protocol version, refused:
    cloonix cli %s ssh client "curl -sS --tls-max 1.1 --cacert /tmp/web.pem https://10.0.0.2:8443/"
""" % (NET, NET, NET, NET, NET, NET))

main()
