#!/usr/bin/env bash
# ==============================================================================
# Build Custom Linux initramfs and custom_init_boot.img for Samsung SM-X520
# Target: Samsung Galaxy Tab S10 FE (Wi-Fi), Exynos 1580 (s5e8855 / mx8855)
# ==============================================================================

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

BASE_DIR="/home/tonton/rom/samsung"
DIST_DIR="$BASE_DIR/Kernel/out/mx8855/dist"
BUSYBOX_BIN="$BASE_DIR/initramfs_builder/extracted/usr/bin/busybox"
INITRAMFS_ROOT="$BASE_DIR/initramfs_builder/root"
RAMDISK_LZ4="$BASE_DIR/initramfs_builder/initramfs.cpio.lz4"
OUTPUT_IMG="$BASE_DIR/custom_init_boot.img"
MKBOOTIMG="$BASE_DIR/Kernel/tools/mkbootimg/mkbootimg.py"

echo -e "${BLUE}[INFO]${NC} 1. Preparing clean initramfs directory structure..."
rm -rf "$INITRAMFS_ROOT"
mkdir -p "$INITRAMFS_ROOT"/{bin,sbin,dev,proc,sys,mnt/rootfs,mnt/storage,modules,etc}

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
# ==============================================================================

export PATH="/bin:/sbin:/usr/bin:/usr/sbin"
/bin/busybox --install -s /bin

# Mount kernel virtual filesystems
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev
mkdir -p /dev/pts /dev/shm
mount -t devpts devpts /dev/pts

echo "================================================================"
echo "    Samsung Galaxy Tab S10 FE (SM-X520) Linux Loader Started   "
echo "================================================================"

# Enable all console kernel messages
echo 7 > /proc/sys/kernel/printk

# Load early hardware modules
echo "[*] Initializing UFS storage controller..."
insmod /modules/ufs-sec-driver.ko 2>/dev/null || true
insmod /modules/ufs-exynos-core.ko 2>/dev/null || true

echo "[*] Initializing CSOT NT36523N Display & Exynos DRM..."
insmod /modules/drm_ttm_helper.ko 2>/dev/null || true
insmod /modules/drm_display_helper.ko 2>/dev/null || true
insmod /modules/exynos-drm.ko 2>/dev/null || true
insmod /modules/mcd-panel-nt36523n_gts10fe_csot.ko 2>/dev/null || true

echo "[*] Initializing Novatek Touchscreen..."
insmod /modules/novatek_ts_nt36523.ko 2>/dev/null || true

echo "[*] Initializing USB subsystem..."
insmod /modules/phy-exynos-usbdrd-super.ko 2>/dev/null || true
insmod /modules/dwc3-exynos-usb.ko 2>/dev/null || true

# Populate device nodes
mdev -s

# Wait for storage block devices to enumerate
echo "[*] Waiting for block devices to settle..."
sleep 2

# Search for Ubuntu RootFS partition by filesystem label
ROOT_DEV=""
for dev in $(blkid | grep -i "ubuntu-rootfs" | cut -d: -f1); do
    ROOT_DEV="$dev"
    break
done

# If not found by label, check common block partitions
if [ -z "$ROOT_DEV" ]; then
    for cand in /dev/mmcblk0p1 /dev/mmcblk1p1 /dev/mmcblk0 /dev/block/by-name/userdata; do
        if [ -b "$cand" ]; then
            ROOT_DEV="$cand"
            break
        fi
    done
fi

echo "[*] Target device: $ROOT_DEV"

# Attempt mount
mkdir -p /mnt/rootfs
if [ -n "$ROOT_DEV" ]; then
    if mount -t ext4 -o rw,noatime "$ROOT_DEV" /mnt/rootfs 2>/dev/null; then
        echo "[+] Mounted rootfs filesystem directly from $ROOT_DEV"
    else
        # Try checking for raw image inside partition
        mkdir -p /mnt/storage
        if mount -o ro "$ROOT_DEV" /mnt/storage 2>/dev/null; then
            if [ -f /mnt/storage/ubuntu-24.04-arm64.img ]; then
                echo "[+] Found loop image on $ROOT_DEV, attaching..."
                losetup -f /mnt/storage/ubuntu-24.04-arm64.img
                LOOP=$(losetup -a | grep "ubuntu-24.04-arm64.img" | cut -d: -f1 | head -n 1)
                mount -t ext4 -o rw,noatime "$LOOP" /mnt/rootfs
                echo "[+] Mounted loop image to /mnt/rootfs"
            fi
        fi
    fi
fi

# Hand over to systemd if init exists
if [ -x /mnt/rootfs/lib/systemd/systemd ] || [ -x /mnt/rootfs/sbin/init ]; then
    echo "[+] Booting Ubuntu 24.04 LTS (systemd)..."
    umount -l /dev/pts 2>/dev/null || true
    umount -l /dev 2>/dev/null || true
    umount -l /sys 2>/dev/null || true
    umount -l /proc 2>/dev/null || true
    exec switch_root /mnt/rootfs /lib/systemd/systemd
fi

# Fallback debug shell
echo "[-] RootFS handover failed. Spawning emergency interactive shell..."
echo "[-] You can test commands: lsblk, blkid, dmesg, insmod, ls /dev/block"
sh
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

echo -e "${GREEN}[OK]${NC} =========================================================================="
echo -e "${GREEN}[OK]${NC} custom_init_boot.img generated successfully!"
echo -e "${GREEN}[OK]${NC} Output: $OUTPUT_IMG"
echo -e "${GREEN}[OK]${NC} Size:   $(ls -lh "$OUTPUT_IMG" | awk '{print $5}')"
echo -e "${GREEN}[OK]${NC} =========================================================================="
