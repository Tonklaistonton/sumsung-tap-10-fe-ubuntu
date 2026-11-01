#!/usr/bin/env bash
# ==============================================================================
# Pack flashable boot chain for SM-X520 (Linux-only layout v1)
#
# Partition map (Android 16 GKI, SM-X520):
#   boot_a        <- custom_boot.img    (our Image, header v4, no ramdisk)
#   init_boot_a   <- custom_init_boot.img (busybox initramfs)
#   vendor_boot_a <- stock vendor_boot.img (base DTB 267KB + vendor ramdisks)
#   dtbo_a        <- custom_dtbo.img    (our board overlay set, board rev 6 -> r05)
#   vbmeta_a      <- stock vbmeta, flashed with --disable-verity --disable-verification
#   userdata      <- ubuntu-24.04-arm64.img (ext4, label ubuntu-rootfs)
# ==============================================================================

set -euo pipefail

RED='\033[0;31m'; GREEN='\033[0;32m'; BLUE='\033[0;34m'; NC='\033[0m'
die() { echo -e "${RED}[FAIL]${NC} $*" >&2; exit 1; }

BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DIST_DIR="$BASE_DIR/Kernel/out/mx8855/dist"
STOCK_DIR="$BASE_DIR/stock_boot"
OUT_DIR="$BASE_DIR/release"
MKBOOTIMG="$BASE_DIR/Kernel/tools/mkbootimg/mkbootimg.py"
IMAGE="$DIST_DIR/Image"
DTBO="$DIST_DIR/dtbo.img"
VENDOR_BOOT_STOCK="$STOCK_DIR/vendor_boot.img"
VBMETA_STOCK="$STOCK_DIR/vbmeta.img"
INIT_BOOT="$BASE_DIR/custom_init_boot.img"
ROOTFS_IMG="$BASE_DIR/ubuntu-noble-rootfs/ubuntu-24.04-arm64.img"

# --- 1. Verify inputs -------------------------------------------------------
[ -f "$IMAGE" ]        || die "missing $IMAGE (build kernel first)"
[ -f "$DTBO" ]         || die "missing $DTBO (build kernel first)"
[ -f "$VENDOR_BOOT_STOCK" ] || die "missing $VENDOR_BOOT_STOCK"
[ -f "$VBMETA_STOCK" ]      || die "missing $VBMETA_STOCK"
[ -f "$INIT_BOOT" ]        || die "missing $INIT_BOOT (run ./build_custom_init_boot.sh)"
[ -f "$ROOTFS_IMG" ]       || die "missing $ROOTFS_IMG (run ./build_ubuntu_arm64_rootfs.sh)"

# DTB sanity: our built DTB must be present in dist (informational; the base
# DTB actually shipped is the one inside stock vendor_boot.img, which comes
# from the same Samsung source tree)
[ -f "$DIST_DIR/s5e8855.dtb" ] || echo -e "${RED}[WARN]${NC} dist DTB not found (vendor_boot carries the base DTB; dtbo.img must still be from the same build)"

# --- 2. Pack boot.img (kernel only, empty ramdisk, header v4) ----------------
mkdir -p "$OUT_DIR"
EMPTY_RAMDISK="$OUT_DIR/empty_ramdisk.cpio"
# Valid empty newc archive (two TRAILER!!! records) — mirrors stock boot.img,
# which carries an empty ramdisk.
true | cpio -o -H newc > "$EMPTY_RAMDISK"

python3 "$MKBOOTIMG" \
    --header_version 4 \
    --kernel "$IMAGE" \
    --ramdisk "$EMPTY_RAMDISK" \
    --os_version 15.0.0 \
    --os_patch_level 2026-09 \
    --pagesize 4096 \
    -o "$OUT_DIR/custom_boot.img"
echo -e "${GREEN}[OK]${NC} boot.img:     $OUT_DIR/custom_boot.img ($(du -h "$OUT_DIR/custom_boot.img" | cut -f1))"

# --- 3. Copy remaining images ------------------------------------------------
cp -f "$INIT_BOOT"        "$OUT_DIR/custom_init_boot.img"
cp -f "$DTBO"             "$OUT_DIR/custom_dtbo.img"
cp -f "$VENDOR_BOOT_STOCK" "$OUT_DIR/stock_vendor_boot.img"
cp -f "$VBMETA_STOCK"      "$OUT_DIR/stock_vbmeta.img"
cp -f "$ROOTFS_IMG"       "$OUT_DIR/ubuntu-rootfs.img"

# --- 4. Checksums ------------------------------------------------------------
(
    cd "$OUT_DIR"
    sha256sum custom_boot.img custom_init_boot.img custom_dtbo.img \
              stock_vendor_boot.img stock_vbmeta.img ubuntu-rootfs.img \
              > SHA256SUMS
)
echo -e "${GREEN}[OK]${NC} SHA256SUMS written to $OUT_DIR/SHA256SUMS"

echo -e "${GREEN}[OK]${NC} Release set ready in $OUT_DIR:"
ls -lh "$OUT_DIR" | sed 's/^/    /'
