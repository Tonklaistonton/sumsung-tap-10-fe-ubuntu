#!/usr/bin/env bash
# ==============================================================================
# Build Ubuntu 24.04 LTS (Noble Numbat) ARM64 RootFS for Samsung Galaxy Tab S10 FE
# Target Device: SM-X520 (Wi-Fi), Exynos 1580 (mx8855)
# Kernel Version: 6.6.102-android15-8-abX520XXSBCZI2-4k
# ==============================================================================

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $*"; }
log_ok() { echo -e "${GREEN}[OK]${NC} $*"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }
log_err() { echo -e "${RED}[ERROR]${NC} $*"; }

if [ "$(id -u)" -ne 0 ]; then
    log_err "Script must be run as root (use sudo ./build_ubuntu_arm64_rootfs.sh)"
    exit 1
fi

BASE_DIR="/home/tonton/rom/samsung"
BUILDER_DIR="$BASE_DIR/rootfs_builder"
ROOTFS_DIR="$BASE_DIR/ubuntu-noble-rootfs/rootfs"
DIST_DIR="$BASE_DIR/Kernel/out/mx8855/dist"
FW_SRC="$BASE_DIR/Kernel/prebuilts/platform/erd8855/firmware"
TARBALL="$BUILDER_DIR/ubuntu-base-24.04.5-base-arm64.tar.gz"
IMAGE_OUT="$BASE_DIR/ubuntu-noble-rootfs/ubuntu-24.04-arm64.img"
KERNEL_VER="6.6.102-android15-8-4k"

# 1. Check prerequisites
log_info "1/8 Checking host prerequisites..."
if ! command -v qemu-aarch64-static >/dev/null 2>&1 && [ ! -f /usr/bin/qemu-aarch64-static ]; then
    log_warn "qemu-aarch64-static not found in PATH. Checking /proc/sys/fs/binfmt_misc/..."
fi

if [ ! -f "$TARBALL" ]; then
    log_info "Downloading Ubuntu Base 24.04 ARM64 tarball..."
    mkdir -p "$BUILDER_DIR"
    curl -L -o "$TARBALL" "http://cdimage.ubuntu.com/ubuntu-base/releases/24.04/release/ubuntu-base-24.04.5-base-arm64.tar.gz"
fi
log_ok "Base tarball ready: $(ls -lh "$TARBALL" | awk '{print $5}')"

# 2. Extract base rootfs
log_info "2/8 Extracting Ubuntu Base to $ROOTFS_DIR..."
rm -rf "$ROOTFS_DIR"
mkdir -p "$ROOTFS_DIR"
tar -xzf "$TARBALL" -C "$ROOTFS_DIR"
log_ok "Base rootfs extracted successfully."

# 3. Setup DNS & Apt Sources
log_info "3/8 Setting up network, DNS, and Ubuntu Noble repositories..."
cp /etc/resolv.conf "$ROOTFS_DIR/etc/resolv.conf"

cat << 'EOF' > "$ROOTFS_DIR/etc/apt/sources.list"
deb http://ports.ubuntu.com/ubuntu-ports noble main restricted universe multiverse
deb http://ports.ubuntu.com/ubuntu-ports noble-updates main restricted universe multiverse
deb http://ports.ubuntu.com/ubuntu-ports noble-security main restricted universe multiverse
deb http://ports.ubuntu.com/ubuntu-ports noble-backports main restricted universe multiverse
EOF

# Ensure daemons are not started during chroot installation
cat << 'EOF' > "$ROOTFS_DIR/usr/sbin/policy-rc.d"
#!/bin/sh
exit 101
EOF
chmod +x "$ROOTFS_DIR/usr/sbin/policy-rc.d"

# Copy static qemu emulator into rootfs if exists on host
if [ -f /usr/bin/qemu-aarch64-static ]; then
    cp /usr/bin/qemu-aarch64-static "$ROOTFS_DIR/usr/bin/"
elif [ -f /usr/bin/qemu-aarch64 ]; then
    cp /usr/bin/qemu-aarch64 "$ROOTFS_DIR/usr/bin/qemu-aarch64-static"
elif [ -f "$BUILDER_DIR/qemu_extracted/usr/bin/qemu-aarch64" ]; then
    cp "$BUILDER_DIR/qemu_extracted/usr/bin/qemu-aarch64" "$ROOTFS_DIR/usr/bin/qemu-aarch64-static"
fi

# 4. Bind mount system pseudofs and chroot
log_info "4/8 Mounting pseudofs and executing chroot package installations..."
mount -t proc /proc "$ROOTFS_DIR/proc"
mount -t sysfs /sys "$ROOTFS_DIR/sys"
mount --bind /dev "$ROOTFS_DIR/dev"
mount --bind /dev/pts "$ROOTFS_DIR/dev/pts"

cleanup() {
    log_info "Unmounting pseudofs..."
    umount -l "$ROOTFS_DIR/dev/pts" 2>/dev/null || true
    umount -l "$ROOTFS_DIR/dev" 2>/dev/null || true
    umount -l "$ROOTFS_DIR/sys" 2>/dev/null || true
    umount -l "$ROOTFS_DIR/proc" 2>/dev/null || true
    rm -f "$ROOTFS_DIR/usr/sbin/policy-rc.d"
    rm -f "$ROOTFS_DIR/usr/bin/qemu-aarch64-static"
}
trap cleanup EXIT

chroot "$ROOTFS_DIR" /bin/bash << 'CHROOT_SCRIPT'
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
export LANG=C.UTF-8

echo "=== Updating package lists ==="
apt-get update

echo "=== Installing core utilities & desktop ==="
apt-get install -y --no-install-recommends \
    ubuntu-desktop-minimal \
    network-manager \
    wpasupplicant \
    gdm3 \
    wayland-protocols \
    pipewire \
    wireplumber \
    alsa-utils \
    systemd \
    systemd-sysv \
    udev \
    kmod \
    sudo \
    curl \
    wget \
    nano \
    htop \
    openssh-server \
    locales \
    tzdata \
    ca-certificates

echo "=== Setting hostname and hosts ==="
echo "s10fe-ubuntu" > /etc/hostname
cat << 'EOF' > /etc/hosts
127.0.0.1   localhost
127.0.1.1   s10fe-ubuntu
::1         localhost ip6-localhost ip6-loopback
EOF

echo "=== Configuring user accounts ==="
if ! id tonton >/dev/null 2>&1; then
    useradd -m -s /bin/bash -G sudo,adm,audio,video,plugdev,input tonton
fi
echo "tonton:tonton" | chpasswd
echo "root:root" | chpasswd

echo "=== Configuring locales ==="
echo "en_US.UTF-8 UTF-8" >> /etc/locale.gen
echo "th_TH.UTF-8 UTF-8" >> /etc/locale.gen
locale-gen
update-locale LANG=en_US.UTF-8

echo "=== Enabling systemd services ==="
systemctl enable NetworkManager.service || true
systemctl enable gdm3.service || true
systemctl enable ssh.service || true

apt-get clean
rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
echo "=== Chroot provisioning completed successfully ==="
CHROOT_SCRIPT

log_ok "Chroot package installation complete."

# Immediately unmount pseudofs before file operations and packaging
log_info "Unmounting pseudofs (/proc, /sys, /dev)..."
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

# 5. Copy Kernel Modules (286 .ko modules)
log_info "5/8 Installing 286 compiled Linux kernel modules..."
MOD_DEST="$ROOTFS_DIR/lib/modules/$KERNEL_VER"
mkdir -p "$MOD_DEST"

KO_COUNT=0
for ko in "$DIST_DIR"/*.ko; do
    [ -f "$ko" ] || continue
    cp "$ko" "$MOD_DEST/"
    KO_COUNT=$((KO_COUNT + 1))
done

log_ok "Copied $KO_COUNT kernel modules into $MOD_DEST."

# Run depmod
log_info "Generating modules.dep and dependency map..."
depmod -a -b "$ROOTFS_DIR" "$KERNEL_VER"
log_ok "depmod completed."

# Configure module autoloading
cat << 'EOF' > "$ROOTFS_DIR/etc/modules-load.d/s10fe.conf"
# Samsung Galaxy Tab S10 FE (SM-X520) Essential Kernel Drivers
ufs-exynos-core
exynos-drm
mcd-panel-nt36523n_gts10fe_csot
novatek_ts_nt36523
wez01
sgpu
scsc_mx
scsc_wlan
EOF
log_ok "Configured /etc/modules-load.d/s10fe.conf."

# 6. Copy Hardware Firmware Blobs
log_info "6/8 Installing hardware firmware binaries..."
mkdir -p "$ROOTFS_DIR/lib/firmware/sgpu"
mkdir -p "$ROOTFS_DIR/lib/firmware/sensorhub"
mkdir -p "$ROOTFS_DIR/lib/firmware/npu"

if [ -f "$FW_SRC/sgpu/mgfx2.0_mid.bin" ]; then
    cp "$FW_SRC/sgpu/mgfx2.0_mid.bin" "$ROOTFS_DIR/lib/firmware/sgpu/"
    cp "$FW_SRC/sgpu/mgfx2.0_mid.bin" "$ROOTFS_DIR/lib/firmware/"
    log_ok "Installed GPU firmware (mgfx2.0_mid.bin)."
fi

if [ -f "$FW_SRC/sensorhub/shub_santa_gts10fe.bin" ]; then
    cp "$FW_SRC/sensorhub/shub_santa_gts10fe.bin" "$ROOTFS_DIR/lib/firmware/sensorhub/"
    log_ok "Installed SensorHub firmware (shub_santa_gts10fe.bin)."
fi

if [ -f "$FW_SRC/mfc_fw_v17.x.bin" ]; then
    cp "$FW_SRC/mfc_fw_v17.x.bin" "$ROOTFS_DIR/lib/firmware/"
    log_ok "Installed Video Codec firmware (mfc_fw_v17.x.bin)."
fi

if [ -f "$FW_SRC/npu/AIE.bin" ]; then
    cp "$FW_SRC/npu/AIE.bin" "$ROOTFS_DIR/lib/firmware/npu/"
    log_ok "Installed NPU firmware (AIE.bin)."
fi

# 7. Copy Audio Configuration
log_info "7/8 Installing audio XML configurations..."
mkdir -p "$ROOTFS_DIR/etc/s10fe/audio"
if [ -d "$BASE_DIR/s10fe-research/audio" ]; then
    cp "$BASE_DIR/s10fe-research/audio"/*.xml "$ROOTFS_DIR/etc/s10fe/audio/" || true
    log_ok "Audio configurations installed to /etc/s10fe/audio/."
fi

# 8. Create flashable ext4 rootfs disk image
log_info "8/8 Building flashable ext4 disk image: $IMAGE_OUT (12 GB sparse)..."
truncate -s 12G "$IMAGE_OUT"
mkfs.ext4 -F -L "ubuntu-rootfs" -d "$ROOTFS_DIR" "$IMAGE_OUT"
log_ok "Filesystem image created: $(ls -lh "$IMAGE_OUT" | awk '{print $5}')"

log_ok "=========================================================================="
log_ok "Ubuntu 24.04 ARM64 RootFS build completed successfully!"
log_ok "RootFS Directory: $ROOTFS_DIR"
log_ok "Flashable Image:  $IMAGE_OUT"
log_ok "Default User:     tonton (password: tonton)"
log_ok "Root User:        root   (password: root)"
log_ok "=========================================================================="

