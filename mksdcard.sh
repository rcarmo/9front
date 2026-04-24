#!/bin/bash
# Build a bootable SD card image for Orange Pi 4 Pro running 9front.
#
# Layout:
#   0       - 8K:     MBR + empty
#   8K      - 16.4M:  boot0 (Allwinner SPL)
#   16.4M   - 32M:    boot_package (U-Boot + firmware)
#   32M     - end:    FAT32 boot partition (kernel uImage + boot.scr)
#
# Usage: ./mksdcard.sh [kernel.u]
#   If no kernel specified, builds one first via QEMU.

set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BOOT0="$SCRIPT_DIR/orangepi-build/external/packages/pack-uboot/sun60iw2/bin/boot0_sdcard_a733.fex"
BOOTPKG="$SCRIPT_DIR/images/boot_package.fex"
IMG="$SCRIPT_DIR/images/sdcard.img"
KERNEL="${1:-$SCRIPT_DIR/images/9a733.u}"

if [ ! -f "$BOOT0" ]; then
    echo "Missing boot0: $BOOT0"
    exit 1
fi
if [ ! -f "$BOOTPKG" ]; then
    echo "Missing boot_package: $BOOTPKG"
    echo "Build it first (see u-boot-aw/)"
    exit 1
fi
if [ ! -f "$KERNEL" ]; then
    echo "No kernel found at $KERNEL"
    echo "Build the kernel first, or pass path as argument."
    exit 1
fi

echo "=== Building SD card image ==="

# Create 64MB image
dd if=/dev/zero of="$IMG" bs=1M count=64 status=none

# Write boot0 at offset 8K (bs=8k seek=1)
dd if="$BOOT0" of="$IMG" bs=8k seek=1 conv=notrunc status=none
echo "  boot0: $(stat -c%s "$BOOT0") bytes at 8K"

# Write boot_package at offset 8K*2050 = 16,793,600
dd if="$BOOTPKG" of="$IMG" bs=8k seek=2050 conv=notrunc status=none
echo "  boot_package: $(stat -c%s "$BOOTPKG") bytes at 16.8M"

# Create FAT32 partition starting at 32MB (sector 65536 with 512B sectors)
# First, create a partition table
cat << PARTEOF | sfdisk "$IMG" --no-reread 2>/dev/null
label: dos
unit: sectors
start=65536, type=c
PARTEOF

# Format the FAT partition
FATOFF=$((65536 * 512))
FATSZ=$(( $(stat -c%s "$IMG") - $FATOFF ))
dd if="$IMG" of="$IMG.fat" bs=512 skip=65536 count=$(( $FATSZ / 512 )) status=none
mkfs.vfat -F 32 -n PLAN9BOOT "$IMG.fat"

# Copy kernel and boot script to FAT partition
mcopy -i "$IMG.fat" "$KERNEL" ::/9a733.u

# Create U-Boot boot script
cat > /tmp/boot.cmd << 'BOOTCMD'
echo "=== 9front Orange Pi 4 Pro ==="
fatload ${devtype} ${devnum}:1 0x43100000 9a733.u
bootm 0x43100000
BOOTCMD
mkimage -C none -A arm64 -T script -d /tmp/boot.cmd /tmp/boot.scr 2>/dev/null || true
if [ -f /tmp/boot.scr ]; then
    mcopy -i "$IMG.fat" /tmp/boot.scr ::/boot.scr
    echo "  boot.scr: U-Boot script"
fi

# Write FAT partition back
dd if="$IMG.fat" of="$IMG" bs=512 seek=65536 conv=notrunc status=none
rm -f "$IMG.fat"

echo "  kernel: $(stat -c%s "$KERNEL") bytes as 9a733.u"
echo ""
echo "SD card image: $IMG ($(stat -c%s "$IMG" | numfmt --to=iec))"
echo ""
echo "Flash to SD card:"
echo "  sudo dd if=$IMG of=/dev/sdX bs=1M status=progress"
