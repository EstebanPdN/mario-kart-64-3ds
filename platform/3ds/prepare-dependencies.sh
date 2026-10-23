#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

apply_compatibility_patch() {
  local checkout="$1" patch="$2"
  if git -C "$checkout" apply --check "$patch" >/dev/null 2>&1; then
    git -C "$checkout" apply "$patch"
  elif git -C "$checkout" apply --reverse --check "$patch" >/dev/null 2>&1; then
    :
  else
    printf 'Cannot apply or verify compatibility patch: %s\n' "$patch" >&2
    return 1
  fi
}

apply_compatibility_patch "${ROOT}/third_party/SpaghettiKart/libultraship" \
  "${ROOT}/platform/3ds/patches/libultraship-e6.patch"
apply_compatibility_patch "${ROOT}/third_party/SpaghettiKart/torch" \
  "${ROOT}/platform/3ds/patches/torch-3ds-devkitarm.patch"
