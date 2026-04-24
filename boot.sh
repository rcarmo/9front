#!/bin/bash
# Boot 9front arm64 in QEMU with optional port disk.
#
# Usage:
#   ./boot.sh                  # basic boot (snapshot mode)
#   ./boot.sh persistent       # writes survive reboot
#   ./boot.sh dev              # dev mode: rebuild port disk + attach
#
# In dev mode, the port disk appears as /dev/sdG0 inside 9front:
#   dossrv -f /dev/sdG0/data portdisk
#   mount /srv/portdisk /n/port
#   mkdir -p /sys/src/9/a733
#   dircp /n/port/a733 /sys/src/9/a733
#   cd /sys/src/9/a733 && mk

set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
IMAGES="$SCRIPT_DIR/images"
BIOS="$IMAGES/u-boot.bin"
DISK="$IMAGES/9front-11677.arm64.qcow2"
PORTDISK="$IMAGES/portdisk.img"

MODE="${1:-snapshot}"

[ -f "$BIOS" ] || { echo "Missing: $BIOS"; exit 1; }
[ -f "$DISK" ] || { echo "Missing: $DISK"; exit 1; }

SNAPSHOT="" EXTRA="" NET="-nic user,model=virtio-net-pci-non-transitional"

case "$MODE" in
    snapshot) SNAPSHOT=",snapshot=on" ;;
    persistent) ;;
    dev)
        SNAPSHOT=",snapshot=on"
        "$SCRIPT_DIR/mkportdisk.sh"
        EXTRA="-drive file=$PORTDISK,if=none,id=portdisk,format=raw,snapshot=on"
        EXTRA="$EXTRA -device virtio-blk-pci-non-transitional,drive=portdisk"
        echo "=== DEV MODE ==="
        echo "Inside 9front:"
        echo "  dossrv -f /dev/sdG0/data portdisk"
        echo "  mount /srv/portdisk /n/port"
        echo "  mkdir -p /sys/src/9/a733"
        echo "  dircp /n/port/a733 /sys/src/9/a733"
        echo "  cd /sys/src/9/a733 && mk"
        echo "================"
        ;;
    *) echo "Usage: $0 [snapshot|persistent|dev]"; exit 1 ;;
esac

exec qemu-system-aarch64 \
    -M virt,gic-version=3,highmem-ecam=off \
    -cpu cortex-a72 -m 2G -smp 2 \
    -bios "$BIOS" \
    -drive "file=$DISK,if=none,id=disk0${SNAPSHOT}" \
    -device virtio-blk-pci-non-transitional,drive=disk0 \
    $EXTRA $NET \
    -nographic -serial mon:stdio \
    -no-reboot
