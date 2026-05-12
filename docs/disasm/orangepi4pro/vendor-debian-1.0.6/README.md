# Orange Pi 4 Pro vendor boot-chain disassembly tree

This tree keeps the disassembly and comparison artifacts used to audit the
known-good `Orangepi4pro_1.0.6_debian_bookworm_server_linux5.15.147` image
against the current 9front A733 bring-up code.

## Layout

- `boot0/` — boot0 header, strings, and entry disassembly
- `boot-package/` — package header and contained component offsets
- `u-boot/` — extracted U-Boot header/strings/disassembly plus relevant source
- `linux/` — saved-image boot config, key symbols, and targeted disassembly for
  `primary_entry`, `__cpu_setup`, `__enable_mmu`, and `__primary_switch`

These artifacts are intentionally targeted rather than full-binary dumps so the
interesting bring-up paths remain reviewable in the repo.
