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
BOARD="${BOARD:-orangepi4pro}"
VENDOR_BOOT_DIR="$SCRIPT_DIR/bootstrap/$BOARD/vendor-debian-1.0.6"
BOOT0="${BOOT0:-$VENDOR_BOOT_DIR/raw/boot0_sdcard.fex}"
BOOTPKG="${BOOTPKG:-$VENDOR_BOOT_DIR/raw/boot_package.fex}"
DTB="${DTB:-$VENDOR_BOOT_DIR/dtb/sun60i-a733-orangepi-4-pro.dtb}"
IMGDIR="$SCRIPT_DIR/images/$BOARD"
IMG="$IMGDIR/sdcard.img"
KERNEL="${1:-$IMGDIR/9a733.u}"
RAWKERNEL="$IMGDIR/9a733.k"
BOOTIKERNEL="$IMGDIR/9a733.img"

[ -f "$BOOT0" ] || BOOT0="$SCRIPT_DIR/vendors/orangepi-build/external/packages/pack-uboot/sun60iw2/bin/boot0_sdcard_a733.fex"
[ -f "$BOOTPKG" ] || BOOTPKG="$SCRIPT_DIR/images/$BOARD/boot_package.fex"

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
if [ ! -f "$DTB" ]; then
    echo "Missing DTB: $DTB"
    exit 1
fi

echo "=== Building SD card image ==="
echo "  boot0 source: $BOOT0"
echo "  boot_package source: $BOOTPKG"
echo "  dtb source: $DTB"

mkdir -p "$IMGDIR"
tail -c +65 "$KERNEL" > "$RAWKERNEL"
python3 - "$RAWKERNEL" "$BOOTIKERNEL" <<'PY'
import struct, sys
src, dst = sys.argv[1], sys.argv[2]
payload = open(src, 'rb').read()
# Minimal AArch64 Linux Image header so vendor U-Boot booti will relocate the
# wrapper to base-of-RAM + text_offset, then branch into the real 9front kernel.
# We want the payload to start at 0x40100000, so place the 64-byte wrapper at
# 0x400fffc0 and the payload immediately after it.
# code0 = NOP, code1 = B +0x3c (from offset 4 -> 0x40)
text_offset = 0x100000 - 64
header = bytearray()
header += struct.pack('<I', 0xD503201F)
header += struct.pack('<I', 0x1400000F)
header += struct.pack('<Q', text_offset)
header += struct.pack('<Q', len(payload) + 64)
header += struct.pack('<Q', 0x2)
header += struct.pack('<Q', 0)
header += struct.pack('<Q', 0)
header += struct.pack('<Q', 0)
header += struct.pack('<I', 0x644D5241)
header += struct.pack('<I', 0)
open(dst, 'wb').write(header + payload)
PY

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

# Copy kernel payloads and DTB to FAT partition
mcopy -i "$IMG.fat" "$KERNEL" ::/9a733.u
mcopy -i "$IMG.fat" "$RAWKERNEL" ::/9a733.k
mcopy -i "$IMG.fat" "$BOOTIKERNEL" ::/9a733.img
mcopy -i "$IMG.fat" "$DTB" ::/sun60i-a733-orangepi-4-pro.dtb

# Create U-Boot boot script
cat > /tmp/boot.cmd << 'BOOTCMD'
echo "=== 9front Orange Pi 4 Pro ==="
fatload ${devtype} ${devnum}:1 ${fdt_addr_r} sun60i-a733-orangepi-4-pro.dtb
fatload ${devtype} ${devnum}:1 ${kernel_addr_r} 9a733.img
booti ${kernel_addr_r} - ${fdt_addr_r}
BOOTCMD
mkimage -C none -A arm64 -T script -d /tmp/boot.cmd /tmp/boot.scr 2>/dev/null || true
if [ -f /tmp/boot.scr ]; then
    mcopy -i "$IMG.fat" /tmp/boot.scr ::/boot.scr
    echo "  boot.scr: U-Boot script"
fi

# Write FAT partition back
dd if="$IMG.fat" of="$IMG" bs=512 seek=65536 conv=notrunc status=none
rm -f "$IMG.fat"

echo "  kernel wrapper: $(stat -c%s "$KERNEL") bytes as 9a733.u"
echo "  kernel raw: $(stat -c%s "$RAWKERNEL") bytes as 9a733.k"
echo "  kernel booti: $(stat -c%s "$BOOTIKERNEL") bytes as 9a733.img"
echo "  dtb: $(stat -c%s "$DTB") bytes as sun60i-a733-orangepi-4-pro.dtb"
echo ""
echo "SD card image: $IMG ($(stat -c%s "$IMG" | numfmt --to=iec))"
echo ""
echo "Flash to SD card:"
echo "  sudo dd if=$IMG of=/dev/sdX bs=1M status=progress"
