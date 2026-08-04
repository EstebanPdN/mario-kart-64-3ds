#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
VERSION="$(tr -d '\r\n' < "${ROOT}/platform/3ds/version.txt")"
DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
BUILD="${ROOT}/build-3ds/game"
TOOLS_ROOT="${MK64_3DS_TOOLS_ROOT:-${ROOT}/../Tools/bin}"
MAKEROM="${MAKEROM:-${TOOLS_ROOT}/makerom}"
BANNERTOOL="${BANNERTOOL:-${TOOLS_ROOT}/bannertool}"

if [[ ! -x "${MAKEROM}" ]] && command -v makerom >/dev/null 2>&1; then
  MAKEROM="$(command -v makerom)"
fi
if [[ ! -x "${BANNERTOOL}" ]] && command -v bannertool >/dev/null 2>&1; then
  BANNERTOOL="$(command -v bannertool)"
fi
if [[ ! -x "${MAKEROM}" && -x "${DEVKITPRO}/tools/bin/makerom" ]]; then
  MAKEROM="${DEVKITPRO}/tools/bin/makerom"
fi
if [[ ! -x "${BANNERTOOL}" && -x "${DEVKITPRO}/tools/bin/bannertool" ]]; then
  BANNERTOOL="${DEVKITPRO}/tools/bin/bannertool"
fi

cmake -S "${ROOT}" -B "${BUILD}" \
  -DCMAKE_TOOLCHAIN_FILE="${DEVKITPRO}/cmake/3DS.cmake" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD}" --target mk64-3ds-3dsx --parallel "${MK64_3DS_JOBS:-4}"

if [[ ! -x "${MAKEROM}" || ! -x "${BANNERTOOL}" ]]; then
  printf '3DSX ready; makerom/bannertool are unavailable for CIA packaging.\n'
  exit 0
fi

"${BANNERTOOL}" makebanner \
  -i "${ROOT}/platform/3ds/assets/banner.png" \
  -a "${ROOT}/platform/3ds/assets/banner.wav" \
  -o "${BUILD}/mk64-3ds.bnr"

(
  cd "${ROOT}"
  "${MAKEROM}" -f cia -o "${BUILD}/mk64-3ds-v${VERSION}.cia" \
    -rsf "${ROOT}/platform/3ds/cia/mk64-3ds.rsf" -target t -exefslogo \
    -elf "${BUILD}/mk64-3ds.elf" -icon "${BUILD}/mk64-3ds.smdh" \
    -banner "${BUILD}/mk64-3ds.bnr"
)

printf 'Ready:\n  %s\n  %s\n' \
  "${BUILD}/mk64-3ds-v${VERSION}.3dsx" "${BUILD}/mk64-3ds-v${VERSION}.cia"
