#pragma once
#include <array>
#include <cstdint>

namespace mk64_3ds {
// Game time excludes diagnostic writes and suspend. A rejected request is
// retried; a capture is counted only after its complete-write acknowledgement.
struct AutoDumpSchedule {
    static constexpr unsigned Races = 6, Captures = 3, EarlyRaces = 2,
        IntervalMs = 15000, PostCaptureMs = 5000;
    std::array<unsigned, 16> courses{};
    unsigned race = 0, captured = 0, total = 0;
    bool pending = false, failed = false;
    void Shuffle(std::uint32_t seed) {
        for (unsigned i = 0; i < courses.size(); ++i) courses[i] = i;
        if (!seed) seed = 0x9e3779b9U;
        for (unsigned i = courses.size() - 1; i > 0; --i) {
            seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
            const unsigned j = seed % (i + 1), old = courses[i];
            courses[i] = courses[j]; courses[j] = old;
        }
    }
    bool HasStartCapture() const { return race < EarlyRaces; }
    unsigned TargetMs() const { return (captured + (HasStartCapture() ? 0 : 1)) * IntervalMs; }
    bool Due(std::uint32_t gameMs) const {
        return !failed && !pending && race < Races && captured < Captures &&
            gameMs >= TargetMs();
    }
    bool Complete(bool success) {
        if (!pending) return false;
        pending = false;
        if (!success) { failed = true; return false; }
        ++captured; ++total; return true;
    }
    bool NextRace() {
        if (failed || pending || captured != Captures || race >= Races) return false;
        ++race; if (race < Races) captured = 0; return true;
    }
    bool Finished() const { return !failed && race == Races && total == Races * Captures; }
};
}
