#!/bin/bash
# Boot 9front arm64 in QEMU
# Usage: ./boot.sh [snapshot|persistent]
#   snapshot (default): boot without modifying the disk image
#   persistent: boot with changes written to disk

set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
IMAGES="$SCRIPT_DIR/images"
BIOS="$IMAGES/u-boot.bin"
DISK="$IMAGES/9front-11677.arm64.qcow2"

MODE="${1:-snapshot}"
SNAPSHOT_FLAG=""
if [ "$MODE" = "snapshot" ]; then
    SNAPSHOT_FLAG=",snapshot=on"
fi

if [ ! -f "$BIOS" ]; then
    echo "Missing U-Boot firmware: $BIOS"
    echo "Build it: cd u-boot && make qemu_arm64_defconfig && make -j\$(nproc)"
    exit 1
fi

if [ ! -f "$DISK" ]; then
    echo "Missing 9front disk image: $DISK"
    echo "Download from: http://iso.only9fans.com/9front/"
    exit 1
fi

exec qemu-system-aarch64 \
    -M virt,gic-version=3,highmem-ecam=off \
    -cpu cortex-a72 -m 2G -smp 2 \
    -bios "$BIOS" \
    -drive "file=$DISK,if=none,id=disk0${SNAPSHOT_FLAG}" \
    -device virtio-blk-pci-non-transitional,drive=disk0 \
    -nographic -serial mon:stdio \
    -no-reboot
