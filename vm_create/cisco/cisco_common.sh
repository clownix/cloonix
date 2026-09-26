# Sourced by the three steps.
BULK="/var/lib/cloonix/bulk"
NAME="c8000v"
QCOW2="${BULK}/${NAME}.qcow2"
# Virtual size only: the qcow2 file grows with what is written, and an
# installed c8000v uses a few gigabytes of it.
DISK_SIZE_G=60
RAM=6000
CPU=4
WORK="/tmp/cisco_dayzero"
DAYZERO_ISO="${WORK}/iosxe_config.iso"
CLOONFS="/usr/libexec/cloonix/cloonfs"
QEMU_BIN="${CLOONFS}/bin/cloonix-qemu-system"
QEMU_DIR="${CLOONFS}/bin/qemu"
XORRISO="${CLOONFS}/bin/xorriso"
UUID="1c54ff10-774c-4e63-9896-4c18d66b50b1"
VNC="127.0.0.1:10"
cisco_qemu() {
  local cdrom="$1"
  local boot="$2"
  ${QEMU_BIN} \
     -L ${QEMU_DIR} \
     -enable-kvm -no-reboot -nodefaults \
     -machine q35,accel=kvm,usb=off,vmport=off,hpet=off \
     -cpu host -m ${RAM} -smp ${CPU} \
     -uuid ${UUID} \
     -drive file=${QCOW2},id=hd,if=none,cache=none \
     -device virtio-scsi-pci,id=scsi \
     -device scsi-hd,drive=hd \
     ${cdrom:+-cdrom ${cdrom}} \
     -boot ${boot} \
     -device VGA -vnc ${VNC} \
     -serial stdio
}
cisco_check_tools() {
  [ -x ${QEMU_BIN} ] || {
    echo "${QEMU_BIN} not found, install cloonix"; exit 1; }
  [ -w /dev/kvm ] || {
    echo "/dev/kvm not writable by $(id -un)"; exit 1; }
}
