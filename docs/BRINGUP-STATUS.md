# Orange Pi 4 Pro bring-up status

Updated: 2026-04-26

## Current boot chain status

Working path on real hardware:

1. Allwinner Boot0 loads from SD
2. ATF / BL31 runs
3. vendor U-Boot starts
4. `boot.scr` is found on the FAT partition
5. U-Boot loads:
   - `sun60i-a733-orangepi-4-pro.dtb`
   - `9a733.img`
6. U-Boot executes `booti ...`
7. Handoff reaches `Starting kernel ...`

## Current breakpoint

The current debug kernel emits single-character UART breadcrumbs from `port/a733/l.s`.

Observed latest sequence:

```text
ABCDE01234
```

Meaning:

- `A` — entered `_start`
- `B` — returned from `svcmode`
- `C` — returned from `mmudisable`
- `D` — returned from cache invalidation
- `E` — initial page-table setup completed
- `0` — entered `mmuenable()`
- `1` — `MAIR_EL1` programmed
- `2` — `TCR_EL1` programmed
- `3` — `TTBR0_EL1` / `TTBR1_EL1` programmed
- `4` — immediately before writing `SCTLR_EL1`

Missing breadcrumb:

- `5` — first instruction after MMU enable

So the current failure is at, or immediately after, `MSR SCTLR_EL1`.

## Ruled out so far

These are no longer the primary problem:

- SD card image layout
- vendor `boot0` / `boot_package` extraction
- U-Boot script execution
- DTB loading by U-Boot
- Linux `Image` wrapper acceptance by `booti`
- serial capture reliability once `make serial-start` is used
- cache enable itself as the only trigger (MMU-only build still fails before `5`)
- low-MMIO-only TTBR0 workaround as the primary cause

## Important audit finding

A previous `mmuidmap()` experiment incorrectly used a child table hanging off `L1BOT`.
That was unsafe because `L1BOT` only reserves one page, so the extra child table could overlap the shared kernel L1 area and corrupt page tables before MMU enable.

Current code keeps TTBR0 bring-up mappings simple again.

## Active local overrides in `port/a733/`

Local bring-up overrides now exist for:

- `l.s`
- `bootargs.c`
- `gic.c`
- `main.c`
- `mem.c`
- `uartaw.c`
- `io.h`
- `mem.h`
- `mkfile`
- `fns.h`

These are the files to inspect first before touching shared `sys/src/9/arm64/` code.

## Recommended next debugging direction

1. audit TTBR0/TTBR1 layout and alignment assumptions against the shared arm64 MMU code
2. inspect whether the temporary identity map covers the exact fetch/stack/page-table addresses in use at MMU-on
3. if needed, emit a second-stage breadcrumb path that avoids any relocation assumptions immediately after `SCTLR_EL1`
4. only after stable UART post-MMU should framebuffer/video debugging become the priority
