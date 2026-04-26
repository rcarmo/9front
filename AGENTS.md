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

## Board rebuild workflow

When changing boot-critical bring-up code for Orange Pi 4 Pro:

1. edit files under `port/a733/`
2. run `make kernel`
3. run `make sdcard`
4. run `make serial-fresh`
5. flash `images/orangepi4pro/sdcard.img`
6. watch UART before asking for video/framebuffer checks
