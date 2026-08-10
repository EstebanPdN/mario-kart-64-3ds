#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
VERSION="$(tr -d '\r\n' < "${ROOT}/platform/3ds/version.txt")"
DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
BUILD="${ROOT}/build-3ds/game"
TOOLS_ROOT="${MK64_3DS_TOOLS_ROOT:-${ROOT}/../Tools/bin}"
MAKEROM="${MAKEROM:-${TOOLS_ROOT}/makerom}"
BANNERTOOL="${BANNERTOOL:-${TOOLS_ROOT}/bannertool}"
TORCH_DIR="${ROOT}/third_party/SpaghettiKart/torch"
TORCH_PATCH="${ROOT}/platform/3ds/patches/torch-3ds-devkitarm.patch"

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

if git -C "${TORCH_DIR}" apply --check "${TORCH_PATCH}" >/dev/null 2>&1; then
  git -C "${TORCH_DIR}" apply "${TORCH_PATCH}"
elif git -C "${TORCH_DIR}" apply --reverse --check "${TORCH_PATCH}" >/dev/null 2>&1; then
  :
else
  printf 'Could not apply or verify the 3DS Torch compatibility patch.\n' >&2
  exit 1
fi

cmake -S "${ROOT}" -B "${BUILD}" \
  -DCMAKE_TOOLCHAIN_FILE="${DEVKITPRO}/cmake/3DS.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DMK64_3DS_BUILD_GAME_CORE=ON \
  -DMK64_3DS_BUILD_GAME_ENGINE=ON \
  -DMK64_3DS_ENABLE_O2R_READER=ON \
  -DMK64_3DS_ENABLE_FAST3D_RENDERER=ON \
  -DMK64_3DS_BUILD_RENDERER_PROBE=OFF \
  -DMK64_3DS_BUILD_INTERPRETER_PROBE=ON
cmake --build "${BUILD}" --target mk64-3ds-game-3dsx --parallel "${MK64_3DS_JOBS:-4}"

if [[ ! -x "${MAKEROM}" || ! -x "${BANNERTOOL}" ]]; then
  printf '3DSX ready; makerom/bannertool are unavailable for CIA packaging.\n'
  exit 0
fi

CGFX_BANNER="${ROOT}/platform/3ds/assets/banner.cgfx"
CGFX_SIZE="$(wc -c < "${CGFX_BANNER}")"
if (( CGFX_SIZE > 0x80000 )); then
  printf '3D banner CGFX exceeds the 512 KiB HOME Menu limit: %s bytes\n' "${CGFX_SIZE}" >&2
  exit 1
fi

"${BANNERTOOL}" makebanner \
  -ci "${CGFX_BANNER}" \
  -a "${ROOT}/platform/3ds/assets/banner.wav" \
  -o "${BUILD}/mk64-3ds.bnr"

(
  cd "${ROOT}"
  "${MAKEROM}" -f cia -o "${BUILD}/mk64-3ds-v${VERSION}.cia" \
    -DAPP_ROMFS="${BUILD#"${ROOT}/"}/romfs" \
    -rsf "${ROOT}/platform/3ds/cia/mk64-3ds.rsf" -target t -exefslogo \
    -elf "${BUILD}/mk64-3ds-game.elf" -icon "${BUILD}/mk64-3ds.smdh" \
    -banner "${BUILD}/mk64-3ds.bnr"
)

printf 'Ready:\n  %s\n  %s\n' \
  "${BUILD}/mk64-3ds-game-v${VERSION}.3dsx" "${BUILD}/mk64-3ds-v${VERSION}.cia"
