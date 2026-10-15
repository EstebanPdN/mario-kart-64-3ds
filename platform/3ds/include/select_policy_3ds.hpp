#pragma once

#include "settings_3ds.h"
#include <cstdint>

namespace mk64_3ds {
enum class SelectEvent3DS { None, Camera, Dump, Overlay, DumpChord };

// HID's digital key bits. Keeping this policy independent of libctru lets the
// same production event routing be exercised by host regression tests.
struct SelectPolicy3DS {
    static constexpr uint32_t kSelect = 1U << 2;
    static constexpr uint32_t kR = 1U << 8;
    static constexpr uint32_t kL = 1U << 9;
    static constexpr uint32_t kChord = kL | kR | kSelect;
    bool chordHeld = false;

    SelectEvent3DS Update(uint32_t held, uint32_t pressed, bool diagnosticRace,
                          Mk64SelectAction3DS binding) {
        const bool chord = (held & kChord) == kChord;
        if (!chord) chordHeld = false;
        if (chord) {
            if (!chordHeld && (pressed & kChord)) {
                chordHeld = true;
                return SelectEvent3DS::DumpChord;
            }
            return SelectEvent3DS::None;
        }
        if (!diagnosticRace || !(pressed & kSelect)) return SelectEvent3DS::None;
        return binding == MK64_SELECT_ACTION_3DS_DUMP ? SelectEvent3DS::Dump
            : binding == MK64_SELECT_ACTION_3DS_OVERLAY ? SelectEvent3DS::Overlay
            : SelectEvent3DS::Camera;
    }
};
}
