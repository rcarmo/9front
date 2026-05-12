# 9front Repo Notes

## Repository conventions

- `vendors/<name>/` — upstream source trees and submodules
- `bootstrap/<board>/` — pinned hardware bootstrap blobs needed for bring-up
- `images/<board>/` — generated board-specific images and emulator/runtime assets
- `port/<board>/` — local board-port sources used for guest-side rebuilds

For Orange Pi 4 Pro, the default board is `orangepi4pro`.

## Serial monitoring

Use the Makefile targets instead of ad-hoc tmux commands.

- `make serial-start` — start tmux + `picocom` capture
- `make serial-fresh` — truncate the log, then start capture
- `make serial-status` — show tmux/process/log status
- `make serial-tail` — tail the current serial log
- `make serial-attach` — attach to the tmux session
- `make serial-stop` — stop capture

Current defaults:

- device: `/dev/ttyUSB0`
- baud: `115200`
- tmux session: `serial`
- log: `/workspace/tmp/serial-boot.log`

Override any of these with Make variables, e.g.:

```sh
make serial-start SERIAL_DEV=/dev/ttyUSB1
```

## Bring-up notes and audit trail

Keep the running analysis in these files instead of scattering findings in chat only:

- `docs/BRINGUP-STATUS.md` — current hardware breakpoint and short status
- `docs/FIT-GAP-AUDIT.md` — deeper fit-gap analysis against the working vendor image
- `docs/disasm/orangepi4pro/vendor-debian-1.0.6/` — targeted disassembly and source snapshots from the known-good boot chain

When debugging early boot, update those notes in the same tranche as the code changes.

## Board rebuild workflow

When changing boot-critical bring-up code for Orange Pi 4 Pro:

1. edit files under `port/a733/`
2. update `docs/BRINGUP-STATUS.md` / `docs/FIT-GAP-AUDIT.md` when findings change
3. run `make kernel`
4. run `make sdcard`
5. run `make serial-fresh`
6. flash `images/orangepi4pro/sdcard.img`
7. watch UART before asking for video/framebuffer checks
