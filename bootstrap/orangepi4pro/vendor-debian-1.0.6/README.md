# Orange Pi 4 Pro bootstrap blobs

Exact boot-chain binaries extracted from:

- `Orangepi4pro_1.0.6_debian_bookworm_server_linux5.15.147.7z`
- image: `Orangepi4pro_1.0.6_debian_bookworm_server_linux5.15.147.img`

These are the pinned inputs used to bootstrap 9front on real hardware.

## Layout

- `raw/boot0_sdcard.fex` — boot ROM stage loaded from SD at 8 KiB
- `raw/boot_package.fex` — sunxi package at 16.8 MiB containing U-Boot + monitor + SCP
- `dtb/sun60i-a733-orangepi-4-pro.dtb` — vendor DTB used by `mksdcard.sh`
- `extracted/u-boot.bin` — U-Boot payload extracted from `boot_package.fex`
- `extracted/monitor.fex` — monitor payload extracted from `boot_package.fex`
- `extracted/scp.fex` — SCP payload extracted from `boot_package.fex`
- `extracted/u-boot.dtb` / `u-boot.dts` — U-Boot DTB extracted from the embedded U-Boot payload for analysis

## SHA256

```text
49bfb4186f13c9642841fd4b7246c6de2d3575341546c877a9d0d6c8b84f4a1c  raw/boot0_sdcard.fex
7a2661b080f5c5d8ba32566bc79f1ccfbfb8912a4a5c0c1a4856a9380542c807  raw/boot_package.fex
d49dfb0b83234aa31f153348a7f6c92098642c780d5939a531f590a30cf97b77  dtb/sun60i-a733-orangepi-4-pro.dtb
8af16b86ae8e2eff634a3ebca76a8cdf3c10b3540387573d46c3d785545b2632  extracted/monitor.fex
07e6b97628101963e7944e012948c4bdf721f914bad347acd7dfbde89db42749  extracted/scp.fex
4d21321fba32d30b1dcc398c5850861db8f8dba213770e690ed6b51aae534533  extracted/u-boot.bin
7d54abbf6240f95e5e2ae9ba36a964ab76ba0e9db56c608008f0b4f1c0df0300  extracted/u-boot.dtb
0d8a1a1ff0915135b6ce8304198a5dbb1630cb0afb4de497e9d596b662b61a12  extracted/u-boot.dts
```
