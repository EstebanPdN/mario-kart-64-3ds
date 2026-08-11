#pragma once

#include <cstdint>

namespace mk64_3ds::install_progress {

// Installation progress is divided by actual work rather than by the number
// of installer stages. The supported extraction produces this many canonical
// O2R entries; older archives may contain two extra aliases and are clamped.
constexpr std::uint32_t kCanonicalEntryCount = 32445;

constexpr int kRomSearch = 1;
constexpr int kRomHashStart = 2;
constexpr int kRomHashEnd = 8;
constexpr int kPreparationStart = 9;
constexpr int kRecoveryValidationEnd = 11;
constexpr int kMetadataCopyStart = 12;
constexpr int kMetadataCopyEnd = 15;
constexpr int kGenerationStart = 16;
constexpr int kGenerationEnd = 88;
constexpr int kValidationStart = 89;
constexpr int kValidationEnd = 99;
constexpr int kComplete = 100;

constexpr int ClampPercent(int percent) {
    return percent < 0 ? 0 : (percent > kComplete ? kComplete : percent);
}

constexpr int AdvanceMonotonic(int currentPercent, int requestedPercent) {
    const int current = ClampPercent(currentPercent);
    const int requested = ClampPercent(requestedPercent);
    return requested > current ? requested : current;
}

// Map completed units into an inclusive progress range. Inputs are 32-bit so
// the widened multiplication cannot overflow; ZIP32 archives cannot expose
// more units than this in the on-device pipeline.
constexpr int MapCompletedWork(std::uint32_t completed, std::uint32_t total,
                               int rangeStart, int rangeEnd) {
    const int start = ClampPercent(rangeStart);
    const int end = ClampPercent(rangeEnd);
    if (end <= start || total == 0) return start;
    const std::uint32_t bounded = completed < total ? completed : total;
    const std::uint64_t span = static_cast<std::uint64_t>(end - start);
    return start + static_cast<int>((static_cast<std::uint64_t>(bounded) * span) / total);
}

constexpr int MapRomHashBytes(std::uint32_t completed, std::uint32_t total) {
    return MapCompletedWork(completed, total, kRomHashStart, kRomHashEnd);
}

constexpr int MapGeneratedEntries(std::uint32_t completed) {
    return MapCompletedWork(completed, kCanonicalEntryCount, kGenerationStart,
                            kGenerationEnd);
}

constexpr int MapValidatedEntries(std::uint32_t completed, std::uint32_t total,
                                  int rangeStart = kValidationStart,
                                  int rangeEnd = kValidationEnd) {
    return MapCompletedWork(completed, total, rangeStart, rangeEnd);
}

constexpr bool CanonicalMappingsAreMonotonic() {
    int previousGeneration = MapGeneratedEntries(0);
    int previousValidation = MapValidatedEntries(0, kCanonicalEntryCount);
    for (std::uint32_t entry = 1; entry <= kCanonicalEntryCount; ++entry) {
        const int generation = MapGeneratedEntries(entry);
        const int validation = MapValidatedEntries(entry, kCanonicalEntryCount);
        if (generation < previousGeneration || validation < previousValidation) return false;
        previousGeneration = generation;
        previousValidation = validation;
    }
    return previousGeneration == kGenerationEnd && previousValidation == kValidationEnd;
}

static_assert(kRomHashEnd == 8, "ROM verification must occupy only the first eight percent");
static_assert(AdvanceMonotonic(43, 16) == 43);
static_assert(AdvanceMonotonic(88, 89) == 89);
static_assert(MapRomHashBytes(0, 12 * 1024 * 1024) == kRomHashStart);
static_assert(MapRomHashBytes(12 * 1024 * 1024, 12 * 1024 * 1024) == kRomHashEnd);
static_assert(MapGeneratedEntries(0) == kGenerationStart);
static_assert(MapGeneratedEntries(kCanonicalEntryCount) == kGenerationEnd);
static_assert(MapGeneratedEntries(kCanonicalEntryCount + 2) == kGenerationEnd);
static_assert(MapValidatedEntries(0, kCanonicalEntryCount) == kValidationStart);
static_assert(MapValidatedEntries(kCanonicalEntryCount, kCanonicalEntryCount) == kValidationEnd);
static_assert(CanonicalMappingsAreMonotonic(), "installation progress mappings must never regress");

} // namespace mk64_3ds::install_progress
