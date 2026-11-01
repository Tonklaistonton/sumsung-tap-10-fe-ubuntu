# Flashing / Installing (v1 — Linux-only layout)

Target: Samsung Galaxy Tab S10 FE (SM-X520), bootloader unlocked, slot A.

> **v1 is destructive.** The Ubuntu rootfs replaces the contents of the
> `userdata` partition. Android cannot be restored by re-flashing boot alone —
> you must restore the full stock firmware via Odin. Back everything up first.
> Dual-boot (Android preserved) is a v2 goal.

## Prerequisites

1. **Stock firmware on hand**: `AP_X520XXSBCZI2...tar.md5` (CSC `XSP`).
   Verify MD5 before first use. This is your recovery path.
2. **Backups**: all personal data off the tablet, and (if already unlocked)
   a dump of the stock `boot_a`, `init_boot_a`, `vendor_boot_a`, `dtbo_a` and
   `persist`/`modem` partitions.
3. **Build complete**: `release/` directory with `SHA256SUMS` from `build.sh`.
   Verify: `cd release && sha256sum -c SHA256SUMS`.

## Unlocking the bootloader

1. Power off. Connect USB to a PC with Samsung drivers.
2. Hold **Volume Up + Volume Down** and connect — Download Mode warning screen.
3. On the warning, **long-press Volume Up** → `Device Unlock Mode` → confirm.
4. Device factory-resets. Knox warranty bit is set permanently.

## Flashing (fastboot)

Boot to fastboot (hold Vol Down + power, or `adb reboot bootloader`).
Flash order matters; all go to the **_a** slot:

```sh
# 1. Verification off (AVB): flash vbmeta first, disabled
fastboot flash vbmeta_a stock_vbmeta.img
fastboot flash vbmeta_system_a stock_vbmeta.img   # if present on your partition table

# 2. Boot chain
fastboot flash boot_a custom_boot.img
fastboot flash init_boot_a custom_init_boot.img
fastboot flash vendor_boot_a stock_vendor_boot.img
fastboot flash dtbo_a custom_dtbo.img

# 3. Rootfs onto userdata (Linux-only: userdata becomes the ext4 root)
fastboot flash userdata ubuntu-rootfs.img
```

Notes:
- `vendor_boot_a` uses the **stock** image on purpose: it carries the base
  DTB (267 KB) and the vendor ramdisks our `dtbo_a` overlays. Our kernel
  (`boot_a`) is the only non-stock boot payload.
- If `fastboot` refuses due to AVB even after the vbmeta flash, retry with
  `fastboot --disable-verity --disable-verification flash vbmeta_a ...`.
- Do **not** flash `modem`, `bootloader`, or `persist` — keep them stock.

## Writing the rootfs (userdata)

The initramfs resolves root as **ext4 with label `ubuntu-rootfs` directly on
the `userdata` partition** (by PARTNAME, verified RO, then RW). There is no
loop-image path.

**Recommended (v1):** flash the prebuilt ext4 image raw to `userdata`. The
image IS a complete ext4 filesystem with the label, so the partition becomes
the rootfs as-is:
```sh
fastboot flash userdata ubuntu-rootfs.img
```
The init then finds `PARTNAME=userdata` → verifies label `ubuntu-rootfs` →
mounts RW → systemd.

**Alternative:** format `userdata` yourself (`mkfs.ext4 -L ubuntu-rootfs`)
and `rsync` the rootfs tree in — useful if you need a larger partition than
the 12 GB image.

## First boot

```sh
fastboot reboot
```

Watch serial/console (USB RNDIS gadget or a JTAG/serial harness):
- `s10fe-init: [...]` lines from the initramfs
- `Root device: /dev/sdaX` (PARTNAME=userdata)
- `Identity verified` → `Booting Ubuntu (systemd)`
- GDM login → user `tonton` / `tonton`

If you land in the emergency shell, `dmesg | tail -50` and `ls /modules` are
the first things to read. The init is fail-loud by design.

## Recovery (back to Android)

1. Boot to Download Mode (Vol Up + Vol Down + USB).
2. Flash the stock `X520XXSBCZI2` (BL/AP/CSC `XSP`) via Odin on Windows.
3. Re-lock the bootloader if you wish (requires stock state).

Keeping the bootloader **unlocked** is mandatory while any non-stock boot
image is present.

## What is NOT covered by v1

- Dual-boot switching (v2)
- Wi-Fi (SCSC WLBT) bring-up on Linux — needs the `vendor` partition firmware
  path wired at boot; expect no network until that lands
- Suspend/resume, fast-charge protocol negotiation
- Camera (front unsupported by hardware path; rear unverified)
