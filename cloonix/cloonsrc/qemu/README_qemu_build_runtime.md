# Cloonix QEMU — build / runtime coupling

Two files work together and must stay consistent:

| File | Role | When it acts |
|------|------|--------------|
| `cmd` | Minimal **build** script (configure + device allow-list) | At compile time — decides what is *inside* the binary |
| `qemu_automaton.c` | **Runtime** command-line generator (`create_linux_cmd_kvm`, `ovs_format_ethv`) | At VM launch — decides what flags are *passed to* the binary |

**The coupling rule:** every `-device` / `-netdev` / `-drive` / `-object` / `-machine`
feature emitted at runtime must have its backing feature compiled into the binary.
Because `cmd` uses `--without-default-features` **and** `--without-default-devices`,
nothing is present unless explicitly enabled. Add a runtime device that is not in
the allow-list and the build still succeeds, but the VM dies at launch with
`-device X: no such device` (or `'driver' is not a valid ... ` for blockdev).

---

## Runtime flag → build option it depends on

### CPU / machine / accel
| Runtime (qemu_automaton.c) | Needs in `cmd` |
|---|---|
| `-enable-kvm`, `accel=kvm`, `-cpu host,+kvm-pv-*` | `--enable-kvm` (KVM paravirt features are core, always present) |
| `-machine q35,...` (q35 platform, SATA, LPC) | `CONFIG_Q35`, `CONFIG_PCI_EXPRESS_Q35`, `CONFIG_LPC_ICH9`, `CONFIG_AHCI*` |
| `vmport=off`, `hpet=off`, `usb=off` | q35 platform only (no extra flag) |
| `-global kvm-pit.lost_tick_policy=discard` | i8042/PIT kept on (do NOT add `i8042=off`/PIT-off) |
| `-smp sockets=1,cores=N,threads=1` | core, no flag |

### Display / input (VNC only — no GTK/SDL/SPICE)
| Runtime | Needs in `cmd` |
|---|---|
| `-vnc unix:%s` | `--enable-vnc` (+ ships `pc-bios/keymaps`) |
| `-device VGA` | `CONFIG_VGA`, `CONFIG_VGA_PCI` (+ `vgabios-stdvga.bin`) |
| `-device qemu-xhci` | `CONFIG_USB`, `CONFIG_USB_XHCI*` |
| `-device usb-tablet` | `CONFIG_HID` |
| default PS/2 keyboard (VNC) | i8042 left enabled — **option 1** |

> If you ever set `-display gtk`/`sdl`/`-spice`, you must re-enable those in `cmd`
> (currently off through `--without-default-features`). Don't — VNC is the design.

### Network (vhost tap, virtio)
| Runtime (ovs_format_ethv) | Needs in `cmd` |
|---|---|
| `virtio-net-pci-non-transitional` | `CONFIG_VIRTIO_NET`, `CONFIG_VIRTIO_PCI` |
| `-netdev type=tap,vhost=on` | `--enable-vhost-net`, `--enable-vhost-kernel` |

### virtio-serial / agent / misc
| Runtime (QEMU_OPTS_CLOON) | Needs in `cmd` |
|---|---|
| `virtio-serial-pci`, `virtserialport` | `CONFIG_VIRTIO_SERIAL` |
| `virtio-balloon-pci` | `CONFIG_VIRTIO_BALLOON` |
| `rng-random` + `virtio-rng-pci` | `CONFIG_RNG_RANDOM`, `CONFIG_VIRTIO_RNG` |

### Block / storage
| Runtime macro | Devices/drivers | Needs in `cmd` |
|---|---|---|
| `BLOCKNAME` | `virtio-scsi-pci`, `scsi-hd`, blockdev `file`+`qcow2` | `CONFIG_VIRTIO_SCSI`, `CONFIG_SCSI_HD` |
| `DRIVE_PARAMS` (`if=virtio`) | `virtio-blk` | `CONFIG_VIRTIO_BLK` |
| `DRIVE_PARAMS_CISCO` | `virtio-scsi-pci`, `scsi-hd` | `CONFIG_VIRTIO_SCSI`, `CONFIG_SCSI_HD` |
| `DRIVE_FULL_VIRT` (`ide-hd,bus=ide.0`) | `ide-hd` on q35 SATA | `CONFIG_AHCI*`, `CONFIG_IDE_*` |
| `AGENT_CDROM` / `INSTALL_DISK` / `ADDED_CDROM` | `scsi-cd` + `media=cdrom` | `CONFIG_SCSI_CD`, `CONFIG_IDE_*` (ATAPI) |
| qcow2 overlay creation (cprootfs_clone) | none - done in `qcow2_hdr.c`, no qemu binary involved |

> `aio=io_uring` (added to drives) needs `--enable-linux-io-uring` ✓ and host
> kernel ≥ 5.1. `cache=none` / `cache.direct=on` needs the backing FS to support
> O_DIRECT (ext4/xfs/btrfs OK). Fallbacks: `aio=threads`, `cache=writeback`.

---

## Quick consistency checklist (run after editing either file)

1. Did you add/remove a `-device NAME` in `qemu_automaton.c`?
   → make sure the matching `CONFIG_*` is in the `DEVICE_OPTS` allow-list in `cmd`
   (or removed from it). Rebuild.
2. Did you add a new block driver / aio mode at runtime?
   → confirm it isn't in the `--disable-*` list in `cmd`.
3. Did you add a new display/char backend?
   → re-enable the corresponding `--enable-*` in `cmd`.
4. Smoke test: launch one VM, then
   `sudo cat /var/log/user.log | grep qemu`
   (the code prints this hint on unexpected stop). A `no such device` /
   `not a valid driver` line means the build and runtime drifted.

## If `cmd` device allow-list is rejected by your QEMU version
The `-DCONFIG_*=y` meson switches are version-sensitive. If `./configure`
rejects them on your tree: drop the whole `DEVICE_OPTS` block and the
`--without-default-devices` flag, then prune
`configs/devices/x86_64-softmmu/default.mak` by hand before `make`, keeping only
the CONFIG_* rows listed in the tables above. Same target binary, different
mechanism.

---

## Dead-code trimming (what `cmd` switches off, and why it is safe)

`--without-default-features` only forces the meson options whose default is
`auto`. Five options cloonix never uses have a hard default and survive it, so
`cmd` turns them off by hand:

| Option | What leaves the binary | Why cloonix cannot need it |
|---|---|---|
| `--disable-tcg` | `tcg/`, `accel/tcg/`, `target/i386/tcg/` (incl. `translate.c`), `fpu/softfloat.c`, `disas/`, `replay/`, `block/blkreplay.c`, `net/filter-replay.c` | `create_linux_cmd_kvm()` always emits `-enable-kvm` and `accel=kvm`; there is no non-KVM path anywhere in the tree |
| `--disable-fdt` | bundled `dtc` subproject | device tree is ARM/PPC, never read on q35 |
| `--disable-qom-cast-debug` | the runtime cast check inside every `OBJECT_CHECK` | debug aid only |
| `--enable-trace-backends=nop` | the hook body of every tracepoint | cloonix reads syslog, not qemu traces |
| `--audio-drv-list=` | the audio backend list | no sound card is ever put on the cmdline |

Measured on the x86_64-softmmu link: **588 objects / ~404600 source lines
before, 527 objects / ~351600 after** — about 53000 lines of source that used
to be compiled into `cloonix-qemu-system` and never executed. Both trees were
built and the stripped binaries compared:

| binary | before | after | gain |
|---|---|---|---|
| `cloonix-qemu-system` | 9483408 | 7057720 | -25.6 % |

## Link-time garbage collection

`cmd` compiles with `-ffunction-sections -fdata-sections` and links with
`-Wl,--gc-sections`. Every function and every table lands in its own section
and the linker drops whatever no root reaches. `.init_array` is a linker root,
so the `type_init()` constructors that register QOM types are never collected -
verified by listing every device cloonix emits on the trimmed binary.

## Two dead ui objects

`ui/meson.build` loses `input-barrier.c` (the Barrier KVM-switch client, an
`-object input-barrier` backend) and `input-linux.c` (host evdev passthrough,
`-object input-linux`). Neither is ever named on the cloonix cmdline.

They matter far beyond their own size: `input-barrier.c` was the last reference
to `qemu_input_map_x11_to_linux`, a 131072-byte table, plus
`qemu_input_map_xorgkbd_to_linux`. Once the file goes, `--gc-sections` reclaims
those too. Same effect had already removed the osx / win32 / xorgxquartz /
xorgxwin / sun / atset1-to-linux tables, whose only callers (`ui/gtk.c`,
`ui/x_keymap.c`, `hw/char/escc.c`) are not compiled in this configuration.

## Relocations and unwind tables

Sections of the binary after the cuts above, before this step:

    .text          2874272
    .rela.dyn      1002072     <- PIE relocation records
    .data           683816
    .eh_frame       586864     <- unwind tables
    .rodata         652033
    .data.rel.ro    304000     <- PIE again

Nearly a third of the file was neither code nor data. `.rela.dyn` holds one
relocation record per pointer in every static table, and QEMU is built almost
entirely out of static tables (QOM `TypeInfo`, `VMStateDescription`, property
lists, QMP dispatch). `.eh_frame` is the stack-unwind table; nothing here
throws, and cloonix diagnoses through syslog, not backtraces.

`--disable-pie` (plus `-no-pie` in LDFLAGS - configure alone does not stop
meson adding `-pie` at link time) and `-fno-asynchronous-unwind-tables` bring
those to:

    .rela.dyn          168
    .eh_frame          240
    .data.rel.ro         8

The price of `--disable-pie` is that the binary image is no longer
address-space randomised. Both flags are isolated in `cmd` and revert
independently.

## Cumulative result

| step | qemu-system |
|---|---|
| starting point | 9483408 |
| + five configure options, ten block drivers | 7057720 |
| + `--gc-sections` | 6445112 |
| + drop input-barrier / input-linux | 6273080 |
| + no-pie, no unwind tables | 4495344 |
| **total** | **-52.6 %** |

## The blobs are bigger than what is left of the binary

`cmd` used to copy 1599657 bytes of `pc-bios` next to a 4495344-byte binary.
Two of those blobs can never be requested, because the only devices that name
them are not compiled:

| blob | requester | compiled? |
|---|---|---|
| `efi-e1000.rom` 159232 | `hw/net/e1000.c` | no |
| `vgabios-virtio.bin` 39424 | `hw/display/virtio-vga.c` | no |
| `efi-virtio.rom` 160768 | `hw/virtio/virtio-net-pci.c` | yes |
| `vgabios-stdvga.bin` 39424 | `hw/display/vga-pci.c` | yes |
| `kvmvapic.bin` 9216 | `hw/i386/vapic.c` | yes |
| `bios-256k.bin` 262144 | firmware | yes |

And `pc-bios/keymaps` was shipped whole: 35 files, 929449 bytes. A keymap file
is read only when `-k` is on the cmdline - `system/vl.c:3178` is the only place
that assigns `keyboard_layout`, and the other assigner, `ui/curses.c`, is not
compiled. cloonix never passes `-k`, so `en-us` (27086) alone is kept as a
fallback.

Blob payload 1599657 -> 337472.

## What is left, and what it would cost

The binary is now 2757152 of `.text`, 977968 of `.rodata`, 683816 of `.data`.

`-Os` instead of `-O2` was measured on this tree: `qemu-system` 4495344 ->
3835888, `.text` alone dropping 22 %. It is not
enabled in `cmd` because it is the first change that trades runtime speed for
size, and the block datapath (virtio-blk/scsi, qcow2, io_uring) runs inside
qemu. Append `-Os` to `CFLAGS` if a disk benchmark inside a guest says it is
free.

Cutting migration, HMP, gdbstub, NBD and audio (the table below) is worth
roughly another 400000-500000 bytes, i.e. about 10 %, for a great deal more
risk than anything above.

`cmd` builds only `qemu-system-x86_64`. Plain `make` would additionally build
`qemu-img`, `qemu-io`, `qemu-nbd`, `qemu-storage-daemon`, `qemu-pr-helper`,
`qemu-vmsr-helper`, `qemu-bridge-helper` and `qemu-edid`, none of which is
shipped.

### `hw/virtio/vhost.c` in `cloonix_patch`

Upstream `do_vhost_virtqueue_stop()` asserts on
`vhost_user_has_protocol_feature()` when asked to skip the drain, with no
`#ifdef`: the symbol lives in `vhost-user.c`, which a build without
vhost-user never compiles, so the link of `qemu-system-x86_64` fails on
that one reference. The hunk wraps the assert in `CONFIG_VHOST_USER`; the
skip is only ever asked for by a vhost-user backend, so the other branch is
`g_assert_not_reached()`. Drop the hunk when upstream guards it itself.

### Block drivers dropped in `cloonix_patch`

`block/meson.build` loses ten drivers. All of them are pure self-registering
`BlockDriver`s: each was compiled and `nm -g` showed **zero** exported symbols,
so nothing in the tree can reference them and the link is unaffected.

`blkdebug` `blklogwrites` `blkverify` `filter-compress` `null` `preallocate`
`quorum` `snapshot-access` `throttle` `nvme`

Two neighbours had to stay: `block/crypto.c` exports
`block_crypto_{open,create,amend}_opts_init`, called from `qcow2.c`, and
`block/copy-on-read.c` exports `bdrv_cor_filter_drop`, called from
`block/stream.c`.

### Still dead, not yet cut, in priority order

These are dead for cloonix too but none of them can be removed without editing
`system/vl.c`, `blockdev.c` or `monitor/monitor.c` to stub the callers, so each
needs its own build-and-boot test round:

| Subsystem | Lines still compiled in |
|---|---|
| `migration/` | 28400 |
| NBD client + server (`nbd/`, `block/nbd.c`, `blockdev-nbd.c`, `block/export/`) | 8600 |
| HMP command tables (`*-hmp-cmds.c`, `monitor/hmp*.c`) | 7800 |
| block jobs (mirror, backup, commit, stream, block-copy) | 5800 |
| `audio/` core | 4100 |
| `gdbstub/` | 3900 |
| `dump/` (`dump-guest-memory`) | 3500 |
| unused chardev backends (pty, ringbuf, udp, parallel, msmouse, wctablet, testdev...) | 3500 |
| unused net backends (socket, stream, dgram, hub, filter-*) | 3200 |
| `qemu-io-cmds.c` inside the system binary | 2900 |
| qtest | 1200 |

### Checking a trimmed binary without booting a VM

    ./cloonix-qemu-system -accel help          # must print kvm only
    ./cloonix-qemu-system -M help              # must list q35
    for d in VGA qemu-xhci usb-tablet virtio-net-pci-non-transitional \
             virtio-scsi-pci scsi-hd scsi-cd ide-hd ide-cd virtio-blk-pci \
             virtio-serial-pci virtserialport virtio-balloon-pci \
             virtio-rng-pci ; do
      ./cloonix-qemu-system -device $d,help >/dev/null || echo "MISSING $d"
    done

---

## The qcow2 overlay is written in C

`cmd` builds no image tool. Creating the overlay of a new guest and reading
its backing file back are `uml_cloonix_switch/src/qcow2_hdr.c`, about 200
lines:

    int qcow2_create_derived(char *backing, char *used);
    int qcow2_get_backing(char *path, char *backing, int len);

A fresh overlay is a fixed qcow2 v3 layout - cluster 0 header plus the
backing-format extension plus the backing path, cluster 1 refcount table,
cluster 2 refcount block, clusters 3.. the all-zero L1 table. Virtual size is
copied from the backing image header (file size if the backing is not qcow2).
`qcow2_get_backing` is a header read plus one pread.

The writer was validated at 8 GiB, 512 GiB and 8 TiB - the last needs an L1
table spanning several clusters - for header fields, cluster allocation on
write, and byte-identical readback of 8 MiB of random data through a
generated overlay.

## Final payload

| | bytes |
|---|---|
| starting point | 12911209 |
| after everything above | 4794734 |
| **saved** | **-62.9 %** |

`cloonix-qemu-system` 4495344, blobs 299424 (bios-256k.bin, kvmvapic.bin,
vgabios-stdvga.bin, keymaps/en-us). One binary and four blobs.
