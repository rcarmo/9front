# Vendors

Checked-out upstream source trees and submodules live under `vendors/`.

## Convention

- `vendors/<name>/` — upstream source dependency used directly by build or analysis scripts
- `bootstrap/<board>/` — pinned binary blobs required for hardware bring-up
- `images/<board>/` — generated runtime images and local build artifacts

Current entries:

- `vendors/orangepi-build/` — Orange Pi BSP/build tree used for pack-uboot tooling and reference configs
