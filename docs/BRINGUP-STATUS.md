# Orange Pi 4 Pro bring-up status

Updated: 2026-05-12

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
8. 9front enters `_start`, enables the MMU, switches to the high virtual path, runs `main()`, builds the page allocator, creates pid1, enters EL0, and resolves pid1's first stack write fault

## Current breakpoint

The current debug kernel emits single-character UART breadcrumbs from `port/a733/l.s` plus C-side markers from the local `main.c`, `page.c`, `proc.c`, `userinit.c`, and `trap.c` overrides.

Latest captured hardware log reaches:

```text
ABCDE120345FGHIJKLMaboNOP
QR
Plan 9
STZ5567890rspciaw: stub — PCIe not yet implemented
12timer frequency 24000000 Hz
3cpu0: Allwinner A733
4tuiI...JUvwxy[]...}kl...mnoABCDEFGHIpq
pid1 fault[0] esr 0x5 pc 0x10034 far 0x3ffffefe90 write
pid1 fault[0] resolved
pid1 ureg preexit pc 0x10034 sp 0x3ffffeff80 psr 0x80 ...
```

Meaning:

- `ABCDE120345FG` — early assembly path, Linux-shaped CPU setup/MMU enable, and high-virtual handoff all complete
- `HIJKLMaboNOPQR` — C bootstrap reaches `bootargsinit()`, `meminit()`, `confinit()`, `xinit()`, `printinit()`, and the first `Plan 9` banner
- `STZ5567890` — `trapinit()`, `fpuinit()`, `pageinitwrap()`, and `pageinit()` complete; the direct-map expansion fixed the earlier `xalloc()` failure
- `rs1234t` — `procinit0()`, `initseg()`, `links()`, IRQ/time setup, and `timersinit()` complete
- `uiI...JUvwxy` — `userinit()`, first kproc creation, `mpinit()`, `mmu1init()`, exception-mask restore, and `schedinit()` all run
- `[]...}kl...mno...pq` — scheduler selects pid1, enters `linkproc()`/`proc0()`/`init0()`, prepares the first user stack/text, and executes the first `ERET` to EL0
- the first pid1 stack write fault is taken and resolved, then trap exit prepares to return to EL0 with IRQ masked in `SPSR_EL1` (`psr 0x80`)

So the live breakpoint has moved from initial MMU enable to the first EL0 fault-return/syscall window for pid1.

## Expected breadcrumb order in the latest image

A fresh boot of the current image should include the following high-level sequence:

```text
ABCDE120345FGHIJKLMaboNOPQRSTZ5567890rs1234tuiI...JUvwxy[]...}kl...mnoABCDEFGHIpq
```

The next useful movement is either:

- a new `pid1 mmuswitch refresh top[255] ...` line from the one-shot A733 diagnostic, proving the process-owned top-level page-table chain can repopulate the live `m->mmutop` entry before EL0 fault return;
- a pid1 syscall trace (`pid1 syscall[...]`) from `trap.c`, proving the first fault-return path resumed user code successfully; or
- a trap/panic after the recorded `pid1 ureg preexit ...`, proving the remaining fault is in the EL0 return path, user page tables, TOS accounting, or early timer/preemption interaction.

## Ruled out so far

These are no longer the primary problem:

- SD card image layout
- vendor `boot0` / `boot_package` extraction
- U-Boot script execution
- DTB loading by U-Boot
- Linux `Image` wrapper acceptance by `booti`
- serial capture reliability once `make serial-start` / `make serial-fresh` is used
- initial EL1 MMU enable and post-`SCTLR_EL1` fetch
- the early TTBR1 direct-map width needed by `pageinit()` / `xalloc()`
- scheduler selection of the first runnable process
- the first `touser()` edge into EL0

## Important audit finding

A previous `mmuidmap()` experiment incorrectly used a child table hanging off `L1BOT`.
That was unsafe because `L1BOT` only reserves one page, so the extra child table could overlap the shared kernel L1 area and corrupt page tables before MMU enable.

Current code now uses a dedicated low DRAM scratch page (`IDMAPL0ADDR = 0x40021000`) for the TTBR0 low-MMIO child table rather than hanging it off `L1BOT` or aliasing it with the TTBR1 root page.

## Latest hardware observation

A fresh boot of the refactored image reached:

```text
ABCDE12
```

That is a useful narrowing:

- `cpusetup<>` now runs and completes
- execution dies before the first previous `mmuenable<>` breadcrumb

The surviving clue was that `cpusetup<>` itself emits breadcrumbs via `debugputcphys<>`, but unlike `mmuenable<>` it was not preserving `LR` across nested `BL` calls. That means after printing `2`, `cpusetup<>` returned to its own post-debug tail instead of back to `_startup`, which matches the observed `ABCDE12` stop exactly.

Current code has therefore been tightened again to:

- preserve `LR` inside `cpusetup<>`
- do `TLBI VMALLE1` + `DSB NSH` at the top of `cpusetup<>`
- print `0` immediately on entry to `mmuenable<>`
- remove the extra `flushlocaltlb()` call from `mmuenable<>`

## Latest hardware observation

A newer boot moved further and printed:

```text
ABCDE120345345345...
```

That narrowed the previous bug to a bad return path inside `mmuenable<>`: it was looping through `3/4/5` because `LR` was being saved only after the first nested `BL debugputcphys<>`.

After fixing that, the next boot printed:

```text
ABCDE120345FGX
```

So the MMU transition and high-virtual handoff are now working, and the next fault happens after entering the pre-`main()` path.

A follow-up image added C-side breadcrumbs in `main()`, and the latest boot printed:

```text
ABCDE120345FGHIJKLMX
```

That narrows the next fault to the window after `quotefmtinstall()` and before `bootargsinit()` returns.

The first suspect was early DTB parsing through an unmapped high-half alias: `bootargsinit()` had been using `KADDR(bootdtb)` even though the DTB at `0x43000000` is outside the small TTBR1 `INITMAP` window at that stage. Current code now parses the DTB through the still-live TTBR0 identity mapping (`va = (void*)pa`).

That change alone did not move the breadcrumbs yet. The next image added fine-grained `bootargsinit()` markers, and the latest boot produced:

```text
ABCDE120345FGHIJKLMabcegX
```

So we now know:

- `a b` — entered `bootargsinit()` and finished `plan9iniinit(BOOTARGS, 0)`
- `c` — started the first DTB probe iteration
- `e` — `cankaddr(pa)` succeeded
- `g` — reached the call into `parsedevtree()`
- `X` — exception occurs inside `parsedevtree()` itself

The next image therefore added `parsedevtree()`-internal breadcrumbs, and the latest boot produced:

```text
ABCDE120345FGHIJKLMabcegpX
```

That means the fault happens on the very first DTB header access inside `parsedevtree()` — after entering the function (`p`), but before even the initial header check completes (`q`).

Rather than block all further bring-up on that one parser fault, the current image temporarily bypasses early DTB parsing in `bootargsinit()` and installs board-known fallback settings instead:

- `*maxmem = 4 GiB`
- `*ncpu = 1`

That worked: the next successful boot progressed well past `bootargsinit()` and reached:

```text
ABCDE120345FGHIJKLMaboNOP127 holes free
0x404ec000 0x7ffff000 1068576768
1068576768 bytes free
QR
Plan 9
STUVtimer frequency 24000000 Hz
Wcpu0: 1000MHz QEMU
YZ3071M memory: 1023M kernel data, 2048M user, 11249M swap
pciaw: stub — PCIe not yet implemented
panic: kenter: -120 stack bytes left, up 0x0 ureg 0xffffffffc00fe030 at pc 0xffffffffc01038bc
```

So the DTB parser is no longer the immediate blocker. The new live issue is a mach-stack overflow once interrupts are active during the later init path.

The first mitigation (increasing both `MACHSIZE` and `KSTACK` to 16 KiB) did not change the observed panic yet. Keeping IRQs masked after `timersinit()` also did not move the panic past `YZ`: the latest fresh hardware log still dies before the first late-init breadcrumb after `pageinit()` / `procinit0()`.

That means the next live narrowing is even tighter: distinguish `pageinit()` from `procinit0()`. The last image added a new breadcrumb `0` immediately after `pageinit()` and before `procinit0()`, but the fresh hardware log still stopped at `YZ...panic` with no `0`, which means the failure still happens before `pageinit()` returns.

The current image therefore takes the stronger next step: use a local A733 `clock.c` override with a real `clockshutdown()` that disables `CNTV_CTL_EL0`, call it immediately after `timersinit()`, and only `clockresume()` right before `schedinit()`. This tests whether the remaining `pageinit()`-edge panic is still a live clock interrupt source rather than a purely synchronous fault.

A fresh hardware log still stopped at `YZ...panic` with no `0`, so even that was not enough. The next disassembly-led step was to follow Linux's later ordering more literally: keep `trapinit()` for real EL1 vectors, but defer `intrinit()` / `clockinit()` / `timersinit()` until after `pageinit()` / `procinit0()` / `initseg()` / `links()` / `chandevreset()` / `userinit()`. The reordered image still stopped at `STZ...panic` with no `0`, which means the fault is still inside `pageinit()` or on its immediate return edge.

That points back to a remaining Linux-vs-9front difference in exception masking rather than timer ordering alone: Linux keeps async-abort/debug masking tighter during this window, whereas our `trapinit()` restores the normal runtime DAIF state early. The current image therefore re-masks all DAIF classes with `splx(0xF<<6)` immediately after `trapinit()` / `fpuinit()`, then restores the normal `0x3<<6` state only right before `schedinit()`.

A fresh hardware log still produced only `...STZ panic...` with no `0`, so the failure remains inside `pageinit()` or on its immediate return edge. The current image now adds `pageinit()`-internal sub-markers:

- `5` — entered `pageinit()`
- `6` — finished `np` accumulation before `xalloc(np*sizeof(Page))`
- `7` — `xalloc()` for `palloc.pages` returned
- `8` — finished the page-list build loop
- `9` — finished the memory/swap accounting just before the summary prints

Because a fresh boot still did not show any of those digits, the next image adds a tighter assembly-level wrapper around `pageinit()` itself:

- `Z` — just before calling the wrapper from `main()`
- `5` — emitted in assembly immediately before the actual `BL pageinit(SB)`
- `0` — emitted in assembly only after `pageinit()` returns

The first wrapper version still pushed LR before emitting `5`, so a failure on the call/prologue edge could still hide that marker. The current image removes that ambiguity too: it emits the pre-`pageinit()` `5` inline with no stack push and no nested `BL`, then saves LR only after the marker is out.

A fresh boot then produced `STZ556panic...`, which is the key localization we needed:

- wrapper `5` printed
- in-pageinit `5` printed
- in-pageinit `6` printed
- no `7`

So the failure is inside `xalloc(np*sizeof(Page))`, before that allocation returns.

That points at a structural mapping gap, not an interrupt ordering problem: `pageinit()` allocates a large `Page[]` array via `xallocz()`, and `xallocz()` dereferences `KADDR(pa)` across the whole kernel hole. But our TTBR1 high-half direct map only covered `INITMAP`, not the full KADDR()-reachable low-DRAM window. The current image fixes that by making `mmu0init()` map the full direct-map DRAM span up front (`VDRAM..-KZERO`), much closer to Linux's early linear-map model.

That fix worked: a fresh boot reached `STZ556789...`, proving `xalloc()` returned and `pageinit()` got all the way through allocation, list build, and accounting. The subsequent bootstrap-trap dump then pinned the remaining cliff precisely: it was not inside `pageinit()` at all, but in the wrapper's post-return UART access. `pageinit()` clobbers caller-saved registers, and `pageinitwrap()` was incorrectly reusing `R2` as the UART base after `BL pageinit(SB)`, so the first post-return UART LSR read faulted on a stale address.

The current image fixes that directly by reloading `IOADDR(UART0)` into `R2` immediately after `BL pageinit(SB)` and before the post-return wait/write sequence.

That worked: the next fresh boot reached `...STZ5567890rs...` and then panicked with `todsetfreq: freq 0 <= 0` immediately after `chandevreset()`. That was a direct consequence of the Linux-inspired IRQ/time deferral going too far: `chandevreset()` calls `todinit()`, and `todinit()` expects `clockinit()/timersinit()` to have already established a valid fast-clock frequency. The current image therefore moves `intrinit()` / `clockinit()` / `cpuidprint()` / `timersinit()` back ahead of `chandevreset()` while still keeping them after `pageinit()` / `procinit0()` / `initseg()` / `links()`.

The next fresh boot then moved past that and reached `...rspciaw: stub ... 1234tuUvwxy`, proving the first-process handoff got much farther than expected:

- `u` — entered `userinit()`
- `U` — returned from `userinit()`
- `v` — `mpinit()` returned
- `w` — `mmu1init()` returned
- `x` — restored normal exception mask state
- `y` — reached `schedinit()`

At that point the system goes quiet. The current image therefore instruments the first runnable-process path more directly:

- local `proc.c` emits `j` when `runproc()` selects a process while `up == nil`
- local `proc.c` emits `k` at `linkproc()` entry before invoking the kproc function
- local `userinit.c` emits `i/I/J` around `userinit()` / `kproc()` setup and `l/m` inside `proc0()`
- local `main.c` emits `n/o/p` through `init0()` and just before `touser()`
- `touser()` in `l.s` emits `q` immediately before `ERET`

Those markers should tell us whether the quiet point after `y` is scheduler selection, kproc start, proc0 setup, init0 preparation, or the final user-mode transition.

One important build-system correction was needed here: the automated QEMU kernel rebuild only copies `port/a733/` into the guest build tree, so changing shared files outside that tree is not sufficient on its own.

The local `proc.c` override is now staged deliberately, with the A733 mkfile passing `-I../port` so the extra scheduler headers are available inside the guest build. The smaller local `trap.c` override remains important as well: it prints bootstrap-trap metadata and pid1 syscall/fault state before any generic panic path hides the actual exception.

The mkfile was also tightened with explicit local rules for `page.c`, `proc.c`, `userinit.c`, `trap.c`, and `clock.c` so the A733 overrides are definitely the objects being built.

The latest hardware log confirms the scheduler and first-process instrumentation is live: pid1 is selected, `proc0()` enters `init0()`, the first `touser()` `ERET` occurs, and the first pid1 stack write fault is resolved. The next expected proof point is a `pid1 syscall[...]` trace after returning from that resolved fault.

## Active local overrides in `port/a733/`

Local bring-up overrides now exist for:

- `bootargs.c`
- `clock.c`
- `fns.h`
- `gic.c`
- `io.h`
- `l.s`
- `main.c`
- `mem.c`
- `mem.h`
- `mkfile`
- `page.c`
- `proc.c`
- `trap.c`
- `uartaw.c`
- `userinit.c`

These are the files to inspect first before touching shared `sys/src/9/arm64/` or `sys/src/9/port/` code. The automated QEMU rebuild copies `port/a733/` into `/sys/src/9/a733`, so effective bring-up-only changes must be staged there or explicitly copied by the build workflow.

## Vendor Linux image analysis

The saved Debian image provides a working AArch64 reference path.

Artifacts used:

- kernel image: `/workspace/tmp/orangepi-image/boot/vmlinux-5.15.147-sun60iw2`
- symbols: `/workspace/tmp/orangepi-image/boot/System.map-5.15.147-sun60iw2`
- extracted vendor U-Boot DTB: `bootstrap/orangepi4pro/vendor-debian-1.0.6/extracted/u-boot.dts`
- vendor pack-uboot DTS: `vendors/orangepi-build/external/packages/pack-uboot/sun60iw2/bin/dts/u-boot-current.dts`
- disassembly artifact tree: `docs/disasm/orangepi4pro/vendor-debian-1.0.6/`
- deeper fit-gap note: `docs/FIT-GAP-AUDIT.md`

Useful kernel symbols from the working image:

- `_text` = `ffffffc008000000`
- `_stext` = `ffffffc008010000`
- `primary_entry` = `ffffffc009360000`
- `__enable_mmu` = `ffffffc008e2a31c`
- `__primary_switch` = `ffffffc008e2a3ec`
- `__cpu_setup` = `ffffffc008e2a6d8`

### Key disassembly findings

The working Linux image splits early MMU bring-up into two distinct phases:

1. `__cpu_setup`
   - programs `MAIR_EL1`
   - programs `TCR_EL1`
   - prepares the final `SCTLR_EL1` value in `x0`
2. `__enable_mmu`
   - writes `TTBR0_EL1`
   - writes `TTBR1_EL1`
   - `ISB`
   - writes `SCTLR_EL1`
   - `ISB`
   - invalidates I-cache (`ic iallu`)
   - `DSB NSH`
   - `ISB`
   - returns

This is materially simpler than the current 9front path.

### Most important comparison to our code

`port/a733/l.s` has now been refactored to follow the working Linux shape more closely:

- `_startup` calls `cpusetup<>` first
- `cpusetup<>` programs `MAIR_EL1` and `TCR_EL1`, then returns the final `SCTLR_EL1` value in `R0`
- `mmuenable<>` now only does the Linux-style MMU-on sequence:
  - `TTBR0_EL1`
  - `TTBR1_EL1`
  - `ISB`
  - `SCTLR_EL1`
  - `ISB`
  - `ic iallu`
  - `DSB NSH`
  - `ISB`
- `primaryswitch<>` handles the post-enable high-virtual return path

One concrete bug fell out of this comparison: the earlier local path was reusing `R0` for breadcrumbs immediately before `MSR SCTLR_EL1`, which risked writing the last debug character instead of the Linux-derived control value. The refactor now preserves the returned `SCTLR_EL1` value in a separate register before any debug calls.

### U-Boot / ATF handoff finding

From `u-boot-aw/arch/arm/lib/bootm.c`, the sunxi ARM64 path uses a custom handoff via ATF/BL31 rather than the generic ARM64 boot path. That handoff passes the DTB pointer in `x0` and is consistent with the behavior already observed on serial. It is not currently the leading suspect.

### Page-size sanity check

The saved Debian image kernel config confirms:

- `CONFIG_ARM64_4K_PAGES=y`
- `CONFIG_PGTABLE_LEVELS=3`

So the 9front A733 bring-up should continue assuming 4KB pages / 3-level page tables.

## Recommended next debugging direction

1. Rebuild the current instrumented image and retest on hardware.
2. Look specifically for `pid1 mmuswitch refresh top[255] ...` after the resolved `pid1 fault[0]`; if that line prints a non-zero top entry, the live per-CPU top-level install path was stale even though `up->mmuhead[PTLEVELS-1]` was correct.
3. Then look for a `pid1 syscall[...]` line after the refresh / resolved fault; that is the next sign that EL0 fault return is working.
4. If the system still goes quiet after `pid1 ureg preexit ... psr 0x80`, inspect the final `kexit()` / `fpukexit()` / `ERET` path and the freshly populated pid1 user page tables rather than revisiting early MMU setup.
5. Keep IRQ delivery conservative until the first syscall is visible; the current `trap.c` path masks pid1 IRQs before first syscall to separate timer preemption from user fault return.
6. Defer framebuffer/video, PCIe, storage, and network work until pid1 reliably reaches `/boot/boot` syscalls.
