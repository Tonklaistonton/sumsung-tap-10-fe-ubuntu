#!/usr/bin/env bash
# ==============================================================================
# Top-level build for Ubuntu on Samsung Galaxy Tab S10 FE (SM-X520)
#
# Stages:
#   check      - verify pinned inputs (manifest.json hashes)
#   kernel     - build kernel (Bazel/Kleaf) -> Kernel/out/mx8855/dist/
#   initboot   - build initramfs + custom_init_boot.img
#   rootfs     - build Ubuntu 24.04 ARM64 rootfs image (needs root)
#   release    - pack boot chain + SHA256SUMS into release/
#
# Usage:
#   ./build.sh check [kernel initboot rootfs release]
#   ./build.sh all
# ==============================================================================

set -euo pipefail

RED='\033[0;31m'; GREEN='\033[0;32m'; BLUE='\033[0;34m'; NC='\033[0m'
BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DIST_DIR="$BASE_DIR/Kernel/out/mx8855/dist"

stage_check() {
    echo -e "${BLUE}[CHECK]${NC} Verifying pinned inputs..."
    python3 "$BASE_DIR/tools/verify_manifest.py" "$BASE_DIR/manifest.json" || {
        echo -e "${RED}[FAIL]${NC} input mismatch. Re-run tools/make-manifest.sh only if you changed inputs on purpose."; exit 1; }
    [ -f "$DIST_DIR/Image" ] && echo -e "${GREEN}[OK]${NC} kernel dist present (Image $(du -h "$DIST_DIR/Image" | cut -f1))" \
        || echo -e "${BLUE}[INFO]${NC} no kernel dist yet — stage 'kernel' required"
}

stage_kernel() {
    echo -e "${BLUE}[KERNEL]${NC} Building kernel (Bazel/Kleaf, ~30-60 min)..."
    (
        cd "$BASE_DIR/Kernel"
        # Toolchain: AOSP common-android15-6.6 (build/, external/, prebuilts/,
        # common-modules/ must be symlinked here, see docs/BUILD.md).
        tools/bazel run --config=stamp --config=mx8855_user //exynos/device/mx/mx8855:mx8855_dist
    )
    [ -f "$DIST_DIR/Image" ] || { echo -e "${RED}[FAIL]${NC} $DIST_DIR/Image not produced"; exit 1; }
    echo -e "${GREEN}[OK]${NC} kernel build complete: $DIST_DIR"
}

stage_initboot() {
    echo -e "${BLUE}[INITBOOT]${NC} Building initramfs + custom_init_boot.img..."
    "$BASE_DIR/build_custom_init_boot.sh"
}

stage_rootfs() {
    [ "$(id -u)" -eq 0 ] || { echo -e "${RED}[FAIL]${NC} rootfs stage needs sudo"; exit 1; }
    echo -e "${BLUE}[ROOTFS]${NC} Building Ubuntu 24.04 ARM64 rootfs (needs root, ~10 min)..."
    "$BASE_DIR/build_ubuntu_arm64_rootfs.sh"
}

stage_release() {
    echo -e "${BLUE}[RELEASE]${NC} Packing release set..."
    "$BASE_DIR/pack_boot_images.sh"
}

run_stage() {
    case "$1" in
        check)    stage_check ;;
        kernel)   stage_kernel ;;
        initboot) stage_initboot ;;
        rootfs)   stage_rootfs ;;
        release)  stage_release ;;
        *) echo "unknown stage: $1"; exit 2 ;;
    esac
}

if [ $# -eq 0 ] || [ "$1" = "help" ]; then
    sed -n '2,18p' "$0"; exit 0
fi

if [ "$1" = "all" ]; then
    set -- check kernel initboot rootfs release
fi

for s in "$@"; do run_stage "$s"; done
echo -e "${GREEN}[OK]${NC} done."
