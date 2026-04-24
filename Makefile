# 9front ARM64 port for Orange Pi 4 Pro (Allwinner A733)
#
# Targets:
#   make portdisk    - pack port files into FAT image for QEMU transfer
#   make kernel      - build kernel inside 9front QEMU (automated)
#   make sdcard      - build bootable SD card image
#   make boot        - boot 9front QEMU (snapshot mode)
#   make dev         - boot QEMU in dev mode (port disk attached)
#   make clean       - remove built artifacts

IMAGES  = images
PORT    = port/a733
PORTIMG = $(IMAGES)/portdisk.img
KERNEL  = $(IMAGES)/9a733.u
SDCARD  = $(IMAGES)/sdcard.img

.PHONY: portdisk kernel sdcard boot dev clean help

help:
	@echo "9front ARM64 — Orange Pi 4 Pro"
	@echo ""
	@echo "  make portdisk   Pack port files → FAT image"
	@echo "  make kernel     Build kernel (boots QEMU, compiles, extracts)"
	@echo "  make sdcard     Build bootable SD card image"
	@echo "  make boot       Boot 9front QEMU (snapshot)"
	@echo "  make dev        Boot QEMU with port disk attached"
	@echo "  make clean      Remove build artifacts"
	@echo ""
	@echo "To flash SD card: sudo dd if=$(SDCARD) of=/dev/sdX bs=1M status=progress"

portdisk: $(PORTIMG)

$(PORTIMG): $(wildcard $(PORT)/*)
	./mkportdisk.sh

kernel: $(KERNEL)

$(KERNEL): $(PORTIMG)
	@echo "=== Building kernel inside 9front QEMU ==="
	expect extract-kernel.exp
	mcopy -i $(PORTIMG) ::/9a733.u $(IMAGES)/9a733.u
	mcopy -i $(PORTIMG) ::/9a733 $(IMAGES)/9a733
	@echo "Kernel: $$(ls -lh $(KERNEL))"

sdcard: $(SDCARD)

$(SDCARD): $(KERNEL)
	./mksdcard.sh $(KERNEL)

boot:
	./boot.sh snapshot

dev: $(PORTIMG)
	./boot.sh dev

clean:
	rm -f $(PORTIMG) $(KERNEL) $(IMAGES)/9a733 $(SDCARD)
	@echo "Cleaned build artifacts"
