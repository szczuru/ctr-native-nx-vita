#!/usr/bin/env bash
# build_vita.sh
# Build CTR Native for PlayStation Vita (vitasdk).
#
# Usage:
#   ./vita/build_vita.sh [clean]
#
# Requires: vitasdk (https://vitasdk.org/), VitaGL in vitasdk sysroot.

set -e

VITASDK="${VITASDK:-/usr/local/vitasdk}"
BUILD_DIR="build-vita"
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

echo "=== CTR Native – PlayStation Vita Build ==="
echo "VITASDK   : ${VITASDK}"
echo "Repo root : ${REPO_ROOT}"

if [ ! -f "${VITASDK}/share/vita.toolchain.cmake" ]; then
    echo "ERROR: vitasdk toolchain not found at ${VITASDK}/share/vita.toolchain.cmake"
    echo "Install vitasdk from https://vitasdk.org/"
    exit 1
fi

export VITASDK
export PATH="${VITASDK}/bin:${PATH}"

cd "${REPO_ROOT}"

if [ "${1}" = "clean" ]; then
    echo "Cleaning ${BUILD_DIR}..."
    rm -rf "${BUILD_DIR}"
fi

mkdir -p "${BUILD_DIR}"

cmake -B "${BUILD_DIR}" \
    -DCMAKE_TOOLCHAIN_FILE="${VITASDK}/share/vita.toolchain.cmake" \
    -DPLATFORM=vita \
    -DCMAKE_BUILD_TYPE=Release \
    -DCTR_NATIVE_VERSION="$(cat VERSION 2>/dev/null || echo 'dev')"

cmake --build "${BUILD_DIR}" --parallel "$(nproc)"

echo ""
echo "=== Build complete ==="
echo "VPK: ${BUILD_DIR}/ctr_native.vpk"
echo ""
echo "Install via VitaShell (FTP or USB):"
echo "  Transfer VPK to the Vita and install it."
echo ""
echo "Place game assets:"
echo "  ux0:/data/ctr_native/assets/ctr-u.bin"
echo "  (or extracted: ux0:/data/ctr_native/assets/BIGFILE.BIG, SOUNDS/, XA/)"
