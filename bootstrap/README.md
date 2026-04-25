# Bootstrap blobs

Checked-in board boot binaries live here.

## Convention

- `images/` holds generated build outputs and temporary artifacts.
- `bootstrap/` holds pinned binary inputs required to bootstrap hardware.
- Board-specific blobs live under `bootstrap/<board>/...`.
- Prefer exact vendor-extracted raw blobs here over ad-hoc regenerated copies when hardware bring-up depends on them.

For Orange Pi 4 Pro, `mksdcard.sh` uses the files under `bootstrap/orangepi4pro/vendor-debian-1.0.6/` by default.
