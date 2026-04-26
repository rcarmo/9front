# 9front ARM64 port for Orange Pi 4 Pro (Allwinner A733)
#
# Targets:
#   make portdisk    - pack port files into FAT image for QEMU transfer
#   make kernel      - build kernel inside 9front QEMU (automated)
#   make sdcard      - build bootable SD card image
#   make boot        - boot 9front QEMU (snapshot mode)
#   make dev         - boot QEMU in dev mode (port disk attached)
#   make clean       - remove built artifacts

BOARD   = orangepi4pro
IMAGES  = images/$(BOARD)
PORT    = port/a733
PORTIMG = $(IMAGES)/portdisk.img
KERNEL  = $(IMAGES)/9a733.u
RAWKERN = $(IMAGES)/9a733.k
BOOTIMG = $(IMAGES)/9a733.img
SDCARD  = $(IMAGES)/sdcard.img

SERIAL_DEV ?= /dev/ttyUSB0
SERIAL_BAUD ?= 115200
SERIAL_SESSION ?= serial
SERIAL_LOG ?= /workspace/tmp/serial-boot.log

.PHONY: portdisk kernel sdcard boot dev clean help \
	serial-start serial-fresh serial-stop serial-status serial-tail serial-attach

help:
	@echo "9front ARM64 — Orange Pi 4 Pro"
	@echo ""
	@echo "  make portdisk   Pack port files → FAT image"
	@echo "  make kernel     Build kernel (boots QEMU, compiles, extracts)"
	@echo "  make sdcard     Build bootable SD card image"
	@echo "  make boot         Boot 9front QEMU (snapshot)"
	@echo "  make dev          Boot QEMU with port disk attached"
	@echo "  make serial-start Start tmux + picocom serial capture"
	@echo "  make serial-fresh Truncate serial log, then start capture"
	@echo "  make serial-stop  Stop tmux + picocom serial capture"
	@echo "  make serial-status Show serial capture status"
	@echo "  make serial-tail  Tail the serial log"
	@echo "  make serial-attach Attach to the tmux serial session"
	@echo "  make clean        Remove build artifacts"
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

serial-start:
	@echo "Starting serial capture on $(SERIAL_DEV) -> $(SERIAL_LOG)"
	@sudo fuser -k $(SERIAL_DEV) 2>/dev/null || true
	@tmux kill-session -t $(SERIAL_SESSION) 2>/dev/null || true
	@mkdir -p $(dir $(SERIAL_LOG))
	@tmux start-server
	@tmux new-session -d -s $(SERIAL_SESSION) "script -a -qfc 'sudo picocom -b $(SERIAL_BAUD) --flow none --noreset $(SERIAL_DEV)' $(SERIAL_LOG)"
	@sleep 1
	@$(MAKE) serial-status

serial-fresh:
	@rm -f $(SERIAL_LOG)
	@$(MAKE) serial-start

serial-stop:
	@sudo fuser -k $(SERIAL_DEV) 2>/dev/null || true
	@tmux kill-session -t $(SERIAL_SESSION) 2>/dev/null || true
	@echo "Stopped serial capture"

serial-status:
	@echo "tmux sessions:"
	@tmux list-sessions 2>/dev/null | grep '^$(SERIAL_SESSION):' || echo "  $(SERIAL_SESSION) not running"
	@echo "serial-related processes:"
	@ps -eo pid,ppid,etimes,cmd | grep -E 'picocom|script -qfc|tmux .*$(SERIAL_SESSION)' | grep -v grep || true
	@echo "serial log:"
	@if [ -f $(SERIAL_LOG) ]; then \
		stat -c 'path=%n size=%s mtime=%y' $(SERIAL_LOG); \
	else \
		echo '  missing $(SERIAL_LOG)'; \
	fi

serial-tail:
	@if [ -f $(SERIAL_LOG) ]; then \
		tail -n 40 $(SERIAL_LOG); \
	else \
		echo 'missing $(SERIAL_LOG)'; \
	fi

serial-attach:
	tmux attach -t $(SERIAL_SESSION)

clean:
	rm -f $(PORTIMG) $(KERNEL) $(RAWKERN) $(BOOTIMG) $(IMAGES)/9a733 $(SDCARD)
	@echo "Cleaned build artifacts"
