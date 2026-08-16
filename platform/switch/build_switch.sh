#!/usr/bin/env bash
# build_switch.sh
# Build CTR Native for Nintendo Switch (devkitPro / libnx).
#
# Usage:
#   ./switch/build_switch.sh [clean]
#
# Requires devkitPro with devkitA64, libnx, switch-tools, switch-sdl3.
# Set DEVKITPRO if it differs from /opt/devkitpro.

set -e

DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
DEVKITA64="${DEVKITPRO}/devkitA64"
BUILD_DIR="build-switch"
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

echo "=== CTR Native – Nintendo Switch Build ==="
echo "DEVKITPRO : ${DEVKITPRO}"
echo "Repo root : ${REPO_ROOT}"

if [ ! -d "${DEVKITA64}" ]; then
    echo "ERROR: devkitA64 not found at ${DEVKITA64}"
    echo "Install devkitPro: https://devkitpro.org/wiki/Getting_Started"
    exit 1
fi

export DEVKITPRO
export DEVKITA64
export PATH="${DEVKITA64}/bin:${DEVKITPRO}/tools/bin:${PATH}"

cd "${REPO_ROOT}"

# Clean if requested
if [ "${1}" = "clean" ]; then
    echo "Cleaning ${BUILD_DIR}..."
    rm -rf "${BUILD_DIR}"
fi

mkdir -p "${BUILD_DIR}"

cmake -B "${BUILD_DIR}" \
    -DCMAKE_TOOLCHAIN_FILE="${REPO_ROOT}/switch/switch-toolchain.cmake" \
    -DPLATFORM=switch \
    -DCMAKE_BUILD_TYPE=Release \
    -DCTR_NATIVE_VERSION="$(cat VERSION 2>/dev/null || echo 'dev')"

cmake --build "${BUILD_DIR}" --parallel "$(nproc)"

echo ""
echo "=== Build complete ==="
echo "NRO: ${BUILD_DIR}/ctr_native.nro"
echo ""
echo "Copy to SD card:"
echo "  /switch/ctr_native/ctr_native.nro"
echo ""
echo "Place game assets on SD card:"
echo "  /ctr_native/assets/ctr-u.bin"
echo "  (or extracted: /ctr_native/assets/BIGFILE.BIG, SOUNDS/, XA/)"
