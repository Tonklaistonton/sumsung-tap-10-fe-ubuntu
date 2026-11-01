#!/usr/bin/env bash
# ==============================================================================
# Build initramfs + custom_init_boot.img for Samsung SM-X520 (GTS10FE)
# Target: Exynos 1580 (s5e8855 / mx8855)
#
# Rootfs contract (Linux-only layout v1):
#   Ubuntu ext4 with label "ubuntu-rootfs" lives directly on the UFS
#   partition whose PARTNAME is "userdata" (first LUN, /dev/sda*).
#   No loop images. Identity is verified read-only before RW mount.
# ==============================================================================

set -euo pipefail

RED='\033[0;31m'; GREEN='\033[0;32m'; BLUE='\033[0;34m'; NC='\033[0m'

BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DIST_DIR="$BASE_DIR/Kernel/out/mx8855/dist"
BUSYBOX_BIN="$BASE_DIR/initramfs_builder/extracted/usr/bin/busybox"
INITRAMFS_ROOT="$BASE_DIR/initramfs_builder/root"
RAMDISK_LZ4="$BASE_DIR/initramfs_builder/initramfs.cpio.lz4"
OUTPUT_IMG="$BASE_DIR/custom_init_boot.img"
MKBOOTIMG="$BASE_DIR/Kernel/tools/mkbootimg/mkbootimg.py"

echo -e "${BLUE}[INFO]${NC} 1. Preparing clean initramfs directory structure..."
rm -rf "$INITRAMFS_ROOT"
mkdir -p "$INITRAMFS_ROOT"/{bin,sbin,dev,proc,sys,mnt/rootfs,modules,etc}

# 2. Install busybox and essential symlinks
echo -e "${BLUE}[INFO]${NC} 2. Installing static busybox (ARM64)..."
cp "$BUSYBOX_BIN" "$INITRAMFS_ROOT/bin/busybox"
chmod 755 "$INITRAMFS_ROOT/bin/busybox"
ln -sf busybox "$INITRAMFS_ROOT/bin/sh"

# 3. Copy early critical kernel modules
echo -e "${BLUE}[INFO]${NC} 3. Copying early boot modules (UFS, Display, Touch, USB)..."
EARLY_MODULES=(
    "ufs-sec-driver.ko"
    "ufs-exynos-core.ko"
    "drm_ttm_helper.ko"
    "drm_display_helper.ko"
    "exynos-drm.ko"
    "mcd-panel-nt36523n_gts10fe_csot.ko"
    "novatek_ts_nt36523.ko"
    "phy-exynos-usbdrd-super.ko"
    "dwc3-exynos-usb.ko"
    "usb_f_rndis_mp.ko"
)

for mod in "${EARLY_MODULES[@]}"; do
    if [ -f "$DIST_DIR/$mod" ]; then
        cp "$DIST_DIR/$mod" "$INITRAMFS_ROOT/modules/"
        echo "  [+] Copied: $mod"
    else
        echo "  [-] Warning: $mod not found in dist"
    fi
done

# 4. Create the /init boot loader script
echo -e "${BLUE}[INFO]${NC} 4. Creating /init script..."
cat << 'EOF' > "$INITRAMFS_ROOT/init"
#!/bin/busybox sh
# ==============================================================================
# Samsung Galaxy Tab S10 FE (SM-X520) Early Linux Initializer
# Layout: Linux-only. Rootfs = ext4 label "ubuntu-rootfs" on PARTNAME userdata.
# Storage gate: resolve by PARTNAME, verify identity RO, then RW. Fail loud.
# ==============================================================================

BB=/bin/busybox
PATH=/bin:/sbin:/usr/bin:/usr/sbin
export PATH

log() {
    echo "s10fe-init: $*" >/dev/console 2>/dev/null || true
    echo "s10fe-init: $*" >/dev/kmsg 2>/dev/null || true
}

die() {
    log "FATAL: $*"
    log "Dropping to emergency shell (lsblk, blkid, dmesg, /modules available)."
    exec $BB sh
}

$BB --install -s /bin || die "busybox --install failed"

# Mount kernel virtual filesystems
mount -t proc proc /proc || die "mount /proc failed"
mount -t sysfs sys /sys || die "mount /sys failed"
mount -t devtmpfs devtmpfs /dev || die "mount /dev failed"
mkdir -p /dev/pts /dev/shm /mnt/rootfs
mount -t devpts devpts /dev/pts 2>/dev/null || true

log "============================================================"
log "Samsung Galaxy Tab S10 FE (SM-X520) Linux Loader (v1)"
log "============================================================"

# Verbose kernel messages on console
echo 7 > /proc/sys/kernel/printk 2>/dev/null || true

# --- Storage: FATAL if any step fails (no disk => no root) ---
for m in ufs-sec-driver ufs-exynos-core; do
    [ -f "/modules/$m.ko" ] || die "missing /modules/$m.ko (UFS driver not built into initramfs)"
    insmod "/modules/$m.ko" || die "insmod $m failed (check dmesg)"
done
mdev -s
log "[+] UFS storage drivers loaded"

# --- Display / Touch / USB: non-fatal (bring-up continues without them) ---
for m in drm_ttm_helper drm_display_helper exynos-drm mcd-panel-nt36523n_gts10fe_csot \
         novatek_ts_nt36523 phy-exynos-usbdrd-super dwc3-exynos-usb usb_f_rndis_mp; do
    if [ -f "/modules/$m.ko" ]; then
        insmod "/modules/$m.ko" || log "WARN: insmod $m failed (check dmesg)"
    else
        log "WARN: module $m.ko not in initramfs"
    fi
done
mdev -s

# --- Wait for storage to enumerate ---
log "[*] Waiting for block devices..."
sleep 2

# Resolve root partition by PARTNAME on the first UFS LUN (sda*).
# partname lives at /sys/block/<dev>/<part>/partname, or on the whole
# /sys/block/<dev>/partname for single-partition LUNs.
find_userdata() {
    for pn in /sys/block/sda/partname /sys/block/sda/*/partname; do
        [ -r "$pn" ] || continue
        if [ "$(cat "$pn" 2>/dev/null)" = "userdata" ]; then
            d=$(dirname $(dirname "$pn"))
            echo "/dev/$(basename "$d")"
            return 0
        fi
    done
    return 1
}

ROOT_DEV=""
i=0
while [ $i -lt 15 ]; do
    ROOT_DEV=$(find_userdata) && break
    i=$((i + 1)); sleep 1
done
[ -n "$ROOT_DEV" ] || die "partition PARTNAME=userdata (LUN 0) never appeared (lsblk?)"
log "[+] Root device: $ROOT_DEV"

# --- Identity gate: read-only first ---
mount -t ext4 -o ro "$ROOT_DEV" /mnt/rootfs || die "RO mount of $ROOT_DEV failed (wrong fs?)"

LABEL=$($BB blkid -s LABEL -o value "$ROOT_DEV" 2>/dev/null || true)
if [ -n "$LABEL" ] && [ "$LABEL" != "ubuntu-rootfs" ]; then
    die "unexpected filesystem label '$LABEL' on $ROOT_DEV (expected 'ubuntu-rootfs')"
fi
if [ ! -e /mnt/rootfs/lib/systemd/systemd ] && [ ! -e /mnt/rootfs/sbin/init ]; then
    die "$ROOT_DEV is not an Ubuntu rootfs (no /lib/systemd/systemd)"
fi
log "[+] Identity verified (label='${LABEL:-<none>}', systemd present)"

# --- Remount read-write and hand over ---
umount /mnt/rootfs
mount -t ext4 -o rw,noatime "$ROOT_DEV" /mnt/rootfs || die "RW remount of $ROOT_DEV failed"
log "[+] Booting Ubuntu (systemd)..."
exec $BB switch_root /mnt/rootfs /lib/systemd/systemd
EOF

chmod 755 "$INITRAMFS_ROOT/init"

# 5. Pack CPIO archive and compress with LZ4 Legacy format
echo -e "${BLUE}[INFO]${NC} 5. Packing CPIO archive and compressing with LZ4 Legacy format..."
(
    cd "$INITRAMFS_ROOT"
    find . -mindepth 1 | cpio -o -H newc | lz4 -l -12 > "$RAMDISK_LZ4"
)
echo -e "${GREEN}[OK]${NC} Ramdisk packed: $(ls -lh "$RAMDISK_LZ4" | awk '{print $5}')"

# 6. Generate custom_init_boot.img with mkbootimg
echo -e "${BLUE}[INFO]${NC} 6. Generating $OUTPUT_IMG with mkbootimg..."
python3 "$MKBOOTIMG" \
    --header_version 4 \
    --ramdisk "$RAMDISK_LZ4" \
    --os_version 15.0.0 \
    --os_patch_level 2026-09 \
    -o "$OUTPUT_IMG"

echo -e "${GREEN}[OK]${NC} custom_init_boot.img generated: $(ls -lh "$OUTPUT_IMG" | awk '{print $5}')"
