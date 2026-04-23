# 9front ARM64 Port — Orange Pi 4 Pro

Porting [9front](http://9front.org) (Plan 9 from Bell Labs fork) to the [Orange Pi 4 Pro](http://www.orangepi.org/html/hardWare/computerAndMicrocontrollers/service-and-support/Orange-Pi-4-Pro.html).

## Target Hardware

### Orange Pi 4 Pro

| Spec | Detail |
|------|--------|
| **SoC** | Allwinner A733 (`sun60iw2`) |
| **Architecture** | ARM64 (AArch64) |
| **CPU** | 8 cores — 4× Cortex-A76 + 4× Cortex-A55 (big.LITTLE) |
| **RAM** | 2/4/8/16 GB LPDDR4/5 |
| **Storage** | eMMC + SD card (Allwinner MMC), PCIe (NVMe), USB 3.0 |
| **Network** | Gigabit Ethernet (Synopsys GMAC/DWMAC), WiFi (AIC8800) |
| **USB** | DWC3 + XHCI |
| **Interrupt Controller** | ARM GICv3 (with ITS) |
| **Timer** | ARM Generic Timer |
| **UART** | Allwinner custom (`AW_UART_NG`) — 8250-register-compatible, MMIO |
| **PCIe** | Allwinner PCIe controller |
| **GPU** | PowerVR (Imagination Technologies) |
| **Boot** | Allwinner custom boot0 → U-Boot |

### Vendor BSP

- **Build system:** [orangepi-xunlong/orangepi-build](https://github.com/orangepi-xunlong/orangepi-build)
- **Board family:** `sun60iw2`
- **Board config:** `orangepi4pro.conf`
- **Device tree:** `allwinner/sun60i-a733-orangepi-4-pro.dtb`
- **Kernel:** Allwinner BSP kernel (5.15 legacy / 6.6 current)
- **Console:** `ttyS0`

## Project Structure

```
projects/9front/
├── src/              # 9front source tree (git mirror)
├── u-boot/           # U-Boot bootloader (built for QEMU arm64)
├── plan9port/        # Plan 9 from User Space (mk, etc.)
├── orangepi-build/   # Vendor BSP/build system (reference)
├── images/           # Boot images
│   ├── u-boot.bin          # U-Boot firmware for QEMU
│   └── 9front-11677.arm64.qcow2  # Pre-built 9front arm64 image
├── boot.sh           # QEMU boot script
└── README.md
```

## Build Chain Status

| Component | Status |
|-----------|--------|
| 9front source tree | ✅ Cloned (`src/`) |
| U-Boot arm64 QEMU firmware | ✅ Built (`images/u-boot.bin`) |
| 9front arm64 QCOW2 image | ✅ Downloaded (rev 11677) |
| QEMU boot (serial console) | ✅ Verified — boots to `rc` shell |
| Plan 9 C compiler (`7c`) | ✅ Works inside 9front |
| Plan 9 linker (`7l`) | ✅ Works inside 9front |
| Kernel build (`mk`) | ✅ Builds `9qemu` from source |
| plan9port (host tools) | ✅ Built (`mk` available on host) |
| Orange Pi vendor BSP | ✅ Cloned (`orangepi-build/`) |

## Quick Start

### Boot 9front in QEMU (arm64)

```sh
./boot.sh              # snapshot mode (no disk changes)
./boot.sh persistent   # writes survive reboot
```

At `bootargs` prompt → Enter. At `user[glenda]:` → Enter.

### Build the arm64 kernel inside 9front

```
cd /sys/src/9/arm64
mk
```

Produces `9qemu` (raw kernel) and `9qemu.u` (U-Boot uImage).

## QEMU Configuration

```
qemu-system-aarch64 -M virt,gic-version=3,highmem-ecam=off \
    -cpu cortex-a72 -m 2G -smp 2 \
    -bios u-boot.bin \
    -drive file=9front.arm64.qcow2,if=none,id=disk0 \
    -device virtio-blk-pci-non-transitional,drive=disk0 \
    -nographic -serial mon:stdio
```

## Port Plan

### Existing 9front ARM64 Ports (reference)

| Port | SoC | Board | Board-specific files |
|------|-----|-------|---------------------|
| `arm64/qemu` | QEMU virt | Virtual | 12 .c |
| `bcm64/pi3` | BCM2837 | Raspberry Pi 3 | 12 .c |
| `bcm64/pi4` | BCM2711 | Raspberry Pi 4 | 12 .c |
| `imx8/reform` | i.MX8MQ | MNT Reform | 18 .c |
| `lx2k/honeycomb` | LX2160A | Honeycomb LX2K | 8 .c |

**Note:** No existing Allwinner/sunxi support in 9front. This is a fresh port.

### Peripheral Compatibility

| Peripheral | A733 IP Block | 9front driver | Status |
|------------|--------------|---------------|--------|
| Interrupt controller | ARM GICv3 | `arm64/gic.c` | ✅ Reuse |
| Timer | ARM Generic Timer | `arm64/clock.c` | ✅ Reuse |
| UART | Allwinner UART (8250-compat) | `pc/uarti8250.c` | 🔧 Adapt to MMIO |
| USB | DWC3 + XHCI | `usbxhci` | ✅ Reuse |
| NVMe | Standard (via PCIe) | `port/sdnvme.c` | ✅ Reuse |
| PCIe | Allwinner PCIe host | — | 🆕 Write |
| Ethernet | Synopsys GMAC (DWMAC) | — | 🆕 Write |
| SD/eMMC | Allwinner MMC | — | 🆕 Write |
| VirtIO (QEMU testing) | Standard | `port/sdvirtio10.c` | ✅ Reuse |

### New Files Needed (~5-8)

| File | Purpose | Reference |
|------|---------|-----------|
| `main.c` | Board init, memory map | `arm64/main.c` + `lx2k/main.c` |
| `mem.c` / `mem.h` | Physical memory layout | `arm64/mem.c` |
| `dat.h` / `fns.h` / `io.h` | Board-specific types & declarations | `lx2k/dat.h` |
| `uartaw.c` | Allwinner UART (8250-register-compat, MMIO) | `pc/uarti8250.c` |
| `pciaw.c` | Allwinner PCIe host controller | `lx2k/pcilx2k.c` |
| `a733` | Kernel config file | `arm64/qemu` |
| `mkfile` | Build rules | `arm64/mkfile` |

### Milestone Plan

1. **M0 — QEMU baseline** ✅
   - Boot pre-built 9front arm64 in QEMU
   - Build kernel from source inside 9front

2. **M1 — UART + serial console**
   - Write `uartaw.c` (Allwinner UART driver)
   - Boot custom kernel in QEMU with Allwinner UART emulation
   - Get serial console output

3. **M2 — Minimal hardware boot**
   - Board init (`main.c`, `mem.c`)
   - GICv3 + timer (reuse from `arm64/`)
   - Boot to `rc` shell on real hardware via serial

4. **M3 — Storage**
   - PCIe host controller (`pciaw.c`)
   - Boot from NVMe or USB storage

5. **M4 — Network**
   - Ethernet driver (Synopsys GMAC)
   - Network boot / TCP/IP

6. **M5 — SD/eMMC**
   - Allwinner MMC driver for SD card boot

## Port Files

All port-specific code lives in `port/a733/`:

```
port/a733/
├── NOTES.md    # Hardware addresses, design decisions, references
├── a733        # Kernel config (device/driver selection)
├── dat.h       # Machine-specific data structures
├── fns.h       # Function declarations
├── io.h        # I/O and interrupt definitions
├── main.c      # Board init, memory config, boot sequence
├── mem.c       # Physical memory setup
├── mem.h       # Memory map, peripheral base addresses
├── mkfile      # Build rules
└── uartaw.c    # Allwinner UART driver (8250-compat, MMIO)
```

To deploy into a 9front tree for building:

```
cp port/a733/* /sys/src/9/a733/
cd /sys/src/9/a733
mk
```

Shared arm64 files (`cache.v8.s`, `clock.c`, `fpu.c`, `gic.c`, `l.s`,
`mmu.c`, `sysreg.c`, `trap.c`) are used directly from `sys/src/9/arm64/`.
