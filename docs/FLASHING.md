# Flashing / Installing (v1 — Linux-only layout)

Target: Samsung Galaxy Tab S10 FE (SM-X520), bootloader unlocked, slot A.

> **v1 is destructive.** The Ubuntu rootfs replaces the contents of the
> `userdata` partition. Android cannot be restored by re-flashing boot alone —
> recovery requires the full stock firmware via Odin. Back everything up first.
> Dual-boot (Android preserved) is a v2 goal.

## 0. Prerequisites — read all of this first

1. **Stock firmware on hand**: the exact set is
   `AP_X520XXSBCZI2_X520XXSBCZI2_MQB114563728_REV00_user_low_ship_MULTI_CERT_meta_OS16.tar.md5`
   (CSC `XSP`). Verify MD5 before first use. This is your only recovery path.

2. **Binary version (rollback protection) — check before unlocking.**
   Samsung enforces a monotonic binary revision (the `S` letter in
   `X520XXS`**`B`**`CZI2`). Odin **refuses** to flash a firmware with a binary
   lower than what the device already has.
   - Confirm the device's current build: `adb shell getprop ro.build.fingerprint`
     (must contain the same binary letter `B` or lower as your firmware).
   - **Disable auto-update now** (Settings → Software update → turn off
     auto-download). If the tablet later OTA-updates past `B`, the stock `B`
     firmware stops being a valid recovery and you must obtain the newer
     official firmware before unlocking.

3. **Backups** (all before any flash):
   - Personal data off the tablet.
   - Stock partition dumps (your `stock_boot/` set already covers
     `boot`/`init_boot`/`vendor_boot`/`dtbo`/`vbmeta`). Also dump
     `vbmeta_system` and `persist` if present:
     ```sh
     adb root
     for p in vbmeta_system persist; do
       adb shell dd if=/dev/block/by-name/$p of=/data/local/tmp/$p.img bs=4M 2>/dev/null
       adb pull /data/local/tmp/$p.img stock_boot/
     done
     ```

4. **Build complete**: `release/` with `SHA256SUMS`. Verify:
   `cd release && sha256sum -c SHA256SUMS`.

## 1. Unlocking the bootloader

**Verified on the physical device (SM-X520, Android 16):** Developer Options
has *no* OEM-unlock switch on this model; unlocking is done from Download
Mode.

1. Power off. Connect USB to a PC.
2. Hold **Volume Up + Volume Down** while connecting → Download Mode warning.
3. On the warning screen: **long-press Volume Up** → `Device Unlock Mode` →
   confirm. The device factory-resets. Knox warranty bit is set permanently.

**If the unlock option is absent** (VaultKeeper / anti-theft gating on some
Samsung builds): the menu appears only after the device has been on a network
with a synced Samsung account for several days. Connect to Wi-Fi, sign in to
your Samsung account, keep the device on for a few days, then retry.

## 2. Flash path selection — VERIFY before flashing anything

This document assumes **fastboot**. Samsung's primary flashing channel is
Download Mode + Odin, and whether the unlocked SM-X520 actually exposes a
fastboot mode is **not yet verified on hardware**. Do not flash until you have
confirmed:

```sh
adb reboot bootloader
fastboot devices          # the tablet MUST appear here
```

- **If the tablet appears in `fastboot devices`** → continue with §3.
- **If it lands in Download Mode instead** (or `adb reboot bootloader` loops)
  → fastboot is unavailable; use the Odin path in §5.

Also note the **fastboot vs fastbootd** distinction: bootloader-level
`fastboot` may refuse some partitions (notably `userdata`). If a flash is
rejected, retry from userspace fastbootd — which requires the **stock** OS to
still boot:

```sh
adb reboot fastboot       # userspace fastbootd; only works while stock Android runs
```

Plan accordingly: if fastbootd is required, flash `userdata` first (it is the
largest), then the rest.

## 3. Flashing (fastboot path, all `_a` slot)

`vbmeta` is flashed **first, with verification disabled — this is the
primary path, not a fallback.** The stock `vbmeta` image hashes the stock
`boot_a`/`init_boot_a`/etc.; once you replace those with custom images the
digests no longer match and the device will refuse to boot unless AVB
verification is disabled in the flashed `vbmeta` itself.

```sh
# 1. vbmeta — flags patch the image being written; verification off from here on
fastboot --disable-verity --disable-verification flash vbmeta_a stock_vbmeta.img

# 2. rootfs (largest first)
fastboot flash userdata ubuntu-rootfs.img

# 3. board overlays
fastboot flash dtbo_a custom_dtbo.img

# 4. vendor boot — STOCK on purpose (base DTB + vendor ramdisks our dtbo overlays)
fastboot flash vendor_boot_a stock_vendor_boot.img

# 5. initramfs
fastboot flash init_boot_a custom_init_boot.img

# 6. kernel last, then reboot
fastboot flash boot_a custom_boot.img
fastboot reboot
```

Rules:

- **Do not flash** `vbmeta_system_a`, `modem`, `bootloader`, or `persist`.
  Leaving `vbmeta_system_a` stock is safe: the root `vbmeta` carries the
  disable flags, so the child chain is not verified. (Never substitute one
  partition's image for another's — each partition has its own image.)
- The initramfs expects `userdata` to be ext4 with label `ubuntu-rootfs`.
  The prebuilt `ubuntu-rootfs.img` is exactly that filesystem; flashing it
  raw to the partition makes the partition itself the rootfs. No loop images.
- If `fastboot flash userdata` is rejected (active-partition guard), use the
  fastbootd procedure from §2.

## 4. First boot

Watch the console (USB RNDIS gadget or serial harness). Expected `init`
sequence:

- `s10fe-init: ...` lines
- `Root device: /dev/sdaX` (resolved by `PARTNAME=userdata`, LUN 0)
- RO identity gate: label `ubuntu-rootfs` + `/lib/systemd/systemd` present
- `Booting Ubuntu (systemd)` → GDM → `tonton` / `tonton`

The `init` is fail-loud by design: any UFS/insmod/identity failure drops to
an emergency shell; `dmesg | tail -50` and `ls /modules` are the first things
to read.

**Getting back to a flasher from Linux:** the Linux rootfs cannot run
fastbootd. Recovery/flash access is Download Mode (Vol Up + Vol Down + USB →
Odin, §5) or — if verified in §2 — the fastboot key entry.

## 5. Odin path (fallback / recovery)

When fastboot is unavailable, or to restore stock:

1. Download Mode (Vol Up + Vol Down + USB), cable connected to a PC with
   Samsung drivers.
2. **Stock restore**: flash the full stock set (BL/AP/CSC `X520XXSBCZI2`,
   CSC `XSP`) via Odin. Do not tick Re-Partition. This is the only full
   recovery — boot-only reflash does not undo the layout.
3. **Custom images via Odin** (only if the fastboot path is unavailable):
   build a custom AP tar by replacing the `BOOT`, `INIT_BOOT`, `DTBO` and
   `USERDATA` entries of the stock AP tar with the images from `release/`
   (sizes must match the original sections or the partition layout breaks —
   if they do not match, this path is not possible and fastboot is mandatory).
   Flash BL + custom AP + CSC. Keep the stock `vbmeta` **disabled** image —
   since the stock `vbmeta` cannot be patched this way without its original
   flags, flashing a custom AP that changes hashed partitions will fail AVB
   at boot; the stock `vbmeta` from the AP tar must therefore also be
   replaced with a flags-disabled copy (produce one with
   `avbtool`/`vbutil` or extract, patch the `flags` field, and repack).

   ⚠️ The Odin custom-AP route is **not implemented or tested yet**; it is
   documented only as an escape hatch. Validate the fastboot path first.

## 6. What is NOT covered by v1

- Dual-boot switching (v2)
- Wi-Fi (SCSC WLBT) bring-up on Linux — needs the `vendor`-partition firmware
  path wired at boot; expect **no network** at first boot until that lands
- Suspend/resume, fast-charge protocol negotiation
- Camera (front unsupported by the hardware path; rear unverified)
