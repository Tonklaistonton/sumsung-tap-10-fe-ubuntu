# Ubuntu for Samsung Galaxy Tab S10 FE (SM-X520)

Run **Ubuntu 24.04 LTS (ARM64) with GNOME** on the Samsung Galaxy Tab S10 FE
(Wi-Fi, `SM-X520` / `gts10fewifi`, Exynos 1580 / `mx8855`) using the stock
Samsung kernel build, repackaged for a standard Linux userspace.

> This project is **only for the Samsung SM-X520 (Wi-Fi, board rev r05-r32,
> CSOT NT36523N panel)**. It builds on the Samsung open-source release
> `X520XXSBCZI2` (Android 16, kernel `6.6.102-android15-8-abX520XXSBCZI2-4k`).
> Other models are not compatible.

## Status (v1 — Linux-only layout)

| Component | State |
|---|---|
| Kernel (`Image`, `dtbo.img`, 286 modules) | ✅ built |
| Initramfs + `init_boot.img` | ✅ built |
| `boot.img` (header v4) | ✅ packed |
| Ubuntu 24.04 ARM64 rootfs (GNOME minimal, ext4 label `ubuntu-rootfs`) | ✅ built |
| First boot on hardware | ⏳ pending (bootloader not yet unlocked) |
| Wi-Fi (SCSC WLBT), S-Pen, audio, sensors | 🟡 unverified on Linux |
| Dual-boot Android ↔ Ubuntu | ❌ not in v1 |

## Requirements

- SM-X520 (Wi-Fi), bootloader unlockable via Download Mode
  (`Long press Volume Up: Device Unlock Mode`). **Unlocking wipes the device.**
- Stock firmware `X520XXSBCZI2` (CSC `XSP`) kept for recovery via Odin.
- Build host: Ubuntu 24.04 / WSL2 x86_64, ~150 GB free,
  AOSP kernel toolchain `common-android15-6.6` (see `docs/BUILD.md`).

## Build

```sh
./build.sh check        # verify pinned inputs
./build.sh kernel       # Bazel kernel build -> Kernel/out/mx8855/dist/
./build.sh initboot     # initramfs + custom_init_boot.img
sudo ./build.sh rootfs  # Ubuntu rootfs image
./build.sh release      # pack everything into release/ with SHA256SUMS
```

Or `./build.sh all` (runs check → kernel → initboot; `rootfs` and `release`
require the previous stages' outputs). See `docs/BUILD.md` for toolchain setup
and `kernel/source.json` for source provenance.

## Install

See `docs/FLASHING.md`. Summary: check the binary version (rollback
protection) before unlocking, unlock the bootloader from Download Mode, verify
fastboot is actually available, flash `vbmeta_a` **first** with
`--disable-verity --disable-verification`, then flash `release/*` to the `_a`
slots and write `ubuntu-rootfs.img` to `userdata`. If fastboot is not
available after unlock, the Odin path is documented in the guide.

## Layout

```
repo/
  build.sh                  # top-level orchestrator
  pack_boot_images.sh       # boot chain packer -> release/
  build_custom_init_boot.sh # initramfs + init_boot
  build_ubuntu_arm64_rootfs.sh, package_img.sh
  kernel/source.json        # source provenance (Samsung packages, layers)
  manifest.json             # pinned input hashes (busybox, tarball, firmware)
  tools/                    # verify_manifest.py, make-manifest.sh
  docs/BUILD.md docs/FLASHING.md
  Kernel/                   # Samsung kernel source (3 layers merged) + Bazel glue
  device/configs/           # (future) config fragments
  initramfs_builder/        # busybox + init sources
  stock_boot/               # extracted stock partitions (identity reference)
```

## Recovery

Flash the original `X520XXSBCZI2` stock firmware (BL/AP/CSC, CSC `XSP`) via
Odin. Keep the bootloader **unlocked** while non-stock boot images are
installed. Details: `docs/FLASHING.md`.

## License

Integration layer (this repo's scripts, init, docs): **MIT**.
Samsung kernel source: GPL-2.0 with Samsung extensions (see Samsung open-source
license). Firmware blobs: non-free, from the Samsung open-source release.
See `NOTICE` and `LICENSE`.
