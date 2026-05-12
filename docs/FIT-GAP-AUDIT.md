# Orange Pi 4 Pro fit-gap audit against vendor Debian image

Updated: 2026-05-12

This note compares the current 9front A733 bring-up strategy against the known-good `Orangepi4pro_1.0.6_debian_bookworm_server_linux5.15.147` image from first principles.

Detailed artifacts are kept under:

- `docs/disasm/orangepi4pro/vendor-debian-1.0.6/`

## Scope

Compared components:

- Boot0 (`boot0_sdcard.fex`)
- boot package (`boot_package.fex`)
- extracted vendor U-Boot payload (`u-boot.bin` + `u-boot.dtb`)
- saved Linux boot config / DTB / early kernel binary path
- current 9front local bring-up overrides under `port/a733/`

## Working vendor boot-chain summary

Observed-good path from the saved image and hardware logs:

1. Boot0 from SD
2. BL31 / ATF
3. 32-bit vendor U-Boot (`U-Boot 2018.07-orangepi-config-dirty`)
4. FAT boot partition
5. DTB + ARM64 kernel image load
6. `booti` handoff
7. Linux enters its own early AArch64 startup path successfully

## Key working Linux reference facts

From the saved image config:

- `CONFIG_ARM64_4K_PAGES=y`
- `CONFIG_PGTABLE_LEVELS=3`
- `CONFIG_ARM64_VA_BITS=39`
- `CONFIG_ARM64_PA_BITS=48`

Important symbols from `System.map`:

- `_text = ffffffc008000000`
- `_stext = ffffffc008010000`
- `primary_entry = ffffffc009360000`
- `__enable_mmu = ffffffc008e2a31c`
- `__primary_switch = ffffffc008e2a3ec`
- `__cpu_setup = ffffffc008e2a6d8`

## Disassembly-driven observations

### Linux `__cpu_setup`

The working Linux image clearly splits CPU setup from MMU enable.

`__cpu_setup`:

- programs `MAIR_EL1`
- programs `TCR_EL1`
- returns a prepared `SCTLR_EL1` value in `x0`

This is a materially different strategy from the original 9front arm64 path.

### Linux `__enable_mmu`

The working sequence is compact and regular:

1. `TTBR0_EL1`
2. `TTBR1_EL1`
3. `ISB`
4. `SCTLR_EL1`
5. `ISB`
6. `IC IALLU`
7. `DSB NSH`
8. `ISB`
9. `RET`

This sequence is now mirrored much more closely in `port/a733/l.s`, with `cpusetup<>` split out ahead of time and a tiny `primaryswitch<>` tail to move from the identity-mapped return path to the normal high virtual stack.

## Current 9front breakpoint

The MMU-enable breakpoint has been cleared. Current hardware breadcrumbs now reach pid1's first EL0 fault/return path:

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

- early MMU enable, high-virtual handoff, and C bootstrap now run
- `pageinit()` completes after broadening the early direct map
- IRQ/time setup completes far enough to establish the 24 MHz counter frequency
- pid1 is created, scheduled, enters `proc0()` / `init0()`, and executes the first `ERET` to EL0
- the first user stack write fault is resolved, and the next live gap is returning from that resolved fault and observing the first pid1 syscall

## Fit-gaps identified

### 1. VA width mismatch (fixed locally)

Vendor Linux uses 39-bit VA with 4KB pages and 3 page-table levels.

Earlier 9front experiments used 36-bit VA, which implied a materially different `TCR_EL1` and address-space split. This has now been moved to 39-bit VA in `port/a733/mem.h`.

### 2. PA range clamp (fixed locally)

Linux clamps `PARange` to 48-bit PA (`IPS = 5`) before inserting it into `TCR_EL1`.

The generic 9front path did not do this. The local A733 path now clamps the IPS field to match the working image.

### 3. CPU setup split (fixed locally)

Linux separates:

- CPU setup
- MMU enable

Current A733 bring-up now uses a local `cpusetup<>` routine to match that structure more closely, and `_startup` now calls it before `mmuenable<>` rather than burying it inside the MMU-on transition.

### 4. Low emergency vectors (partially tested)

Because the failure happens right at MMU enable, early EL1 vectors were moved into low identity-mapped DRAM and installed before the transition. No visible `X` exception marker has appeared so far, which suggests the failure may be an immediate translation/fetch collapse rather than a clean vectored exception.

### 5. TTBR0 child-table placement bug (fixed locally)

A previous experiment placed a TTBR0 child table immediately after `L1BOT`, which is unsafe because `L1BOT` only reserves one page and the next page collides with the shared `L1` region.

The next local cleanup also removed the temporary `L1TOP-KZERO` alias trick and moved the TTBR0 child table into a dedicated low DRAM scratch page (`IDMAPL0ADDR = 0x40021000`) just above `REBOOTADDR`. That keeps the TTBR0 idmap structure separate from the TTBR1 root page and matches the Linux mental model more closely.

### 6. `SCTLR_EL1` clobber risk in local debug path (fixed locally)

The disassembly-driven refactor exposed a local bug risk: once `cpusetup<>` started returning the final Linux-derived `SCTLR_EL1` value in `R0`, the earlier breadcrumb path in `mmuenable<>` could overwrite `R0` with `'3'`/`'4'` before the `MSR SCTLR_EL1` write.

Current local code now preserves the returned control value in a separate register before any debug calls in `mmuenable<>`.

### 7. TLB invalidation phase placement (tightened locally)

A fresh refactored boot produced `ABCDE12` and then died before the first old `mmuenable<>` breadcrumb. Linux does its local `tlbi vmalle1` work in `__cpu_setup`, not in `__enable_mmu`, so the local A733 path was tightened to match that split more literally:

- `cpusetup<>`: `TLBI VMALLE1`, `DSB NSH`, then `MAIR_EL1` / `TCR_EL1`
- `mmuenable<>`: emit `0` immediately, then only the TTBR/SCTLR/I-cache sequence

### 8. Nested-BL `LR` bug in `cpusetup<>` (fixed locally)

The `ABCDE12` stop also exposed a plain local assembly bug: `cpusetup<>` calls `debugputcphys<>` with `BL`, but until now it did not preserve `LR`. After the last breadcrumb call, `RETURN` jumped back into `cpusetup<>`'s own tail instead of returning to `_startup`.

That exactly explains why `1` and `2` printed but `0` never appeared. `cpusetup<>` now saves/restores `LR` around its nested debug calls.

### 9. Nested-BL `LR` bug in `mmuenable<>` (fixed locally)

Once `cpusetup<>` was fixed, hardware progressed to:

```text
ABCDE120345345345...
```

That pattern means the MMU transition itself succeeds far enough to print `5`, but the post-enable tail returns into the middle of `mmuenable<>` and repeats the `3/4/5` sequence.

The local cause was the same class of bug in a slightly different place: `mmuenable<>` only stacked `LR` after its first `BL debugputcphys<>`, so it saved the return address of that nested call rather than the caller's return address from `_startup`. Current code now saves the caller's `LR` before any nested `BL` in `mmuenable<>`.

### 10. Early DTB parse used unmapped TTBR1 alias (fixed locally)

After the `mmuenable<>` return-path fix, hardware progressed to:

```text
ABCDE120345FGHIJKLMX
```

The added `main()` breadcrumbs place the next exception after `quotefmtinstall()` and before `bootargsinit()` returns. The most direct fit-gap is that `bootargsinit()` was still using `KADDR(bootdtb)` very early, before `meminit()` had expanded the TTBR1 high-half mapping beyond the small `INITMAP` window.

On this board the DTB lives at `0x43000000`, which remains accessible through the temporary TTBR0 identity mapping but is not yet guaranteed to be reachable through the early TTBR1 alias. The local fix is to parse the DTB via the identity-mapped low VA (`va = (void*)pa`) during `bootargsinit()`.

When that did not immediately move the hardware breadcrumbs, the next step was to instrument `bootargsinit()` itself with sub-step UART markers. Hardware then produced `ABCDE120345FGHIJKLMabcegX`, which localizes the remaining failure to inside `parsedevtree()` rather than somewhere higher-level in `main()`.

The next step was correspondingly narrower: instrument `parsedevtree()` itself. Hardware then produced `ABCDE120345FGHIJKLMabcegpX`, which shows the fault happens on the very first DTB header access inside `parsedevtree()`, before the initial header check completes.

For continued bring-up, the current local strategy is pragmatic: bypass early DTB parsing temporarily in `bootargsinit()` and seed board-known fallback values (`*maxmem = 4 GiB`, `*ncpu = 1`) so the rest of kernel init can proceed. Once later bring-up is stable, the DTB parser fault can be revisited with the same fine-grained breadcrumbs.

That bypass did unlock later init. A subsequent hardware boot reached all the way through `bootargsinit()`, `meminit()`, `confinit()`, `xinit()`, `printinit()`, `trapinit()`, `fpuinit()`, `intrinit()`, `clockinit()`, `timersinit()`, and at least into the later init path, then panicked in `kenter()` with `-120 stack bytes left` on the mach stack while still in bootstrap.

The first mitigation was to enlarge both `MACHSIZE` and `KSTACK` from 8 KiB to 16 KiB and keep late-init breadcrumbs. Since the observed hardware panic signature did not change yet, the next pragmatic mitigation was to keep IRQs masked after `timersinit()` and only unmask immediately before `schedinit()`, so timer/IRQ entry cannot eat the mach stack during `pageinit()` / `procinit0()` / `links()` / `chandevreset()` / `userinit()` / `mpinit()` / `mmu1init()`.

A fresh hardware log still panicked at `YZ` before the first late-init breadcrumb after `pageinit()` / `procinit0()`, so the next narrowing was stricter still: add a dedicated marker immediately after `pageinit()` and before `procinit0()` to distinguish those two paths.

That additional marker still did not appear on hardware, which means the failure remains before `pageinit()` returns. The next mitigation therefore stopped being observational and became structural again: use a local A733 clock implementation with a real `clockshutdown()` that disables `CNTV_CTL_EL0`, then keep the timer source off for the entire deep bootstrap window and only `clockresume()` right before `schedinit()`.

When even that did not move the fresh hardware panic past `YZ`, the next step returned to the Linux control path ordering: Linux's `System.map` shows `init_IRQ`, `time_init`, and `tick_init` well after `start_kernel`, not interleaved with the earliest bootstrap bookkeeping the way current 9front does it. The current A733 image therefore keeps `trapinit()` installed but defers `intrinit()` / `clockinit()` / `timersinit()` until after `pageinit()` / `procinit0()` / `initseg()` / `links()` / `chandevreset()` / `userinit()`, with fresh markers `1 2 3 4` to show how far that later IRQ/time-init window gets.

A fresh hardware log still stopped earlier at `STZ...panic` with no `0`, so the issue is now narrower than IRQ/time ordering alone. The remaining Linux-shaped difference is exception masking: Linux keeps early DAIF handling tighter, while 9front's `trapinit()` restores the normal runtime state immediately. The current local experiment re-masks all DAIF classes (`splx(0xF<<6)`) immediately after `trapinit()` / `fpuinit()` and only restores the usual `0x3<<6` state right before `schedinit()`.

Since a fresh hardware log still did not show `0`, the current image adds `pageinit()`-internal markers (`5 6 7 8 9`) so the next boot can distinguish a failure in the initial `xalloc()` for `palloc.pages` from a failure in the later page-list/accounting loops or on the summary-print edge.

Because even a fresh boot still showed only `STZ...panic` with none of those digits visible, the next step goes one level lower: wrap `pageinit()` in assembly and emit `5` immediately before the actual `BL pageinit(SB)`, then `0` only after it returns. That tells us whether the failure is happening on the call/prologue boundary itself rather than inside the C body. The local mkfile now also has an explicit `page.$O: page.c` rule to remove any doubt that the A733 page override is the object being linked.

The first wrapper version still saved LR before emitting `5`, so the present image tightens that further: the pre-`pageinit()` `5` is now written inline to UART with no stack push and no nested `BL`, and only then does the wrapper save LR and branch into `pageinit()`.

That paid off immediately: hardware produced `STZ556panic...`, proving the failure is inside `xalloc(np*sizeof(Page))` in `pageinit()` before the allocation returns. This strongly suggests the remaining gap is the breadth of the early TTBR1 direct map, not the page allocator logic itself.

The current fix therefore broadens `mmu0init()` from `INITMAP` to the full KADDR()-reachable direct-map DRAM span (`VDRAM..-KZERO`). That matches the way Linux's early linear map is usable for sizeable bootstrap allocations before the generic allocator is fully online, and it should let `xallocz()` zero the `Page[]` metadata array without walking off the mapped high-half window.

A fresh hardware boot confirmed exactly that: `STZ556789...` showed `xalloc()` returned and `pageinit()` completed allocation, page-list construction, and accounting. The bootstrap-trap dump then pinpointed the remaining fault: it was not inside `pageinit()` at all, but in the wrapper's post-return UART access. `pageinit()` clobbers caller-saved registers, and `pageinitwrap()` was incorrectly reusing `R2` as the UART base after `BL pageinit(SB)`, so the first post-return UART LSR read faulted on a stale address.

The current fix is correspondingly small and direct: reload `IOADDR(UART0)` into `R2` immediately after `BL pageinit(SB)` and before the post-return wait/write sequence.

That fix immediately moved the next failure deeper: the subsequent fresh boot reached `pageinit()` return (`0`) and then panicked in `todsetfreq()` during `chandevreset()`. That exposed one place where the Linux-shaped deferral had overshot: `chandevreset()` calls `todinit()`, and `todinit()` requires a valid fast-clock frequency from `clockinit()/timersinit()`. The current bootstrap order therefore restores `intrinit()` / `clockinit()` / `cpuidprint()` / `timersinit()` to *before* `chandevreset()`, while still keeping them after `pageinit()` / `procinit0()` / `initseg()` / `links()`.

With that fixed, the next fresh boot reached `...1234tuUvwxy`, so the old null-branch cliff was gone and the system now goes quiet only after `schedinit()` is reached. That shifts the bring-up problem out of early bootstrap and into the first runnable-process path.

The current image therefore instruments that path directly: `runproc()` selection (`j`), `linkproc()` entry (`k`), `userinit()` / `kproc()` (`i/I/J`), `proc0()` (`l/m`), `init0()` (`n/o/p`), and the final `touser()` `ERET` edge (`q`).

Because the automated rebuild only copies `port/a733/` into the guest build tree, any effective bring-up-only override must live under `port/a733/`, not only in shared source. The A733 mkfile now builds local overrides for `clock.c`, `page.c`, `proc.c`, `trap.c`, and `userinit.c`; `proc.c` is built with `-I../port` so its scheduler dependencies are available inside the guest tree. The local `trap.c` override reports bootstrap traps plus pid1 fault/syscall state before generic panic paths hide the real exception.

The latest hardware log confirms those overrides are active: scheduler handoff, kproc entry, first-process setup, `touser()`, and pid1's first resolved user fault are all visible. The remaining fit-gap has therefore moved from EL1 MMU construction into the first EL0 fault-return/syscall path.

## Gaps still open

### A. First EL0 fault-return/syscall path

The current hardware log resolves pid1's first user stack write fault and reaches the trap-exit path (`pid1 ureg preexit ... psr 0x80`), but no first `pid1 syscall[...]` trace has been captured yet. This is now the primary live gap.

### B. Early DTB parser re-enable

Early DTB parsing is still bypassed in `bootargsinit()` in favor of board-known fallback values. Once pid1 reaches `/boot/boot` reliably, revisit the `parsedevtree()` fault and remove the fallback.

### C. Exact vendor `TCR_EL1` semantics

Even after matching Linux much more closely, there may still be fields in the vendor Linux `TCR_EL1` value that are not yet fully reasoned through in the 9front implementation. This is no longer blocking the initial boot path but should be cleaned up before the port is considered stable.

### D. Exact vendor `SCTLR_EL1` derivation

Current local code uses the Linux-observed `SCTLR_EL1` value directly, but not yet a fully derived symbolic reconstruction of why every bit is needed for this platform.

### E. Device bring-up after pid1

PCIe, storage, framebuffer, USB, Ethernet, and SD/eMMC should remain deferred until the first user process and `/boot/boot` path are stable.

## Conclusion

The current best understanding is:

- boot media, DTB loading, and `booti` wrapper format are all working
- handoff into the kernel is real
- the Linux-shaped CPU setup / MMU enable split is validated on hardware
- broadening the early TTBR1 direct map fixed the `pageinit()` / `xalloc()` cliff
- the system now reaches pid1, enters EL0, and resolves its first user stack write fault
- the remaining primary gap is the first EL0 fault-return/syscall path, not the original MMU-enable transition

## Artifact tree

See:

- `docs/disasm/orangepi4pro/vendor-debian-1.0.6/README.md`
- `docs/disasm/orangepi4pro/vendor-debian-1.0.6/boot0/`
- `docs/disasm/orangepi4pro/vendor-debian-1.0.6/boot-package/`
- `docs/disasm/orangepi4pro/vendor-debian-1.0.6/u-boot/`
- `docs/disasm/orangepi4pro/vendor-debian-1.0.6/linux/`
