#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build"

if ! command -v cmake >/dev/null 2>&1; then
  echo "cmake is not installed."
  exit 1
fi

CPU_COUNT="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}"
cmake --build "${BUILD_DIR}" -j"${CPU_COUNT}"

if [ "$(id -u)" -eq 0 ]; then
  exec "${BUILD_DIR}/Raspberry_Fun_Control"
fi

exec sudo "${BUILD_DIR}/Raspberry_Fun_Control"

