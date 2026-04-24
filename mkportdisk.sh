#!/bin/bash
# Pack port/a733/ into a FAT disk image for transfer to 9front QEMU.
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PORT="$SCRIPT_DIR/port/a733"
IMG="$SCRIPT_DIR/images/portdisk.img"

dd if=/dev/zero of="$IMG" bs=1M count=16 status=none
mformat -i "$IMG" -F ::
mmd -i "$IMG" ::/a733
for f in "$PORT"/*; do
  [ -f "$f" ] && mcopy -i "$IMG" "$f" ::/a733/
done
echo "Port disk: $IMG ($(mdir -i "$IMG" ::/a733/ 2>&1 | grep 'files' | head -1))"
