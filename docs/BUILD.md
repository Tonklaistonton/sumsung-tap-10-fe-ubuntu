# Building

Build Ubuntu 24.04 ARM64 for the Samsung Galaxy Tab S10 FE (SM-X520).

## Host prerequisites (Ubuntu 24.04 / WSL2 x86_64)

```sh
sudo apt update
sudo apt install -y \
  git repo curl wget rsync \
  build-essential bc bison flex \
  libssl-dev libelf-dev \
  dwarves device-tree-compiler \
  python3 python3-pip cpio gzip xz-utils lz4 \
  gcc-aarch64-linux-gnu g++-aarch64-linux-gnu \
  clang lld llvm \
  qemu-user-static squashfs-tools p7zip-full \
  adb fastboot
```

~150 GB free disk is needed (AOSP toolchain + Samsung source + Bazel outputs).

## 1. Toolchain (AOSP common-android15-6.6)

```sh
mkdir -p ~/s10fe-linux/aosp-kernel-toolchain
cd ~/s10fe-linux/aosp-kernel-toolchain
repo init -u https://android.googlesource.com/kernel/manifest -b common-android15-6.6
repo sync -c -j"$(nproc)"
```

Link the toolchain into `Kernel/` (these paths are gitignored):

```sh
cd <repo>/Kernel
ln -s ~/s10fe-linux/aosp-kernel-toolchain/build        build
ln -s ~/s10fe-linux/aosp-kernel-toolchain/external     external
ln -s ~/s10fe-linux/aosp-kernel-toolchain/common-modules common-modules
# prebuilts/* symlinks per AOSP layout (see .gitignore)
```

> The kernel source (3 Samsung layers) is **committed** to this repo under
> `Kernel/`. See `kernel/source.json` for the exact source packages, their
> sha256, and the layer ordering. A clean clone builds without re-downloading
> source.

## 2. Verify inputs

```sh
./build.sh check
```

This checks `manifest.json` hashes: the Ubuntu base tarball, the ARM64 busybox,
and every firmware blob under `Kernel/prebuilts/platform/erd8855/firmware`.
If you change an input on purpose, regenerate hashes:

```sh
tools/make-manifest.sh   # rewrites manifest.json hashes
```

## 3. Kernel

```sh
./build.sh kernel
# == tools/bazel run --config=stamp --config=mx8855_user //exynos/device/mx/mx8855:mx8855_dist
```

Outputs land in `Kernel/out/mx8855/dist/`: `Image`, `dtbo.img`,
`s5e8855.dtb`, and ~286 `.ko` modules. The `s5e8855-bazel_defconfig` is
pre-configured for Ubuntu (Samsung/Android security disabled, systemd/VT/
framebuffer-console enabled) — see `kernel/source.json → config`.

First build takes 30–60 min. Rebuilds reuse Bazel's action cache.

## 4. Initramfs + init_boot

```sh
./build.sh initboot
```

Builds `initramfs_builder/initramfs.cpio.lz4` (busybox + early modules) and
`custom_init_boot.img` (header v4, `os_version 15.0.0`). The `init` script
resolves the root partition by `PARTNAME=userdata`, verifies it read-only
(label `ubuntu-rootfs`, systemd present), then remounts RW and hands over to
systemd. It is fail-loud: any UFS/insmod/identity failure drops to a debug
shell with a `dmesg`-readable message on `/dev/kmsg`.

## 5. Ubuntu rootfs

```sh
sudo ./build.sh rootfs
```

Extracts `ubuntu-base-24.04.5-base-arm64.tar.gz` (pinned), chroots to install
GNOME minimal + NetworkManager + Wayland + pipewire + openssh, copies the 286
kernel modules + firmware blobs, and writes
`ubuntu-noble-rootfs/ubuntu-24.04-arm64.img` (12 GB ext4, label
`ubuntu-rootfs`). Default user `tonton:tonton`, `root:root` — change before
shipping.

## 6. Release set

```sh
./build.sh release
```

Packs `release/`:

| File | Partition | Source |
|---|---|---|
| `custom_boot.img` | `boot_a` | our `Image`, header v4, empty ramdisk |
| `custom_init_boot.img` | `init_boot_a` | our initramfs |
| `custom_dtbo.img` | `dtbo_a` | our board overlays (board rev 6 → r05) |
| `stock_vendor_boot.img` | `vendor_boot_a` | stock (base DTB + vendor ramdisks) |
| `stock_vbmeta.img` | `vbmeta_a` | stock, flashed with verification disabled |
| `ubuntu-rootfs.img` | `userdata` | ext4 rootfs |
| `SHA256SUMS` | — | checksums of all of the above |

## Byte-for-byte note

Reproducible builds require the identical AOSP toolchain revision, identical
Samsung source layers (sha256 in `kernel/source.json`), and identical
`SOURCE_DATE_EPOCH` (`1788864812`). Differences in Bazel version or host
timezone may change timestamps. `SHA256SUMS` in `release/` pins each build.
