# Orange Pi 4 Pro (A733) — Board Notes

Source: OrangePi_4_Pro_A733_User Manual_v1.4.pdf (18MB, in workspace root)

## SoC & CPU

- **SoC:** Allwinner A733 (sun60iw2)
- **CPU:** Octa-core ARM Cortex (big.LITTLE)
- **RAM:** Optional 2/4/8/16 GB LPDDR

## Physical

- **PCB:** 89mm × 56mm
- **Weight:** 58g
- **Positioning holes:** 4× 3.0mm diameter

## Boot Priority

**TF card (SD) has HIGHER priority than eMMC.**
If a TF card with a bootable image is inserted, it boots from TF card.
(Manual section 3.35, p.193)

## Debug Serial Port

- **Type:** 3-pin header (separate from 40-pin GPIO)
- **Labeled:** DEBUG on PCB
- **Pins:** GND, TX, RX
- **Baud:** 115200
- **Flow control:** None
- **Voltage:** 3.3V (use 3.3V USB-to-TTL adapter, NOT 5V)

### Wiring (FTDI/USB-TTL ↔ Board)

| USB-TTL | Board DEBUG |
|---------|-------------|
| GND     | GND         |
| RX      | TX          |
| TX      | RX          |

Cross-connect TX↔RX. If no output, swap TX and RX.

## Buttons

- **BOOT** — 1× boot button
- **RESET** — 1× reset button
- **PWR ON** — 1× power button

## Connectors

| Interface | Detail |
|-----------|--------|
| Display | 1× HDMI 2.0, 1× MIPI-DSI, 1× eDP |
| Camera | 1× 4-lane MIPI-CSI |
| USB | 1× USB-A 3.0, 3× USB-A 2.0 HOST |
| Audio | 3.5mm headphone jack (input/output) |
| Network | GbE Ethernet (RJ45) |
| WiFi/BT | Onboard (AIC8800) |
| Storage | TF card slot, eMMC expansion (16/32/64/128GB optional) |
| GPIO | 40-pin header (GPIO, UART, I2C, SPI, PWM) |
| Debug | 3-pin serial header |
| Power | USB Type-C 5V 3A |

## Power

- **Input:** USB Type-C, 5V 3A
- **5V pin on 40-pin header:** Can also supply power (see manual section 2.11)

## TF Card Requirements

- Minimum 8GB capacity
- Class 10 or higher speed
- Recommended: SanDisk brand

## Supported OS (vendor)

Ubuntu, Debian, Android 13

## SD Card Image Layout (Allwinner)

| Offset | Content |
|--------|---------|
| 8 KiB (bs=8k seek=1) | boot0 SPL |
| 16.8 MiB (bs=8k seek=2050) | boot_package (U-Boot + firmware) |
| 32 MiB+ | OS partition(s) |

## Key Memory-Mapped Addresses (from vendor DTS)

| Peripheral | Address |
|------------|---------|
| SoC base | `0x03000000` |
| UART0 (debug) | `0x02500000` |
| UART1 | `0x02501000` |
| GIC Distributor | `0x03400000` |
| GIC Redistributor | `0x03460000` |
| GMAC0 (Ethernet) | `0x04500000` |
| GMAC1 | `0x04510000` |
| PCIe DBI | `0x06000000` |
| XHCI (USB3) | `0x06A00000` |
| SD/MMC 0 | `0x04020000` |
| SD/MMC 2 (eMMC) | `0x04022000` |
| DRAM base | `0x40000000` |
| Pinctrl | `0x02000000` |
| CCU (clocks) | `0x02002000` |

## Serial Console Software (from manual)

### Linux
- picocom, minicom, or PuTTY
- Device: `/dev/ttyUSB0`
- Settings: 115200 8N1, no flow control

### Windows
- PuTTY or MobaXterm
- Same settings: 115200 8N1, no flow control

## Notes

- The manual references images/diagrams for the debug header position
  and board layout that can't be extracted as text. Refer to the PDF
  directly (sections 1.5 and 2.10.1) for visual pinout.
- eMMC module is optional and plugs into a dedicated connector.
