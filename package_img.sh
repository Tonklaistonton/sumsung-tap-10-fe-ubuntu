#!/usr/bin/env bash
# ==============================================================================
# Package Ubuntu 24.04 ARM64 RootFS into Flashable ext4 Disk Image
# ==============================================================================

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

if [ "$(id -u)" -ne 0 ]; then
    echo -e "${RED}[ERROR]${NC} Please run with sudo: sudo ./package_img.sh"
    exit 1
fi

BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOTFS_DIR="$BASE_DIR/ubuntu-noble-rootfs/rootfs"
IMAGE_OUT="$BASE_DIR/ubuntu-noble-rootfs/ubuntu-24.04-arm64.img"

echo -e "${BLUE}[INFO]${NC} 1. Safely unmounting pseudofs (/proc, /sys, /dev)..."
umount -l "$ROOTFS_DIR/dev/pts" 2>/dev/null || true
umount -l "$ROOTFS_DIR/dev" 2>/dev/null || true
umount -l "$ROOTFS_DIR/sys" 2>/dev/null || true
umount -l "$ROOTFS_DIR/proc" 2>/dev/null || true

rm -f "$ROOTFS_DIR/usr/sbin/policy-rc.d"
rm -f "$ROOTFS_DIR/usr/bin/qemu-aarch64-static"
rm -f "$ROOTFS_DIR/etc/resolv.conf"
cat << 'EOF' > "$ROOTFS_DIR/etc/resolv.conf"
nameserver 8.8.8.8
nameserver 1.1.1.1
EOF

echo -e "${BLUE}[INFO]${NC} 2. Creating 12GB sparse image: $IMAGE_OUT..."
rm -f "$IMAGE_OUT"
truncate -s 12G "$IMAGE_OUT"

echo -e "${BLUE}[INFO]${NC} 3. Building ext4 filesystem from clean rootfs..."
mkfs.ext4 -F -L "ubuntu-rootfs" -d "$ROOTFS_DIR" "$IMAGE_OUT"

echo -e "${GREEN}[OK]${NC} =========================================================================="
echo -e "${GREEN}[OK]${NC} Ubuntu 24.04 ARM64 ext4 image packaged successfully!"
echo -e "${GREEN}[OK]${NC} Path: $IMAGE_OUT"
echo -e "${GREEN}[OK]${NC} Size: $(ls -lh "$IMAGE_OUT" | awk '{print $5}')"
echo -e "${GREEN}[OK]${NC} =========================================================================="

