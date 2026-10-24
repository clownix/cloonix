# FOSDEM talk proposal: cloonix

## Submission fields

**Title**
cloonix: one tarball, a whole network — VMs, containers and wifi in the same lab

**Suggested tracks**
Network devroom (first choice), Virtualization and Cloud Infrastructure devroom (second choice), Lightning talks (fallback, condensed version below).

**Format**
Talk, 25 minutes (20 minutes of content plus 5 minutes of questions), with a live demo.

**Abstract (short, for the schedule page)**
Building a realistic network lab on Linux usually means installing a pile of host packages, choosing between VMs and containers, and giving up on wifi. cloonix takes another path: everything it runs, from QEMU and Open vSwitch to crun, passt and dropbear, ships inside its own tree, so nothing comes from the host but the kernel and python3 for the client. In one topology you can mix KVM guests (Linux, FreeBSD, Windows, Cisco IOS-XE) and lightweight containers, cable them through lans, give them internet through a passt NAT, emulate wifi with real mac80211_hwsim radios, and link labs running on different hosts. This talk shows how it is built, what that design costs, and runs a live lab from an empty canvas to a wifi ping between a VM and a container.

**Description (long)**
cloonix is an open-source Linux network virtualization platform. Its guiding rule is that the user should never fight the tool: the software repairs what it can, accepts as many user mistakes as it can, and depends on nothing installed on the host.

The talk covers:

- **The self-contained design.** Every binary and library cloonix uses lives under its own tree. The same release runs unchanged on Alpine, Arch, Fedora, Rocky, openSUSE Tumbleweed, Debian trixie and Ubuntu, which an automated test bench checks on each distro. A "toy" variant runs entirely as a normal user, with no root install at all.
- **One lab, two kinds of guests.** KVM machines and crun containers sit in the same topology and are cabled the same way, through Open vSwitch lans. Containers start in a fraction of a second, so labs of dozens of nodes fit on a laptop; VMs give you real kernels and other operating systems.
- **Wifi that behaves like wifi.** Wireless interfaces use mac80211_hwsim radios, and cloonix acts as the radio medium. Airs are named like lans, so several isolated wireless networks can coexist, and an air can hold radios from both containers and VMs. Captures of an air show real 802.11 frames.
- **The plumbing.** passt gives guests dual-stack internet without privileges, with docker-style inbound port publishing. Labs on different hosts are joined with cloonix-to-cloonix links. A packet-loss, delay and rate shaper sits on any link.
- **Seeing what happens.** A browser GUI draws the topology, opens consoles and desktops, and captures traffic in a built-in browser packet viewer. Every action in the GUI corresponds to a plain command line, and the GUI can print the commands that rebuild the current lab.
- **Teaching.** cloonix ships a course of demos organised in packages, from the cloonix items themselves to routing (FRR, BIRD), security (IPsec), DNS and a whole wifi package. Each demo is a short python script that prints every command it runs, so students learn the low-level commands rather than a black box.
- **Testing a tool that must not fail.** Non-regression suites, randomised soak tests, fault injection and saturating flows are used to find leaks and races; the talk shares a few of the bugs they caught.

The audience is anyone who builds network labs: network engineers, students and teachers, developers who need reproducible multi-node test setups, and people working on wireless.

**Speaker bio**
[Your name], known as clownix, is the developer and maintainer of cloonix. He works across the whole stack, from the C daemons that drive QEMU, Open vSwitch and crun to the JavaScript web interface.

**Links to include in the submission**
- Project site: https://clownix.net
- Source repository: [add the URL]
- Short screencast of the web GUI: [add the URL — strongly recommended, reviewers watch these]

---

## Talk plan (25 minutes)

| Time | Section | Content |
|------|---------|---------|
| 0:00–2:00 | The problem | Why lab tools hurt: host dependencies, "VMs or containers", no wifi, labs that do not survive a distro upgrade. |
| 2:00–5:00 | The cloonix answer | One tree, nothing from the host. The two deliveries: the bundle and the rootless toy. The distro list proven by the test bench. |
| 5:00–8:00 | Architecture | Server, Open vSwitch lans, KVM and crun guests, passt NAT, c2c links, the single suid helper, the TLS command port and the password-protected https GUI. One diagram. |
| 8:00–15:00 | Live demo | See the demo plan below. |
| 15:00–17:30 | Wifi in depth | hwsim radios, cloonix as the medium, named airs, mixed VM/container airs, 802.11 capture. |
| 17:30–19:30 | Teaching and testing | The demo course for students; the soak and stress suites and two or three bugs they found. |
| 19:30–20:00 | Call to action | Try the toy, run a demo, send feedback. Project URL on screen. |
| 20:00–25:00 | Questions | |

## Live demo plan

Prepare everything beforehand: images in /var/lib/cloonix/bulk, mac80211_hwsim loaded with modprobe, the net already started, the browser logged in to the https GUI. Keep a pre-recorded video of the same demo as a fallback in case the venue network or the laptop misbehaves.

1. Show the empty canvas of the running net.
2. Run the simplest wifi demo from the Demo panel (two containers pinging over the air), so the room sees the topology appear, the Comments panel explain it and the ping succeed.
3. Run the container-to-VM wifi demo, then double-click the air to open the packet viewer and show 802.11 frames, using the flip-flop button to filter down to the IP packets.
4. Open the nat demo: a published port opened in a browser tab, then a ping to the internet from inside the guest.
5. Click "Commands" to show the plain command lines that rebuild what is on screen. Point out that this is what the demo scripts contain.
6. Delete all with one click.

Rehearse with a timer: the demo must fit in seven minutes, including the speaking.

## Lightning-talk version (5 minutes)

1. One slide: the problem.
2. One slide: one tarball, nothing from the host, VMs and containers and wifi together.
3. Ninety seconds of pre-recorded demo: wifi ping between a VM and a container, the packet viewer on the air.
4. One slide: try the toy, project URL.

## Before submitting

- Check the FOSDEM website for the list of accepted devrooms and each devroom's own call for participation and deadline; they are published separately and the deadlines differ.
- Verify that every capability claimed in the abstract is in the release that will be current in February.
- Record the screencast and add its link; it noticeably improves acceptance chances.
