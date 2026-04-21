# 9front ARM64 Port

Porting [9front](http://9front.org) (Plan 9 from Bell Labs fork) to ARM SBCs.

## Project Structure

```
projects/9front/
├── src/              # 9front source tree (git clone)
├── u-boot/           # U-Boot bootloader (built for QEMU arm64)
├── plan9port/        # Plan 9 from User Space (mk, etc.)
├── images/           # Boot images
│   ├── u-boot.bin          # U-Boot firmware for QEMU
│   └── 9front-11677.arm64.qcow2  # Pre-built 9front arm64 image
├── boot.sh           # QEMU boot script
└── README.md
```

## Quick Start

### Boot 9front in QEMU (arm64)

```sh
./boot.sh              # snapshot mode (no disk changes)
./boot.sh persistent   # writes survive reboot
```

At the `bootargs` prompt, press Enter for default (`local!/dev/sdF0/fs`).
At the `user[glenda]:` prompt, press Enter.

### Build the arm64 kernel inside 9front

```
cd /sys/src/9/arm64
mk
```

This produces:
- `9qemu` — raw kernel binary
- `9qemu.u` — U-Boot uImage

## Build Chain Status

| Component | Status |
|-----------|--------|
| 9front source tree | ✅ Cloned (`src/`) |
| U-Boot arm64 QEMU firmware | ✅ Built (`images/u-boot.bin`) |
| 9front arm64 QCOW2 image | ✅ Downloaded (rev 11677) |
| QEMU boot (serial console) | ✅ Verified — boots to `rc` shell |
| Plan 9 C compiler (7c) | ✅ Works inside 9front |
| Plan 9 linker (7l) | ✅ Works inside 9front |
| Kernel build (mk) | ✅ Builds `9qemu` successfully |
| plan9port (host tools) | ✅ Built (`mk` available on host) |

## QEMU Configuration

```
qemu-system-aarch64 -M virt,gic-version=3,highmem-ecam=off \
    -cpu cortex-a72 -m 2G -smp 2 \
    -bios u-boot.bin \
    -drive file=9front.arm64.qcow2,if=none,id=disk0 \
    -device virtio-blk-pci-non-transitional,drive=disk0 \
    -nographic -serial mon:stdio
```

Key requirements:
- `gic-version=3` — GICv3 interrupt controller
- `highmem-ecam=off` — PCIe ECAM in low memory (9front requirement)
- `virtio-blk-pci-non-transitional` — VirtIO 1.0 block device
- U-Boot as BIOS (not UEFI firmware)

## ARM SBC Targets

_Board selection pending._ Candidates should have:
- ARM64 (AArch64) SoC
- Mainline Linux/U-Boot support (for reference)
- PCIe or USB storage
- UART serial console
- Ideally: existing 9front or Plan 9 community interest

## Notes

- 9front uses its own C toolchain (Plan 9 compilers: `7c`/`7l`/`7a` for arm64)
- Cross-compilation from Linux requires building inside 9front (native toolchain only)
- The `plan9port` install provides `mk` on the host for scripting/automation
- Serial console is the primary interface; graphics via `drawterm` connection
