#pragma once
#include <stdint.h>

// Both lists retain their head and final free-list sentinel. u8 indices remain valid.
#define MK64_POSITIONAL_SOUND_SLOTS_3DS 82
#define MK64_SOUND_BANK_SLOTS_3DS 32

// Keep the original ID, playback flags and stop/dedup identity unchanged.
static inline uint32_t Mk64FeedbackPriority3DS(uint32_t soundBits) {
    if (soundBits == 0x19008406U) return 0xE400U; // Item-box pickup.
    if (soundBits == 0x19018010U) return 0xE800U; // Shell/item impact.
    return soundBits & 0xFF00U;
}
