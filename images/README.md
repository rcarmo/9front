# Images layout

Board-specific runtime images live under `images/<board>/`.

## Convention

- `images/<board>/u-boot.bin` — QEMU firmware / emulator boot binary for that board workflow
- `images/<board>/*.qcow2` — local emulator disks
- `images/<board>/portdisk.img` — generated transfer disk for development
- `images/<board>/9*.u`, `9*.k`, `9*.img` — generated kernel artifacts
- `images/<board>/sdcard.img` — generated flashable SD card image

Pinned hardware bootstrap binaries do **not** live here; those belong under `bootstrap/<board>/`.
