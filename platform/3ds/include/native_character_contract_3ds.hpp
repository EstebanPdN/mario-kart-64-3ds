#pragma once
#include "native_kart_lab_3ds.h"
namespace mk64_3ds {
// MK64 migration semantics, separate from the reusable native GPU material.
constexpr bool CharacterIsShadow(Mk64NativeCharacterKind kind) {
    return kind==MK64_NATIVE_GROUND_SHADOW || kind==MK64_NATIVE_CREDITS_SHADOW;
}
constexpr bool CharacterWritesPalette(Mk64NativeCharacterKind kind) {
    return kind==MK64_NATIVE_BODY || kind==MK64_NATIVE_GHOST;
}
constexpr unsigned CharacterImageHeight(Mk64NativeCharacterKind kind) {
    return kind==MK64_NATIVE_CREDITS_SHADOW?32:64;
}
}
